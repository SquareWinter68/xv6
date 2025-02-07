#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "shmem.h"

int
sys_fork(void)
{
	return fork();
}

int
sys_exit(void)
{
	exit();
	return 0;  // not reached
}

int
sys_wait(void)
{
	return wait();
}

int
sys_kill(void)
{
	int pid;

	if(argint(0, &pid) < 0)
		return -1;
	return kill(pid);
}

int
sys_getpid(void)
{
	return myproc()->pid;
}

int
sys_sbrk(void)
{
	int addr;
	int n;

	if(argint(0, &n) < 0)
		return -1;
	addr = myproc()->sz;
	if(growproc(n) < 0)
		return -1;
	return addr;
}

int
sys_sleep(void)
{
	int n;
	uint ticks0;

	if(argint(0, &n) < 0)
		return -1;
	acquire(&tickslock);
	ticks0 = ticks;
	while(ticks - ticks0 < n){
		if(myproc()->killed){
			release(&tickslock);
			return -1;
		}
		sleep(&ticks, &tickslock);
	}
	release(&tickslock);
	return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
	uint xticks;

	acquire(&tickslock);
	xticks = ticks;
	release(&tickslock);
	return xticks;
}

int sys_shm_open(void){
	char* name;
	// get nth argument 0 in this case, and save it as a 
	//pointer to a string
	if(argstr(0, &name) < 0)
		return -1;
	if (name[0] == 0)
        return -1;
	
	return shm_open(name);
}

int sys_shm_trunc(void){
	int object_descriptor, size;
	if (argint(0, &object_descriptor) < 0 ||
					argint(1, &size) < 0)
		return -1;
	
	return shm_trunc(object_descriptor, size);
}

int sys_shm_map(void){
	int object_descriptor, virtual_adress, flags;
	if (argint(0, &object_descriptor) < 0 ||
		argint(1, &virtual_adress) < 0 ||
		argint(2, &flags) < 0)
		return -1;
	return shm_map(object_descriptor, (void**) virtual_adress, flags);
}

int sys_shm_close(void){
	int object_descriptor;
	if (argint(0, &object_descriptor) < 0)
		return -1;
	shm_close(object_descriptor);
	return 0;
}