#ifndef SHARED_MEM_MODULE
#define SHARED_MEM_MODULE
#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
void init_shared_mem_objects(void);
// returns the object despriptor of the newly opened shm
// object
int shm_open(char* name);
// assignes memory to the shared memory object
// it will page align the size meaning the user might get
// more memory than they asked for
// only the first call to this funciton will succseed
// all subsequent fail
int shm_trunc(int object_descriptor, int size);
// stores the adress of the start of shared memory in virtual adress var
int shm_map(int object_descriptor, void **virtual_adress, int flags);
// drops the refrence count in the object struct, and
// clears the the object if ref reaches 0
int shm_close(int object_descriptor);

void shm_close_direct(int object_descriptor, struct proc* process);
//void copy_shm_vm(struct proc* parent, struct proc* child);
// void fork_proc_clone(struct proc* parrent, struct proc* child);
#endif