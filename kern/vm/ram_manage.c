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
        //calculate how many pages we have in total
        unsigned long total_size = ram_getsize();
        total_pages= total_size / PAGE_SIZE; 

        ram_mu = kmalloc(total_pages * sizeof(struct ram_chunk));

        long after = ram_getfirstfree()/PAGE_SIZE;

        spinlock_acquire(&ram_mu_lock);
        //mark all the pages before we malloc our ram_mu as not free
        for( long i = 0; i<after+1; i++){
                ram_mu[i].free = 0;
                ram_mu[i].size = 0;
        }
        //mark all the pages after we malloc our ram_mu as free
        for(long i = after+1;i<total_pages;i++){
                ram_mu[i].free = 1;
                ram_mu[i].size = 0;
        }
        
        ram_mu_boost = 1;
        spinlock_release(&ram_mu_lock);

}
/*when the kernel called kmalloc we  try to find some 
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
                                /*we need to find the  npages continuous
                                 *free pages.The if below check whether we 
                                 *can find n continuous pages.
                                 */
                                if(i!=0){
                                        if(!ram_mu[i-1].free){
                                                free_counter = 1;
                                        }
                                }
                                if(free_counter == npages){
                                        start_index=i-npages+1;
                                        //calculate the physical address
                                        addr = (paddr_t) (start_index*PAGE_SIZE);
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

                }
                spinlock_release(&ram_mu_lock);
        }
        return addr;

}


vaddr_t alloc_ker_pages(unsigned long npages)
{
        paddr_t paddr = 0;
        unsigned long pages= npages;
        //try get the free pages 
        paddr = k_try_free_pages(pages);
        /* if we can not find the free pages  and if we have not set up the ram management unit yet
         * we have to call stealmem to get the memory.If we already set up our ram management unit
         * we can not call the steal mem to get memory anymore ,cause we call the ram_getfirstfree()
         * when we set up our ram management unit
         */
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
        //if we have setup the ram management unit we need to mark the page to be free in out ram_mu
        //otherwise we do not have to do so
        if(ram_mu_boost){
                //get the start index in our ram_mu
                long start_index  =  paddr / PAGE_SIZE;
                long length = ram_mu[start_index].size;
                spinlock_acquire(&ram_mu_lock);
                //mark them to be free
                for(long i= start_index; i < length+start_index; i++){
                        ram_mu[i].free = 1;
                }
                ram_mu[start_index].size = 0;
                spinlock_release(&ram_mu_lock);
        }
}

