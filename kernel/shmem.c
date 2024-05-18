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
#define LOCAL_NUMBER_OF_SHM_OBJ 16
#define MAX_PAGES 32
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
} shared_memory_objects[GLOBAL_NUMBER_OF_SHM_OBJ];


// this struct will be local to the processes that have a refrence to the shared memory object
// the process is meant to have a list of these structs withihn the struct proc, and index init the list by the descriptor assigned
//====================================================IMPORTANT====================================================
//this should really go into the header file, do that once you find time
//====================================================IMPORTANT====================================================
struct shared_memory_object_local{
    int virtual_adress; // I am not to sure about the datatype of a virtual adress 
    int shared_mem_descriptor;
    int owner_pid;
    int flags;
    struct shared_memory_object* shared_mem_object;
    
}process_local_shm_objects[MAX_SHM_OBJ_REFS];

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

void clean_allocated_mem(struct shared_memory_object* shm_obj){
    int i = shm_obj->allocated_pages;
    for (; i >= 0; i --){
        // free the pages that were allocated prior to failure if any
        kfree(shm_obj->memory[i]);
        cprintf("I freed a page of memory\n");
    }
    shm_obj->allocated_pages = i;
}

void init_process_local_shm_objs(void){
    for (int i = 0; i < MAX_SHM_OBJ_REFS; i++){
        process_local_shm_objects[i].owner_pid = process_local_shm_objects[i].shared_mem_descriptor = -1;
        process_local_shm_objects[i].flags = 0;
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

    // might be replaced with deallocuvm
    clean_allocated_mem(shm_obj);
    // free any pages allocated by kalloc if any
}

void clean_local_shared_mem_obj(struct shared_memory_object_local* shm_obj){
    shm_obj->shared_mem_descriptor = -1;
    shm_obj->flags = shm_obj->owner_pid = 0;
    shm_obj->shared_mem_object = 0;
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
     
    clean_local_shared_mem_obj(current_proc->shared_mem_objects[object_descriptor]);
    if (shm_obj->ref_count == 1){
        shm_obj->ref_count --;
        cprintf("entered cleaning section\n");
        clean_shared_mem_obj(shm_obj, 0);
        release(&shm_obj->lock);
        cprintf("it seems i workded correctly\n");
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
        
        cprintf("Name:%s, ref:%d od:%d\n", current_process->
        shared_mem_objects[local_obj->shared_mem_descriptor]->shared_mem_object->name,
        current_process->shared_mem_objects[local_obj->shared_mem_descriptor]->shared_mem_object->ref_count, local_obj->shared_mem_descriptor);
        current_process->shared_mem_objects_size++;
        release(&shared_memory_objects[object_descriptor].lock);
        return local_obj->shared_mem_descriptor;
    }
    
    cprintf("i already existed \n");
    acquire(&current_process->shared_mem_objects[exists]->shared_mem_object->lock);
    cprintf("Name:%s, ref:%d od:%d\n", current_process->
        shared_mem_objects[exists]->shared_mem_object->name,
        current_process->shared_mem_objects[exists]->shared_mem_object->ref_count, exists);   
    release(&current_process->shared_mem_objects[exists]->shared_mem_object->lock);
    return exists; 
}

int shm_trunc(int object_descriptor, int size){

}

int shm_map(int object_descriptor, void **virtual_adress, int flags){

}


int shm_close(int object_descriptor){
    struct proc* current_process = myproc();
    if (object_descriptor >= current_process->shared_mem_objects_size || object_descriptor < 0)
        return -1;
    if (drop_refrence_count(current_process, object_descriptor) < 0)
        return -1;
    return 0;
}

void drop_refrence_direct(struct shared_memory_object_local* shm_obj_local){
    struct shared_memory_object* shm_obj = shm_obj_local->shared_mem_object;
    // already holding ptable lock, no need to lock here
   // acquire(&shm_obj->lock);
    cprintf("hello from direct\n");
    clean_local_shared_mem_obj(shm_obj_local);
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

// fix the proc.h problem where if you pop the first elemen the pointer to the available slot just
// decrements