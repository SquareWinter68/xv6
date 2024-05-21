#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "vm.h"

#define NAME_SZ 14
#define GLOBAL_NUMBER_OF_SHM_OBJ 64

#define MAX_SHM_OBJ_REFS NPROC*LOCAL_NUMBER_OF_SHM_OBJ

static int local_shm_obj_arr_index = 0;

struct shared_memory_object{
    int id;
    struct spinlock lock;
    char name[NAME_SZ];
    // this will be the memory allocated by kalloc
    char* memory[MAX_PAGES];
    // this will indicate wheather the object is in use
    //  1 = used, 0 = unused
    int ref_count;
    // keep a trac of how many pages were allocated in trunc, 
    // not to cause a panic when freeing
    int allocated_pages;

    uint trunc_flag;
} shared_memory_objects[GLOBAL_NUMBER_OF_SHM_OBJ];


// this struct will be local to the processes that have a refrence to the shared memory object
// the process is meant to have a list of these structs withihn the struct proc, and index init the list by the descriptor assigned
//====================================================IMPORTANT====================================================
//this should really go into the header file, do that once you find time
//====================================================IMPORTANT====================================================
struct shared_memory_object_local{
    uint virtual_adress; // I am not to sure about the datatype of a virtual adress 
    int shared_mem_descriptor;
    int owner_pid;
    int flags;
    uint size;
    struct shared_memory_object* shared_mem_object;
    
}process_local_shm_objects[MAX_SHM_OBJ_REFS];

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

void copy_shm_vm(pde_t* pgdir, struct proc* child){
    pte_t *pte;
    for (uint i = VIRT_SHM_MEM; i < KERNBASE; i += PGSIZE){
        pte = walkpgdir(pgdir, (void*) i, 0);
        if (pte && (*pte & PTE_P)){
            uint physical_adress = PTE_ADDR(*pte);
            int flags = PTE_FLAGS(*pte);
            cprintf("I:%d\n", i);
            mappages(child->pgdir, (void*) i, PGSIZE, physical_adress, flags);
        }
    }
}

void clean_allocated_mem(struct shared_memory_object* shm_obj){
    int i = shm_obj->allocated_pages;
    for (; i >= 0; i --){
        // free the pages that were allocated prior to failure if any
        kfree(shm_obj->memory[i]);
        //cprintf("I freed a page of memory\n");
    }
    shm_obj->allocated_pages = i;
}

void init_process_local_shm_objs(void){
    for (int i = 0; i < MAX_SHM_OBJ_REFS; i++){
        process_local_shm_objects[i].owner_pid = process_local_shm_objects[i].shared_mem_descriptor = -1;
        process_local_shm_objects[i].flags = process_local_shm_objects[i].virtual_adress = 0;
        process_local_shm_objects[i].size = 0;
    }
}

void init_shared_mem_objects(void){
    // caled from main.c once on startup to initialize the shared memory objects
    for (int i = 0; i < GLOBAL_NUMBER_OF_SHM_OBJ; i ++){
        shared_memory_objects[i].id = i;
        // a lock must be initialized before it is ever used
        initlock(&shared_memory_objects[i].lock, "shm_object_lock");
        shared_memory_objects[i].ref_count = 0;
        shared_memory_objects[i].allocated_pages = -1;
        shared_memory_objects[i].trunc_flag = 0;
        memset(shared_memory_objects[i].name, 0, NAME_SZ);
    }
    //to initialize the process local shm objects list as well
    init_process_local_shm_objs();
}

// takes locked shared memory object, and cleans it.
void clean_shared_mem_obj(struct shared_memory_object* shm_obj, int direct){
    //struct shared_memory_object* shm_obj = &shared_memory_objects[index];
    // clear the name field
    memset(shm_obj->name, 0, NAME_SZ);
    // frees the allocated memory
    
    if (direct == 0)
        //deallocuvm(myproc()->pgdir, uint, uint)
     shm_obj->trunc_flag = 0;

    // might be replaced with deallocuvm
    clean_allocated_mem(shm_obj);
    // free any pages allocated by kalloc if any
}

void clean_local_shared_mem_obj(struct shared_memory_object_local* shm_obj, int direct){
    if (direct){
        shm_obj->shared_mem_descriptor = -1;
        shm_obj->flags = shm_obj->owner_pid = 0;
        shm_obj->virtual_adress = 0;
        shm_obj->size = 0;
    }
    else {
        shm_obj->shared_mem_descriptor = -1;
        shm_obj->flags = shm_obj->owner_pid = 0;
        shm_obj->shared_mem_object = 0;
        shm_obj->virtual_adress = 0;
        shm_obj->size = 0;
    }
}
// if found, returns the index of the free or specified object in the global array of 
// shared mem objects
int fecth_unused_shm_object(char* name){
    int firs_free = -1;
    for (int i = 0; i < GLOBAL_NUMBER_OF_SHM_OBJ; i++){
        acquire(&shared_memory_objects[i].lock);
        if (strcmp(name, shared_memory_objects[i].name) == 0){
            shared_memory_objects[i].ref_count ++;
            release(&shared_memory_objects[i].lock);
            return i;
        }
        
        if (firs_free < 0){
            if (shared_memory_objects[i].name[0] == 0){
                firs_free = i;
            }
        }
        release(&shared_memory_objects[i].lock);
    }
    if (firs_free > -1){
        acquire(&shared_memory_objects[firs_free].lock);
        shared_memory_objects[firs_free].ref_count ++;
        strcpy(shared_memory_objects[firs_free].name, name);
        release(&shared_memory_objects[firs_free].lock);
        return firs_free;
    }
        
    return -1; // No unused shared memory object found
}

int drop_refrence_count(struct proc* current_proc, int object_descriptor){
    struct shared_memory_object* shm_obj = current_proc->
                                            shared_mem_objects[object_descriptor]->
                                            shared_mem_object;
    // since the process, droped the memory object, the list shrank thusly
    current_proc->shared_mem_objects_size --;
    current_proc->shm_occupied[object_descriptor] = 0;
    local_shm_obj_arr_index --;
    acquire(&shm_obj->lock);
    // no need for size -1, since it was decremented above
     
    clean_local_shared_mem_obj(current_proc->shared_mem_objects[object_descriptor], 0);
    if (shm_obj->ref_count == 1){
        shm_obj->ref_count --;
       // cprintf("entered cleaning section\n");
        clean_shared_mem_obj(shm_obj, 0);
        release(&shm_obj->lock);
       // cprintf("it seems i workded correctly\n");
        return 1;
    }
    if (shm_obj->ref_count != 0){
        shm_obj->ref_count --;
        release(&shm_obj->lock);
        return 1;
    }
    release(&shm_obj->lock);
    // error, the object whose refrence count the user tried to decrease
    // is already at 0
    return -1;
}
// returns the index in the processes local shared mem objects table
int check_if_exists(char* name, struct proc* current_proc){
    for (int i = 0; i < 16; i ++){
        if (current_proc->shm_occupied[i]){
            struct shared_memory_object* shm_obj = current_proc->shared_mem_objects[i]->shared_mem_object;
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

int find_free_slot(struct proc* current_process){
    for (int i = 0; i < 16; i ++){
        if (current_process->shm_occupied[i] == 0){
            current_process->shm_occupied[i] = 1;
            return i;
        }
    }
    return  -1;
}
// this function clears the p bit in page table entries starting from adress "from" and ending at "to" in thereby freing them
void unmap(pde_t* pgdir, uint from, uint to){
   // cprintf("Unmap got called \n");
    pte_t* page_table_entry;
    uint adress = PGROUNDUP(from);
    for (; adress < to; adress += PGSIZE){
        
        page_table_entry =  walkpgdir(pgdir, (char*)adress, 0);
        if (*page_table_entry & PTE_P){
            cprintf("UNMAPED ONE PAGE\n");
            *page_table_entry = 0;
        }
    }
}

int shm_open(char* name){
   

    //get current process
    struct proc* current_process = myproc();
    int exists = check_if_exists(name, current_process);
    if (exists < 0){
        if (current_process->shared_mem_objects_size == LOCAL_NUMBER_OF_SHM_OBJ){
            cprintf("too many objects nigga\n");
            return -1;
        }
        int object_descriptor = fecth_unused_shm_object(name);
        
        //cprintf("the memory at which i start is %d  %d\n", PGROUNDUP(VIRT_SHM_MEM), VIRT_SHM_MEM);
        if (object_descriptor < 0)
            return -1;
            // error, could not find unused shared memory object
        acquire(&shared_memory_objects[object_descriptor].lock);
        // getting the relevant object from the local processes array, and incrementing the index
        struct shared_memory_object_local* local_obj = &process_local_shm_objects[local_shm_obj_arr_index++];
        local_obj->owner_pid = current_process->pid;
        local_obj->shared_mem_object = &shared_memory_objects[object_descriptor];
        local_obj->shared_mem_descriptor = find_free_slot(current_process);
        
        current_process->shared_mem_objects[local_obj->shared_mem_descriptor] = local_obj;
        
        // cprintf("Name:%s, ref:%d od:%d\n", current_process->
        // shared_mem_objects[local_obj->shared_mem_descriptor]->shared_mem_object->name,
        // current_process->shared_mem_objects[local_obj->shared_mem_descriptor]->shared_mem_object->ref_count, local_obj->shared_mem_descriptor);
        current_process->shared_mem_objects_size++;
        release(&shared_memory_objects[object_descriptor].lock);
        return local_obj->shared_mem_descriptor;
    }
    
    //cprintf("i already existed \n");
    acquire(&current_process->shared_mem_objects[exists]->shared_mem_object->lock);
    // cprintf("Name:%s, ref:%d od:%d\n", current_process->
    //     shared_mem_objects[exists]->shared_mem_object->name,
    //     current_process->shared_mem_objects[exists]->shared_mem_object->ref_count, exists);   
    release(&current_process->shared_mem_objects[exists]->shared_mem_object->lock);
    return exists; 
}

int shm_trunc(int object_descriptor, int size){
    struct proc* current_proc = myproc();
    struct shared_memory_object_local* shm_obj_local = current_proc->shared_mem_objects[object_descriptor];
    struct shared_memory_object* shm_obj = shm_obj_local->shared_mem_object;
    char* memory;
    uint pages, new_size, address;
    if (object_descriptor >= current_proc->shared_mem_objects_size || object_descriptor < 0)
        return -1;
    if (current_proc->shm_occupied[object_descriptor] == 0)
        return -1;
    pages = round_up_division(size, PGSIZE);
    if (pages > MAX_PAGES || size <= 0)
        return -1;
    if (shm_obj->trunc_flag != 0 /*|| size/PGSIZE > MAX_PAGES*/){
        cprintf("You tried to truncate twice wtf? Or maybe\n");
        cprintf("too many pages\n");
        return -1;
    }
    acquire(&shm_obj->lock);
    address = PGROUNDUP(VIRT_SHM_MEM) + (object_descriptor * SHM_OBJ_MAX_SIZE); // the page round up does absolutely nothing here
    new_size = address + size; // maximum possible value 2147483648
    cprintf("I start from %d\n", address);
    cprintf("I start from page aligned %d\n", PGROUNDUP(address));
    for (; address < new_size; address += PGSIZE){
        memory = kalloc();
        if (memory == 0){
            cprintf("Allocator ran out of memory\n"); 
            clean_allocated_mem(shm_obj);
            release(&shm_obj->lock);
            return -1;
        }
        shm_obj->allocated_pages ++ ;
        shm_obj->memory[shm_obj->allocated_pages] = memory;
      //  cprintf("allocated a page of memory\n");
        memset(memory, 0, PGSIZE);
    }
    shm_obj->trunc_flag = new_size;
    release(&shm_obj->lock);
    return new_size;
}

int shm_map(int object_descriptor, void **virtual_adress, int flags){
    struct proc* current_proc = myproc();
    uint address, persistent_address;
    if (object_descriptor >= LOCAL_NUMBER_OF_SHM_OBJ || object_descriptor < 0)
        return -1;
    if (current_proc->shm_occupied[object_descriptor] == 0)
        return -1;
    struct shared_memory_object* shm_obj = current_proc->shared_mem_objects[object_descriptor]->shared_mem_object;
    address = persistent_address = (PGROUNDUP(VIRT_SHM_MEM) + (object_descriptor * SHM_OBJ_MAX_SIZE));
    for (int i = 0; i <= shm_obj->allocated_pages; i ++){
        if (mappages(current_proc->pgdir, (char*)address, PGSIZE, V2P(shm_obj->memory[i]), flags|PTE_U) < 0){
            cprintf("Mappages failed\n");
            if (i > 0){
                unmap(current_proc->pgdir, persistent_address, address);
                return -1;
            }
        }
       // cprintf("MAPPED A PAGE OF MEMORY\n");
        address += PGSIZE;
    }
    *virtual_adress = (void*)persistent_address;
    int ** ptr = (int**)virtual_adress;
    int* ptr1 = *ptr;
    ptr1[0] = 42;
    current_proc->shared_mem_objects[object_descriptor]->virtual_adress = persistent_address;
    current_proc->shared_mem_objects[object_descriptor]->size = address;
    return 0;
}


int shm_close(int object_descriptor){
    struct proc* current_process = myproc();
    uint oldsz, newsz;
    if (object_descriptor >= LOCAL_NUMBER_OF_SHM_OBJ || object_descriptor < 0)
        return -1;
    //cprintf("Entered close with descriptor %d\n", object_descriptor);
    oldsz = PGROUNDUP(VIRT_SHM_MEM) + (object_descriptor * SHM_OBJ_MAX_SIZE);
    newsz = current_process->shared_mem_objects[object_descriptor]->size;
    // chrck to see if maped, only unmap then
    if (current_process->shared_mem_objects[object_descriptor]->virtual_adress){
        cprintf("called from close \n");
        unmap(current_process->pgdir, oldsz, newsz);
    }
        
    if (drop_refrence_count(current_process, object_descriptor) < 0)
        return -1;
    return 0;
    //do not change the process size
}

void drop_refrence_direct(struct shared_memory_object_local* shm_obj_local, struct proc* process, int object_descriptor){
    struct shared_memory_object* shm_obj = shm_obj_local->shared_mem_object;
    // already holding ptable lock, no need to lock here
   // acquire(&shm_obj->lock);
    //cprintf("hello from direct\n");
    uint start = PGROUNDUP(VIRT_SHM_MEM) + (object_descriptor * SHM_OBJ_MAX_SIZE);
    if (shm_obj_local->virtual_adress){
        cprintf("called from drop refrence direct \n");
        unmap(process->pgdir, start, shm_obj_local->size);
    }
        
    clean_local_shared_mem_obj(shm_obj_local, 1);
    if (shm_obj->ref_count == 1){
        shm_obj->ref_count --;
        clean_shared_mem_obj(shm_obj, 1);
        //release(&shm_obj->lock);
        return;
    }
    if (shm_obj->ref_count != 0){
        shm_obj->ref_count --;
        //release(&shm_obj->lock);
        return;
    }
    //release(&shm_obj->lock);
}

// unclutter