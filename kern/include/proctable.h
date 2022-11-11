#ifndef _PROCTABLE_H_
#define _PROCTABLE_H_

#include <types.h>
#include <spl.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <limits.h>


struct procinfo
{
    pid_t pid;//process id 
    pid_t p_pid//parent process id . when in the wait we shoudl check the waiting pid whether is a child of the caller of wait
    struct cv *proc_cv;
};

/*Initialized the process table only called once in the main program*/
void proctable_init(void);

/*Create a procinfo type, one argrument is the pid another one is the parent pid
 * 
 *  
 */
struct procinfo* procinfo_create(pid_t pid,pid_t p_pid);

/*When we forked a new process ,this function should be called,to get the an unuesed and new pid
 * the pid will be stored in the arguments retval.
 *If error , erron number will be returned by this function.
 *0 will be returned if there is no error
 */
int proctable_add(int *retval);

/*When a process is exited , this function will be called.
 * Remove the exited process info from the proctable and free the relevent memory
 */
void proctable_free(struct procinfo* procinfo);

#endif /*_PROCTABLE_H_*/