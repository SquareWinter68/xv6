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
        shared_memory_objects_global[i].trunc_flag = shared_memory_objects_global[i].ref_count = shared_memory_objects_global[i].allocated_pages = 0;
        memset(shared_memory_objects_global[i].name, 0, NAME_SZ);
    }
    init_shared_mem_objects_local();
}

// called only when the last refrence to shm object is droped
void clean_shared_mem_obj(struct shared_memory_object* shm_obj){
    for (int i = 0; i < shm_obj->allocated_pages; i ++){
        kfree(shm_obj->memory[i]);
    }
    shm_obj->trunc_flag = shm_obj->ref_count = shm_obj->allocated_pages = 0;
    memset(shm_obj->name, 0, NAME_SZ);

}

void clean_local_shared_mem_obj(struct shared_memory_object_local* shm_obj_local){
    shm_obj_local->virtual_adress = shm_obj_local->size = 0;
    shm_obj_local->flags = shm_obj_local->object_descriptor = 0;
    shm_obj_local->shared_mem_object = 0; 
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
        cprintf("Opened new %d\n", local_obj->object_descriptor);
        return local_obj->object_descriptor;
    }
    cprintf("Name existed %d\n", exists);
    return exists;
}


int close_logic(int object_descriptor, struct proc* current_process){
    struct proc* current_proc = (current_process == 0) ? myproc():current_process;
    if (object_descriptor >= LOCAL_NUMBER_OF_SHM_OBJ || object_descriptor < 0){
        cprintf("Invalid object descriptor provided to close%d\n", object_descriptor);
        return -1;
    }
    struct shared_memory_object_local* shm_obj_local = current_proc->shared_mem_objects[object_descriptor];
    struct shared_memory_object* shm_obj_glob = shm_obj_local->shared_mem_object;
    if (shm_obj_local->virtual_adress){
        // the object needs to be unmaped
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
        cprintf("Closed %d\n", shm_obj_local->object_descriptor);
        clean_local_shared_mem_obj(shm_obj_local);
        release(&shm_obj_glob->lock);
        return 1;
    }
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
void copy_shm_vm(pde_t* pgdir, struct proc* child){}
int shm_trunc(int object_descriptor, int size){}
int shm_map(int object_descriptor, void **virtual_adress, int flags){}