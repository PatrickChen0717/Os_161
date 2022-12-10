#ifndef _RAM_MANAGE_H_
#define _RAM_MANAGE_H_



#include <addrspace.h>
#include <types.h>


#define KUSE 0  // the current page is used by the kenerl
#define UUSE 1  // the current page is used by the user process 
#define UNkNOW 2 // unknow status which is we do not know whethere is used by the kernel or the user process 

/*
 *A data structure that used to keep track the state of the physical page.

 * First it has the state of the the current chunk.

 * It contains  the information of the  address information of the Onweer,
 * both physicall addrress and the virtual address.
 * 
 * 
 * And the size of the current chunk.
 * 
 * 
 * 
 */

struct ram_chunk{
        //status of this chunk , used by the kenerl or user process 
        //int status;

        //whether the current chunk is free or not 
        int free;

        // address of the owner of this chunk
        struct addrspace *as;// the actual address assigned to this chunk
        vaddr_t virtual_addr; // the virtual address that mapped by the physical page

        size_t size;  //how many we ram we used for this chunk.

};


void ram_manage_init(void);


vaddr_t alloc_ker_pages(unsigned long npages);

/*free the physical pages used by the kernel */
/* copied from the dumbvm.c*/
void free_ker_pages(vaddr_t addr);

#endif 


