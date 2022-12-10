#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <ram_manage.h>

#define DUMBVM_STACKPAGES    18

void
vm_bootstrap(void)
{
	ram_manage_init();
}


void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}


int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	(void) faulttype;
        (void) faultaddress;
        return 0;
}


vaddr_t alloc_kpages(unsigned npages){
        vaddr_t vaddr = alloc_ker_pages(npages);
        return vaddr;
};
void free_kpages(vaddr_t addr){
        free_ker_pages(addr);
        
};