#ifndef SHARED_MEM_STRUC_MODULE
#define SHARED_MEM_STRUC_MODULE
#include "spinlock.h"
#define NAME_SZ 14
#define GLOBAL_NUMBER_OF_SHM_OBJ 64

#define MAX_SHM_OBJ_REFS NPROC*LOCAL_NUMBER_OF_SHM_OBJ
struct shared_memory_object{
    int id;
    struct spinlock lock;
    char name[NAME_SZ];
    char* memory[32];
    int ref_count;
    int allocated_pages;
    uint size;
};

struct shared_memory_object_local{
    uint virtual_adress;
    int flags;
    struct shared_memory_object* shared_mem_object;
};

#endif