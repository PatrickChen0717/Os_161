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
    struct proc *kproc;
    struct cv *proc_cv;
    
    
};

/*create an empty process table with OPEN_MAX entries*/
void proctable_create (void);

/*delete the filetable and free memory*/
void proctable_delete (void);

/*add a new openfile into the filetable*/
int proctable_add (struct proc *kproc,pid_t *retval);

/*remove an existing file in the filetable*/
int proctable_remove (struct proc *kproc);

struct procinfo * procinfo_create(struct proc* new_proc);


#endif /*_PROCTABLE_H_*/