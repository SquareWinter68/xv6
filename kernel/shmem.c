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
static int initialized = 0;
static int local_shm_obj_array_index = 0;

struct shared_memory_object{
    struct spinlock lock;
    char name[NAME_SZ];
    char* memory[MAX_PAGES];
    int ref_count;
    int allocated_pages;
    int trunc_flag;
} shared_memory_objects_global[GLOBAL_NUMBER_OF_SHM_OBJ];

struct shared_memory_object_local{
    uint virtual_adress;
    int flags;
    uint size;
    int object_descriptor;
    struct shared_memory_object* shared_mem_object;
} shared_memory_objects_local[MAX_SHM_OBJ_REFS];

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

void init_shared_mem_objects_local(void){
    for (int i = 0; i < MAX_SHM_OBJ_REFS; i++){
        shared_memory_objects_local[i].virtual_adress = shared_memory_objects_local[i].size = 0;
        shared_memory_objects_local[i].flags = shared_memory_objects_local[i].object_descriptor = 0;
        shared_memory_objects_local[i].shared_mem_object = 0;
    }
}

void init_shared_mem_objects(void){
    for (int i = 0; i < GLOBAL_NUMBER_OF_SHM_OBJ; i ++){
        initlock(&shared_memory_objects_global[i].lock, "shm_object_lock");
        shared_memory_objects_global[i].trunc_flag = shared_memory_objects_global[i].ref_count = 0;
        shared_memory_objects_global[i].allocated_pages = -1;
        memset(shared_memory_objects_global[i].name, 0, NAME_SZ);
    }
    init_shared_mem_objects_local();
    initialized = 1;
}

void clean_shm_mem(struct shared_memory_object* shm_obj){
    for (int i = 0; i <= shm_obj->allocated_pages; i ++){
        kfree(shm_obj->memory[i]);
    }
    shm_obj->allocated_pages = -1;
}

// called only when the last refrence to shm object is droped
void clean_shared_mem_obj(struct shared_memory_object* shm_obj){
    shm_obj->trunc_flag = shm_obj->ref_count = 0;
    clean_shm_mem(shm_obj);
    memset(shm_obj->name, 0, NAME_SZ);

}

void clean_local_shared_mem_obj(struct shared_memory_object_local* shm_obj_local){
    shm_obj_local->virtual_adress = shm_obj_local->size = 0;
    shm_obj_local->flags = shm_obj_local->object_descriptor = 0;
    // causes locks for some reason
    //shm_obj_local->shared_mem_object = 0; 
}

int fetch_shared_memory_object(char* name){
    int first_free = -1;
    for (int i = 0; i < GLOBAL_NUMBER_OF_SHM_OBJ; i++){
        acquire(&shared_memory_objects_global[i].lock);
        if (strcmp(name, shared_memory_objects_global[i].name) == 0){
            shared_memory_objects_global[i].ref_count ++;
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
        acquire(&shared_memory_objects_global[first_free].lock);
        shared_memory_objects_global[first_free].ref_count ++;
        strcpy(shared_memory_objects_global[first_free].name, name);
        release(&shared_memory_objects_global[first_free].lock);
        return first_free;
    }

    return -1; // no free object available
}

int check_if_exists(char* name, struct proc* current_proc){
    for (int i = 0; i < 16; i++) {
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

int find_free_slot(struct proc* current_proc){
    for (int i = 0; i < 16; i ++){
        if (current_proc->shm_occupied[i] == 0){
            current_proc->shm_occupied[i] = 1;
            return i;
        }
    }
    return -1; // no free slot found
}

void unmap(pde_t* pgdir, uint from, uint to){
   // cprintf("Unmap got called \n");
    pte_t* page_table_entry;
    uint adress = PGROUNDUP(from);
    for (; adress < to; adress += PGSIZE){
        
        page_table_entry =  walkpgdir(pgdir, (char*)adress, 0);
        if (*page_table_entry & PTE_P){
            cprintf("UNMAPED ONE PAGE\n");
            *page_table_entry &= ~PTE_P;
            *page_table_entry &= ~PTE_U;
        }
    }
}

int shm_open(char* name){
    struct proc* current_proc = myproc();
    int exists = check_if_exists(name, current_proc);
    if (exists < 0){
        if (current_proc->shared_mem_objects_size == LOCAL_NUMBER_OF_SHM_OBJ){
            cprintf("too many objects open\n");
            return -1;
        }
        int object_index = fetch_shared_memory_object(name);
        if (object_index < 0){
            cprintf("fetch shared mem object returned -1");
            return -1;
        }
        acquire(&shared_memory_objects_global[object_index].lock);
        struct shared_memory_object_local* local_obj = &shared_memory_objects_local[local_shm_obj_array_index++];
        local_obj->shared_mem_object = &shared_memory_objects_global[object_index];
        local_obj->object_descriptor = find_free_slot(current_proc);
        
        current_proc->shared_mem_objects[local_obj->object_descriptor] = local_obj;
        current_proc->shared_mem_objects_size ++;
        release(&shared_memory_objects_global[object_index].lock);
        cprintf("Opened new local %d\n", local_obj->object_descriptor);
        return local_obj->object_descriptor;
    }
    cprintf("Name existed locally %d\n", exists);
    return exists;
}


int close_logic(int object_descriptor, struct proc* current_process){
    cprintf("Entered close\n");
    struct proc* current_proc = (current_process == 0) ? myproc():current_process;
    if (object_descriptor >= LOCAL_NUMBER_OF_SHM_OBJ || object_descriptor < 0){
        cprintf("Invalid object descriptor provided to close%d\n", object_descriptor);
        return -1;
    }
    struct shared_memory_object_local* shm_obj_local = current_proc->shared_mem_objects[object_descriptor];
    struct shared_memory_object* shm_obj_glob = shm_obj_local->shared_mem_object;
    if (shm_obj_local->virtual_adress){
        // the object needs to be unmaped
        uint oldsz, newsz;
        oldsz = PGROUNDUP(VIRT_SHM_MEM) + (object_descriptor * SHM_OBJ_MAX_SIZE);
        newsz = shm_obj_local->size;
        unmap(current_proc->pgdir, oldsz, newsz);
    }
    if (shm_obj_glob->ref_count > 0){
        acquire(&shm_obj_glob->lock);
        if (shm_obj_glob->ref_count == 1){
            shm_obj_glob->ref_count --;
            clean_shared_mem_obj(shm_obj_glob);
        }
        else {
            shm_obj_glob->ref_count --;
        }
        local_shm_obj_array_index --;
        current_proc->shm_occupied[object_descriptor] = 0;
        current_proc->shared_mem_objects_size --;
        cprintf("Closed %d from %s\n", shm_obj_local->object_descriptor, (current_process == 0) ? "close":"wait");
        clean_local_shared_mem_obj(shm_obj_local);
        release(&shm_obj_glob->lock);
        cprintf("close worked correctly and finnished\n");
        return 1;
    }
    cprintf("close failed and finnished\n");
    return -1;
}

int shm_close(int object_descriptor){
    if (close_logic(object_descriptor, 0) < 0)
        return -1;
    return 1;
}

void shm_close_direct(int object_descriptor, struct proc* process){
    close_logic(object_descriptor, process);
}

int shm_trunc(int object_descriptor, int size){
    struct proc* current_proc = myproc();
    struct shared_memory_object_local* shm_obj_local = current_proc->shared_mem_objects[object_descriptor];
    struct shared_memory_object* shm_obj_glob = shm_obj_local->shared_mem_object;
    char* memory;
    uint pages, new_size, address;
    if (object_descriptor >= LOCAL_NUMBER_OF_SHM_OBJ || object_descriptor < 0)
        return -1;
    if (current_proc->shm_occupied[object_descriptor] == 0){
        cprintf("TRUNC: Object non existent\n");
        return -1;
    }
    pages = round_up_division(size, PGSIZE);
    if (pages > MAX_PAGES || size <= 0){
        cprintf("TRUNC: Size inadequate\n");
        return -1;
    }
    if (shm_obj_glob->trunc_flag != 0){
        cprintf("TRUNC: Cannot truncate more than once\n");
        return -1;
    }
    acquire(&shm_obj_glob->lock);
    address = PGROUNDUP(VIRT_SHM_MEM) + (object_descriptor * SHM_OBJ_MAX_SIZE); // the page round up does absolutely nothing here
    new_size = address + size; // maximum possible value 2147483648
    for (; address < new_size; address += PGSIZE){
        memory = kalloc();
        if (memory == 0){
            cprintf("TRUNC: allocator ran out of memory");
            clean_shm_mem(shm_obj_glob);
            release(&shm_obj_glob->lock);
            return -1;
        }
        shm_obj_glob->allocated_pages++;
        shm_obj_glob->memory[shm_obj_glob->allocated_pages] = memory;
        memset(memory, 0, PGSIZE);
    }
    shm_obj_glob->trunc_flag = 1;
    release(&shm_obj_glob->lock);
    return 1;
}

int shm_map(int object_descriptor, void **virtual_adress, int flags){
    uint address, persistent_address;
    struct proc* current_procces = myproc();
    if (object_descriptor >= LOCAL_NUMBER_OF_SHM_OBJ || object_descriptor < 0){
        cprintf("MAP: inadequate descriptor\n");
        return -1;
    }
    if (current_procces->shm_occupied[object_descriptor] == 0){
        cprintf("MAP: Object not present\n");
        return -1;
    }
    struct shared_memory_object_local* shm_obj_local = current_procces->shared_mem_objects[object_descriptor];
    struct shared_memory_object* shm_obj_glob = shm_obj_local->shared_mem_object;
    if (shm_obj_local->virtual_adress != 0){
        cprintf("MAP: Cannot remap object\n");
        return -1;
    }
    address = persistent_address = (PGROUNDUP(VIRT_SHM_MEM) + (object_descriptor * SHM_OBJ_MAX_SIZE));
    for (int i = 0; i <= shm_obj_glob->allocated_pages; i ++){
        cprintf("MADE IT HERE\n");
        if (mappages(current_procces->pgdir, (char*) address, PGSIZE, V2P(shm_obj_glob->memory[i]), flags | PTE_U) < 0){
            cprintf("MAP: mapping failed\n");
            if (i > 0){
                //map failed call unmap
                unmap(current_procces->pgdir, persistent_address, address);
                return -1;
            }
        }
        address += PGSIZE;
    }
    cprintf("and here\n");
    *virtual_adress = (void*) persistent_address;
    shm_obj_local->virtual_adress = persistent_address;
    shm_obj_local->size = address;
    cprintf("maybe even here\n");
    return 0;
}

void test_validity(struct proc* parent, struct proc* child){
    cprintf("ENTERED VALIDITY TEST\n");
    if (parent->shared_mem_objects_size != child->shared_mem_objects_size){
        cprintf("VALIDITY: parent and child shm obj sizes do not match\n");
    }
    for (int i = 0; i < 16; i ++){
        if (parent->shm_occupied[i] != child->shm_occupied[i]){
            cprintf("VALIDITY: parent and child ocupied slots do not match\n");
        }
    }
    for (int i = 0; i < 16; i ++){
        struct shared_memory_object_local* parent_shm_local = parent->shared_mem_objects[i];
        struct shared_memory_object_local* child_shm_local = child->shared_mem_objects[i];
        if (parent_shm_local != child_shm_local){
            cprintf("VALIDITY: parent and child proccess local shared memory object pointers do not match\n");
        }
        if (parent_shm_local->size != child_shm_local->size){
            cprintf("VALIDITY: parent and child shm_local do not match in size\n");
        }
        if (parent_shm_local->flags != child_shm_local->flags){
            cprintf("VALIDITY: parent and child shm_local do not match in size\n");
        }
        if (parent_shm_local->shared_mem_object != child_shm_local->shared_mem_object){
            cprintf("VALIDITY: parrent and child shm_local objects do not point to the same global object\n");
        }
        if (parent_shm_local->virtual_adress != child_shm_local->virtual_adress){
            cprintf("VALIDITY: parrent and child virtual addresses do not match\n");
        }
        else {
            pte_t* page_table_entry_parent;
            pte_t* page_table_entry_child;
            page_table_entry_parent = walkpgdir(parent->pgdir, (void*)parent_shm_local->virtual_adress, 0);
            page_table_entry_child = walkpgdir(child->pgdir, (void*)child_shm_local->virtual_adress, 0);
            uint parent_physical_address = PTE_ADDR(*page_table_entry_parent);
            uint child_physical_address = PTE_ADDR(*page_table_entry_child);
            int flags_parent = PTE_FLAGS(*page_table_entry_parent);
            int flags_child = PTE_FLAGS(*page_table_entry_child);
            if (parent_physical_address != child_physical_address){
                cprintf("VALIDITY: parrent and child physical addersses do not match\n");
            }
            if (flags_parent != flags_child){
                cprintf("VALIDITY: parrent and child flags do not match\n");
            }
        }
    }
    cprintf("EXITED VALIDITY TEST\n");
}

void copy_shm_vm(struct proc* parent, struct proc* child){
    cprintf("ENTERED COPY SHM_VM\n");
    pte_t* pte;
    uint start, stop;
    start = VIRT_SHM_MEM;
    stop = KERNBASE;
    for (; start < stop; start += PGSIZE){
        pte = walkpgdir(parent->pgdir, (void*)start, 0);
        if (pte && (*pte & PTE_P)){
            uint physical_adress = PTE_ADDR(*pte);
            int flags = PTE_FLAGS(*pte);
            mappages(child->pgdir, (void*)start, PGSIZE, physical_adress, flags | PTE_U);
        }
    }
    test_validity(parent, child);
    cprintf("EXITED COPY SHM_VM\n");
}

void fork_proc_clone(struct proc* parrent, struct proc* child){
    if (initialized){
        cprintf("ENTERED COPY FORK_CLONE\n");
        for (int i = 0; i < 16; i ++){
		child->shm_occupied[i] = parrent->shm_occupied[i];
            if (child->shm_occupied[i]){
                child->shared_mem_objects[i] = parrent->shared_mem_objects[i];
                parrent->shared_mem_objects[i]->shared_mem_object->ref_count ++;
            }
	    }
        cprintf("EXITED COPY FORK_CLONE\n");
    }
}