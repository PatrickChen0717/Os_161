#include <types.h>
#include <lib.h>
#include <limits.h>
#include <synch.h>
#include <vnode.h>
#include <proctable.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <current.h>

/*
 * create an empty filetable with OPEN_MAX entries
 */


struct procinfo * proc_table[PID_MAX+1];
struct lock* proctable_lock;
int cur_procnum;
pid_t next_pid; //do not need to search the whole array to find next avalibale 

void
proctable_init (void)
{
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
    kenerl_proc=procinfo_create(1,0);
    if(kenerl_proc==NULL){
        kfree(proctable_lock);
        panic("Creating the kenerl procinfo failed");
    }
    proc_table[1]=kenerl_proc;
    cur_procnum=1; //number of the process is 1 we only create the kenerl process 
    next_pid=PID_MIN; //the next pid will start at PID_MIN which is 2
}

struct procinfo* 
procinfo_create(pid_t pid,pid_t p_pid)
{

    struct procinfo* new_p;

    new_p=kmalloc(sizeof(struct procinfo));
    if(new_p==NULL){
        return NULL;
    }
    //create cv
    new_p->proc_cv = cv_create("proc_cv");
    if(new_p->proc_cv==NULL){
        kfree(new_p);
        return NULL;
    }
    new_p->pid=pid;
    new_p->p_pid=p_pid;
    return new_p;

 }

int 
proctable_add (int *retval)
{
    struct procinfo* new_proc;
    lock_acquire(proctable_lock);

    if(cur_procnum==PID_MAX){
        lock_release(proctable_lock);
        return ENPROC;
    }
    new_proc=procinfo_create(next_pid,curproc->pid);

    if(new_proc==NULL){
        lock_release(proctable_lock);
        return ENOMEM;
    }

    proc_table[next_pid]=new_proc;
    cur_procnum++;

    *retval=next_pid;

    if(next_pid==PID_MAX){
        next_pid=PID_MIN;
    }
    for(int i=next_pid;i<PID_MAX+1;i++){
        if(proctable[i]==NULL){
                next_pid=i;
                break;
        }
    }
    lock_release(proctable_lock);
    return 0;
}

void
proctable_free(struct procinfo*proc)
{
    KASSERT(proc!=NULL);
    cv_destroy(proc->proc_cv);
    proctable[proc->pid]=NULL;
    kfree(proc);
    
}

