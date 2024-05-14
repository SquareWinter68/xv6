#ifndef VIRTUAL_MEM_MODULE
#define VIRTUAL_MEM_MODULE
int mappages(pde_t *pgdir, void *virtual_adress, uint size, uint physical_adress, int perm);

#endif