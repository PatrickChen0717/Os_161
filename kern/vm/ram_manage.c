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

/*
 * Wrap ram_stealmem in a spinlock.
 * spin lock we need to used when we call ram_stealmem()
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;


/*the arayy of ram management unit*/
static struct ram_chunk *ram_mu;

/* A lock used to protect the ram_mu when we make change the ram_mu*/
static struct spinlock ram_mu_lock = SPINLOCK_INITIALIZER;

/* The number of the physical pages that we have in total*/
static int total_pages = 0;

/*whether the ram_mu  has been boostrap*/

static int ram_mu_boost = 0;

void ram_manage_init(void)
{
        unsigned long total_size = ram_getsize();
        total_pages= total_size / 4096; 

        
        
        ram_mu = kmalloc(total_pages * sizeof(struct ram_chunk));

        long after = ram_getfirstfree()/4096;

        spinlock_acquire(&ram_mu_lock);
        for( long i = 0; i<after+1; i++){
                ram_mu[i].free = 0;
                ram_mu[i].size = 0;
                ram_mu[i].virtual_addr = 0;
                ram_mu[i].as = NULL;
        }
        for(long i = after+1;i<total_pages;i++){
                ram_mu[i].free = 1;
                ram_mu[i].size = 0;
                ram_mu[i].virtual_addr = 0;
                ram_mu[i].as = NULL;
        }
        
        ram_mu_boost = 1;
        spinlock_release(&ram_mu_lock);

}
/*when the kernel called kmalloc we first try to find some 
 *free pages that managemend by our  ram management array.
 */
static paddr_t k_try_free_pages(unsigned long npages)
{
        paddr_t addr = 0;
        unsigned long free_counter = 0;
        long start_index =-1;
        if(ram_mu_boost ==1){
                spinlock_acquire(&ram_mu_lock);
                for( long i=0;i<total_pages;i++){
                        if(ram_mu[i].free){
                                
                                free_counter = free_counter +1;
                                
                                if(i!=0){
                                        if(!ram_mu[i-1].free){
                                                free_counter = 1;
                                        }
                                }
                                if(free_counter == npages){
                                        start_index=i-npages+1;
                                        addr = (paddr_t) (start_index*4096);
                                        break;
                                } 
                        }         
                }
                if(start_index != -1){
                        //update the information in the ram_mu
                        for(unsigned long i = start_index; i<start_index+npages; i++){
                                ram_mu[i].free = 0;
                        }
                        ram_mu[start_index].size = npages;
                        ram_mu[start_index].virtual_addr = PADDR_TO_KVADDR(addr);

                }
                spinlock_release(&ram_mu_lock);
        }
        return addr;

}


vaddr_t alloc_ker_pages(unsigned long npages)
{
        paddr_t paddr = 0;
        unsigned long pages= npages;

        paddr = k_try_free_pages(pages);
        //we do not find any free pages that are managemend 
        //by the ram_mu, so we just call steal the memory from the ram 
        if(paddr == 0){
                if(ram_mu_boost==0){
                                spinlock_acquire(&stealmem_lock);
                                paddr = ram_stealmem(pages);
                                spinlock_release(&stealmem_lock);
                }

        }


        return PADDR_TO_KVADDR(paddr);
        
}

void free_ker_pages(vaddr_t addr)
{
        //PADDR_TO_KVADDR(paddr) ((paddr)+MIPS_KSEG0)
        //we transfer the kernel virtual address  to the physical address 
        paddr_t paddr = addr - MIPS_KSEG0;
        if(ram_mu_boost){
                long start_index  =  paddr / 4096;
                long length = ram_mu[start_index].size;
                spinlock_acquire(&ram_mu_lock);
                for(long i= start_index; i < length+start_index; i++){
                        ram_mu[i].as = NULL;
                        ram_mu[i].virtual_addr = 0;
                        ram_mu[i].free = 1;
                }
                ram_mu[start_index].size = 0;
                spinlock_release(&ram_mu_lock);
        }
}

