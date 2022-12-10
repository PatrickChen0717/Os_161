#ifndef _RAM_MANAGE_H_
#define _RAM_MANAGE_H_


#include <types.h>

/*
 *A data structure that used to keep track the state of the physical page.

 * First it has the state of the the current chunk.
 * And the size of the current chunk.
 */

struct ram_chunk{
        //whether the current chunk is free or not 
        int free;
        size_t size;  //how many we ram we used for this chunk.
};

/*initailze ram management unit*/
void ram_manage_init(void);

/*alloc continuous pages for the kenerl , for kmalloc*/
vaddr_t alloc_ker_pages(unsigned long npages);

/*free the physical pages used by the kernel */
void free_ker_pages(vaddr_t addr);


#endif 


