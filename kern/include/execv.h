#ifndef _execv_H_
#define _execv_H_

#include<types.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <kern/limits.h>

/*define a data structure which is used to store 
 * the information of the arguments when copin from 
 *the userspace to the kernel space and copyout from 
 *the kernel space to the user space.The main idea is
 *to avoid put many things on the stack. So we just make 
 *a struct and put those information on the heap.
 */
struct execv_store{
    char *kernel_data;
    size_t args_len; // the length of the content
    int args_count; //number args
};

//init this structure
void init(struct execv_store * exe);

//a helper function for the copy the arguments from the userspace to the kernel space 
int copy_args(struct execv_store* exe,userptr_t args);

//a helper function for the copy the arguments from the kernel space  to the user space 
int copy_args_out(struct execv_store* exe,vaddr_t *sp,userptr_t *args_start);




#endif