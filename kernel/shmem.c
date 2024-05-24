#include "shmem.h"
#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "vm.h"
#include "shmem_structs.h"

struct shared_memory_object shared_memory_objects_global[GLOBAL_NUMBER_OF_SHM_OBJ];

static int initialized = 0;
int round_up_division(int x, int y){
    int whole_part = x/y;
    if (x%y)
        whole_part ++;
    return whole_part;
}

int
strcmp(const char *p, const char *q)
{
	while(*p && *p == *q)
		p++, q++;
	return (uchar)*p - (uchar)*q;
}
char*

strcpy(char *s, const char *t)
{
	char *os;

	os = s;
	while((*s++ = *t++) != 0)
		;
	return os;
}

void init_shared_mem_objects(void){
    for (int i = 0; i < GLOBAL_NUMBER_OF_SHM_OBJ; i ++){
        initlock(&shared_memory_objects_global[i].lock, "shm_object_lock");
        shared_memory_objects_global[i].ref_count = 0;
        shared_memory_objects_global[i].allocated_pages = -1;
        shared_memory_objects_global[i].size = 0;
        shared_memory_objects_global[i].id = i;
        memset(shared_memory_objects_global[i].name, 0, NAME_SZ);
    }
    initialized = 1;
}


int fetch_shared_memory_object(char* name, int* exist){
    int first_free = -1;
    for (int i = 0; i < GLOBAL_NUMBER_OF_SHM_OBJ; i++){
        acquire(&shared_memory_objects_global[i].lock);
        if (strcmp(name, shared_memory_objects_global[i].name) == 0){
            *exist = 1;
            release(&shared_memory_objects_global[i].lock);
            return i;
        }
        if (first_free < 0){
            if (shared_memory_objects_global[i].name[0] == 0){
                first_free = i;
            }
        }
        release(&shared_memory_objects_global[i].lock);
    }
    if (first_free > -1){
        return first_free;
    }

    return -1; // no free object available
}

int check_if_exists(char* name, struct proc* current_proc){
    for (int i = 0; i < 16; i++) {
        if (current_proc->shared_mem_objects[i].shared_mem_object != 0){
            struct shared_memory_object* shm_obj = current_proc->shared_mem_objects[i].shared_mem_object;
            acquire(&shm_obj->lock);
            if (strcmp(shm_obj->name, name) == 0){
                release(&shm_obj->lock);
                return i;
            }
            release(&shm_obj->lock);
        }
    }
    return -1;
}

int find_free_slot_local(struct proc* current_proc){
    for (int i = 0; i < 16; i ++){
        if (current_proc->shared_mem_objects[i].shared_mem_object == 0){
            return i;
        }
    }
    return -1; // no free slot found
}

int edit_shm_obj(int arr_index, char* name, int exist){
    acquire(&shared_memory_objects_global[arr_index].lock);
    if (exist == 0)
        strcpy(shared_memory_objects_global[arr_index].name, name);
    shared_memory_objects_global[arr_index].ref_count ++;
    release(&shared_memory_objects_global[arr_index].lock);
    return 0;
}

void unmap(pde_t* pgdir, uint from, uint to){
   // cprintf("Unmap got called \n");
    pte_t* page_table_entry;
    uint adress = PGROUNDDOWN(from);
    for (; adress < to; adress += PGSIZE){
        
        page_table_entry =  walkpgdir(pgdir, (char*)adress, 0);
        if (*page_table_entry & PTE_P){
            //cprintf("UNMAPED ONE PAGE\n");
            // *page_table_entry &= ~PTE_P;
            // *page_table_entry &= ~PTE_U;
            *page_table_entry = 0;
        }
    }
}

void clean_shm_mem1(struct shared_memory_object* shm_obj){
    for (int i = 0; i <= shm_obj->allocated_pages; i ++){
        kfree(shm_obj->memory[i]);
    }
    shm_obj->allocated_pages = -1;
}
void clean_shared_mem_obj(struct shared_memory_object* shm_obj){
    shm_obj->ref_count = shm_obj->size = 0;
    memset(shm_obj->name, 0, NAME_SZ);
}
void clean_local_shared_mem_obj(struct shared_memory_object_local* shm_obj_local, int object_descriptor){
    struct shared_memory_object** shared_mem_obj = &myproc()->shared_mem_objects[object_descriptor].shared_mem_object;
    shm_obj_local->virtual_adress = shm_obj_local->flags = 0;
    shm_obj_local->shared_mem_object = 0;
    (*shared_mem_obj) = 0;
}
int shm_open(char* name){
    struct proc* current_proc = myproc();
    int exist = check_if_exists(name, current_proc);
    if (exist < 0){
        int free_slot_local = find_free_slot_local(current_proc);
        int glob_exist_flag = 0;
        int global_object_index = fetch_shared_memory_object(name, &glob_exist_flag);
        // means that either local or global array is full, so no more objects can be added
        if (free_slot_local < 0 || global_object_index < 0)
            return -1;
        edit_shm_obj(global_object_index, name, glob_exist_flag);
        //cprintf("name %s\n", shared_memory_objects_global[global_object_index].name);
        struct shared_memory_object** shared_mem_obj = &current_proc->shared_mem_objects[free_slot_local].shared_mem_object;
        *shared_mem_obj = &shared_memory_objects_global[global_object_index];
        //cprintf("name1: %s\n", current_proc->shared_mem_objects[free_slot_local].shared_mem_object->name);
        return  free_slot_local;
    }
    return exist;
}

int shm_close_logic(int object_descriptor, struct proc* current_process){
    struct proc* current_proc = (current_process == 0) ? myproc():current_process;
    if (object_descriptor >= LOCAL_NUMBER_OF_SHM_OBJ || object_descriptor < 0)
        return -1;
    struct shared_memory_object** shared_mem_obj_glob = &current_proc->shared_mem_objects[object_descriptor].shared_mem_object;
    //struct shared_memory_object_local* = s
    if (current_proc->shared_mem_objects[object_descriptor].virtual_adress){
        uint oldsz, newsz;
        oldsz = PGROUNDUP(VIRT_SHM_MEM) + (object_descriptor * SHM_OBJ_MAX_SIZE);
        newsz = oldsz + (*shared_mem_obj_glob)->size;
        //cprintf("UNMAP GOT CALLED\n");
        unmap(current_proc->pgdir, oldsz, newsz);
    }
    acquire(&(*shared_mem_obj_glob)->lock);
    (*shared_mem_obj_glob)->ref_count --;
    if ((*shared_mem_obj_glob)->ref_count == 0){
        clean_shm_mem1((*shared_mem_obj_glob));
        release(&(*shared_mem_obj_glob)->lock);
        clean_shared_mem_obj((*shared_mem_obj_glob));
        clean_local_shared_mem_obj(&current_proc->shared_mem_objects[object_descriptor], object_descriptor);
        return 1;
    }
    release(&(*shared_mem_obj_glob)->lock);
    clean_local_shared_mem_obj(&current_proc->shared_mem_objects[object_descriptor], object_descriptor);
    return 1;
}
int shm_close(int object_descriptor){
    return shm_close_logic(object_descriptor, 0);
}
void shm_close_direct(int object_descriptor, struct proc* process){
    //shm_close_logic(object_descriptor, process);
}

int shm_trunc(int object_descriptor, int size){
    //struct shared_memory_object_local* shm_obj_local = &myproc()->shared_mem_objects[object_descriptor];
    struct shared_memory_object** shared_mem_obj_glob = &myproc()->shared_mem_objects[object_descriptor].shared_mem_object;
    char* memory;
    uint pages, new_size, address;
    if (object_descriptor >= LOCAL_NUMBER_OF_SHM_OBJ || object_descriptor < 0)
        return -1;
    if ((*shared_mem_obj_glob) == 0)
        return -1;
    pages = round_up_division(size, PGSIZE);
    if (pages > MAX_PAGES || size <= 0)
        return -1;
    if ((*shared_mem_obj_glob)->size != 0)
        return -1;
    //cprintf("Trunc opened with name %s and ref count %d and ID: %d\n", (*shared_mem_obj_glob)->name, (*shared_mem_obj_glob)->ref_count, (*shared_mem_obj_glob)->id);
    //cprintf("SIZE %d\n", (*shared_mem_obj_glob)->size);
    acquire(&(*shared_mem_obj_glob)->lock);
    address = PGROUNDUP(VIRT_SHM_MEM) + (object_descriptor * SHM_OBJ_MAX_SIZE);
    new_size = address + size;
    for (; address < new_size; address += PGSIZE){
        memory = kalloc();
        if (memory == 0){
            clean_shm_mem1((*shared_mem_obj_glob));
            release(&(*shared_mem_obj_glob)->lock);
            return  -1;
        }
        (*shared_mem_obj_glob)->allocated_pages ++;
        memset(memory, 0, PGSIZE);
        (*shared_mem_obj_glob)->memory[(*shared_mem_obj_glob)->allocated_pages] = memory;
    }
    (*shared_mem_obj_glob)->size = size;
    release(&(*shared_mem_obj_glob)->lock);
    return new_size;
}

int shm_map(int object_descriptor, void** virtual_adress, int flags){
    uint address, persistent_address;
    struct proc* current_proc = myproc();
    struct shared_memory_object_local* local_shm_obj = &current_proc->shared_mem_objects[object_descriptor];
    struct shared_memory_object** shared_mem_obj_glob = &current_proc->shared_mem_objects[object_descriptor].shared_mem_object;
    if (object_descriptor >= LOCAL_NUMBER_OF_SHM_OBJ || object_descriptor < 0)
        return -1;
    if (local_shm_obj->virtual_adress != 0 || (*shared_mem_obj_glob) == 0)
        return -1;
    address = persistent_address = (PGROUNDUP(VIRT_SHM_MEM) + (object_descriptor * SHM_OBJ_MAX_SIZE));
    for (int i = 0; i <= (*shared_mem_obj_glob)->allocated_pages; i ++){
        if (mappages(current_proc->pgdir, (char*) address, PGSIZE, V2P((*shared_mem_obj_glob)->memory[i]), flags | PTE_U) < 0){
            if (i > 0){
                unmap(current_proc->pgdir, persistent_address, address);
                return -1;
            }
        }
        address += PGSIZE;
    }
    *virtual_adress = (void*) persistent_address;
    local_shm_obj->virtual_adress = persistent_address;
    local_shm_obj->flags = flags;
    return 0;
}
