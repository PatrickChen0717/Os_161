#include <types.h>
#include <lib.h>
#include <limits.h>
#include <synch.h>
#include <vnode.h>
#include <proctable.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <current.h>
#include <kern/limits.h>


//create the process table lock
struct lock* proctable_lock;
//create the process table
struct procinfo * proc_table[PID_MAX+1];
//the current number of process in the porcess table
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
    for(int i=PID_MIN;i<PID_MAX+1;i++){  
        proc_table[i] = NULL;
    }

    //set the proc_table[1] to be the kernel process and the kernel has no parent process 
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

    //set the pid and parent pid and the current status of the process
    new_p->pid=pid;
    new_p->p_pid=p_pid;
    new_p->proc_status=Ready;
    new_p->an_Orphan=0;
    return new_p;

 }

int 
proctable_add (int *retval)
{
    struct procinfo* new_proc;

    lock_acquire(proctable_lock);

    //if the process table has already has the max number of process 
    if(cur_procnum==PID_MAX){
        lock_release(proctable_lock);
        return ENPROC;
    }
    //create the a procinfo structure 
    new_proc=procinfo_create(next_pid,curproc->pid);

    if(new_proc==NULL){
        lock_release(proctable_lock);
        return ENOMEM;
    }

    //add the new procinfo into assigned proc_table entry
    proc_table[next_pid]=new_proc;
    cur_procnum++;

    //return the pid
    *retval=next_pid;

    //if we at the end of the process , we need to loop back to find a empty space
    if(next_pid==PID_MAX){
        next_pid=PID_MIN;
    }
    //make the next_pid to be an empty space when used by next time 
    for(int i=next_pid;i<PID_MAX+1;i++){
        if(proc_table[i]==NULL){
                next_pid=i;
                break;
        }
    }
    lock_release(proctable_lock);
    return 0;
}


void
proctable_remove(pid_t pid)
{
    KASSERT(pid!=1);
    KASSERT(pid!=0);
    //decrease the process num 
    cur_procnum--;
    proctable_free(proc_table[pid]);

}
void
proctable_free(struct procinfo*proc)
{
    KASSERT(proc!=NULL);

    // free the memory of that procinfo
    cv_destroy(proc->proc_cv);
    proc_table[proc->pid]=NULL;
    kfree(proc);
    
}


void
proctable_exit(struct proc*proc,int exitcode)
{
    struct procinfo* cur_procinfo;
    lock_acquire(proctable_lock);
    //reap all the zombie children ,a zombie child means that it has already exited but the parent does not know 
    for(int i=PID_MIN;i<PID_MAX+1;i++){
        if(proc_table[i]!=NULL){
            //find the child 
            if(proc_table[i]->p_pid == proc->pid){
                //make it child to be an orphan
                proc_table[i]->an_Orphan=1;
                //the child process has alredy exited we need romove it for the proctable
                if(proc_table[i]->proc_status==Zombie){
                    int pid=proc_table[i]->pid;
                    proctable_remove(pid);
                }
            }
        }
    }
    
    //set the exit status
    cur_procinfo=proc_table[proc->pid];
    cur_procinfo->proc_status=Zombie;
    cur_procinfo->exitcode=exitcode;

    if(cur_procinfo->an_Orphan){
        // the current process is an Orphan so we do not need to broadcast just make the entry in the proctable to be null
        proctable_remove(cur_procinfo->pid);
    }
    else{
        // tell the parent  it going to exit
        cv_broadcast(cur_procinfo->proc_cv,proctable_lock);
    }
    lock_release(proctable_lock);
    //remove the current thread from  process
    proc_remthread(curthread);
    //destroy the proc
    proc_destroy(proc);
    //exit the thread 
    thread_exit_new();

}
int
proctable_waitpid(pid_t pid,int options,int *exitstatus,int *retval)
{
    struct procinfo*wait_proc;
    int found;

    //check pid validity
    if(pid==curproc->pid){
        return ECHILD;
    }

    //check options validity
    if(options!=0){
        return EINVAL;
    }

    lock_acquire(proctable_lock);

    //find the proc with the pid in proc_table
    found=0;
    for(int i=PID_MIN;i<PID_MAX+1;i++){
        if(proc_table[i]!=NULL){
            if(proc_table[i]->pid==pid){
                wait_proc = proc_table[i];
                found=1;
                break;
            }
        }
    }

    //pid not found in the proctable
    if(!found){
        lock_release(proctable_lock);
        return ESRCH;
    }

    //the pid is not a child of caller of waitpid
    if(curproc->pid!=wait_proc->p_pid){
        lock_release(proctable_lock);
        return ECHILD;

    }
    
    //when proc_status is still active, wait on cv
    if(wait_proc->proc_status!=Zombie){
        cv_wait(wait_proc->proc_cv,proctable_lock);
    }

    //copy the exitcode for return output
    if(exitstatus!=NULL){
        *exitstatus=wait_proc->exitcode;
    }

    KASSERT(pid==wait_proc->pid);
    //return the pid
    *retval=pid;
    //remove the pid from the process table 
    proctable_remove(pid);
    lock_release(proctable_lock);

    return 0;
}