#include <types.h>
#include <lib.h>
#include <limits.h>
#include <synch.h>
#include <vnode.h>
#include <proctable.h>
#include <kern/fcntl.h>
#include <kern/errno.h>

/*
 * create an empty filetable with OPEN_MAX entries
 */


struct procinfo * proc_table[PID_MAX+1];
struct lock* proctable_lock;
int cur_procnum;
pid_t next_pid; //do not need to search the whole array to find next avalibale 

void
proctable_create (void){
    struct procinfo* kenerl_proc;
    

    //create the proctable lock
    proctable_lock = lock_create("pt_lock");
    if(proctable_lock==NULL){
        panic("No memory for create a lock for the process table");
    }
    //initialize all the enties of process table to be NULL
    for(int i=0;i<PID_MAX+1;i++){  
        proc_table[i] = NULL;
    }

    //set the proc_table[1] to be the kernel process 
    kenerl_proc=procinfo_create(curproc);
    if(kenerl_proc==NULL){
        panic("Creating the kenerl procinfo failed");
    }
    proc_table[1]=kenerl_proc;
    curproc->p_pid=1; //the kenerl has the pid1
    cur_procnum=1; //number of the process is 1 we only create the kenerl process 
    next_pid=PID_MIN; //the next pid will start at PID_MIN which is 2
}

void 
proctable_delete (void){
    for(int i=0; i<PID_MAX; i++){
        //remove reference
        proc_table[i] = NULL;
    }

    //free memory
    lock_destroy(proctable_lock);
    kfree(proc_table);
}

int 
proctable_add (struct proc *kproc,pid_t *retval){
    struct procinfo* new_proc;
    lock_acquire(proctable_lock);

    if(cur_procnum==PID_MAX){
        lock_release(proctable_lock);
        return ENPROC;
    }
    new_proc=procinfo_create(kproc);

    if(new_proc==NULL){
        lock_release(proctable_lock);
        return ENOMEM;
    }

    proc_table[next_pid]=new_proc;
    kproc->p_pid=next_pid;
    &retval=next_pid;

    next_pid=next_pid+1;//increase the next_pid 
    lock_release(proctable_lock);
    return 0;
}

int 
proctable_remove (struct proc *kproc_to_delete){
     //check if fd is valid
    for(int i=0;i<PID_MAX;i++){
        //check if the filetable index is empty
        if(proc_table[i]->kproc == kproc_to_delete){
            proc_table[i] = NULL;
            return 0;
        }
    }

    //return 1, for no file to remove
    return 1;
}

struct procinfo * procinfo_create(struct proc* new_proc){
    KASSERT(new_proc!=NULL);
    struct procinfo* new_p;
    new_p=kmalloc(sizeof(struct procinfo));
    if(new_p==NULL){
        return NULL;
    }
    new_p->kproc=new_proc;
    
    //create cv
    new_p->proc_cv = cv_create("proc_cv");

    return new_p;

 }