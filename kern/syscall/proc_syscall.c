
#include <types.h>
#include <spl.h>
#include <copyinout.h>
#include <syscall.h>
#include <vnode.h>
#include <filetable.h>
#include <proctable.h>
#include <proc.h>
#include <synch.h>
#include <kern/errno.h>
#include <mips/trapframe.h>



int
sys_getpid(pid_t *retval){
    *retval = curproc->pid;

    return 0;
}


int sys_fork(struct trapframe *tf, pid_t *retval){
    struct proc *child_proc;

    child_proc = proc_create_new(curproc->p_name);
    if(child_proc==NULL){
        return ENOMEM;
    }


    int ret;
    ret = proctable_add(&child_proc->pid);
    if(ret){
        proc_destroy(child_proc);
        return ret;
    }

    //copy addressspace of parent to child
    ret = as_copy(curproc->p_addrspace, &child_proc->p_addrspace);
    if(ret){
        proctable_remove(child_proc->pid);
        proc_destroy(child_proc);
        return ret;
    }

    //copy current working directory of parent to child
    spinlock_acquire(&child_proc->p_lock);
    child_proc->p_cwd = curproc->p_cwd;
    VOP_INCREF(curproc->p_cwd);
    spinlock_release(&child_proc->p_lock);

    //copy filetable
    lock_acquire(curproc->p_ft->lock);
    filetable_copy (curproc->p_ft, &(child_proc->p_ft));
    lock_release(curproc->p_ft->lock);

    //set up trapframe for child process
    struct trapframe *child_tf;
    child_tf = kmalloc(sizeof(struct trapframe));
    if (child_tf == NULL) {
		return ENOMEM;
	}

    memcpy(child_tf, tf, sizeof(struct trapframe));

    child_tf->tf_v0 = 0;  //return 0 for child
    child_tf->tf_a3 = 0;
    child_tf->tf_epc += 4;

    ret = thread_fork("child", child_proc, switch_usermode, child_tf, 0);
    if(ret){
        proctable_remove(child_proc->pid);
        proc_destroy(child_proc);
        kfree(child_tf);
        return ENOMEM;
    }

    //return child pid
    *retval = child_proc->pid;

    
    return 0;
}

int
sys_waitpid(pid_t pid, userptr_t status, int options, pid_t *retval){
    int ret;
    if(options != 0){
        return EINVAL;
    }

    //locate the proc with pid inside the proctable
    struct procinfo * wait_proc;
    bool found;

    if(pid==curproc->pid){
        return EINVAL;
    }

    lock_acquire(proctable_lock);
    found=false;
    
    for(int i=2;i<PID_MAX+1;i++){
        if(pid == proc_table[i]->pid && proc_table[i]->p_pid == curproc->pid){
            wait_proc = proc_table[i];
            found=true;
            break;
        }
    }

    if(!found){
        lock_release(proctable_lock);
        return ESRCH;
    }
    else if(wait_proc->p_pid != curproc->pid){
        lock_release(proctable_lock);
        return ECHILD;
    }
    

    while(wait_proc->proc_status != Zombie){
        cv_wait(wait_proc->proc_cv, proctable_lock);
    }
    lock_release(proctable_lock);

    ret = copyout(&wait_proc->exitcode, status, sizeof(int));
    //*status = wait_proc->exitcode;

    *retval = wait_proc->pid;


    return ret;
}

void
sys__exit(int exitcode){
    struct procinfo * exit_proc;

    lock_acquire(proctable_lock);
    for(int i=0;i<PID_MAX+1;i++){
        if(curproc->pid == proc_table[i]->pid){
            exit_proc = proc_table[i];
            break;
        }
    }
    
    for(int i=0;i<PID_MAX+1;i++){
        if(curproc->pid == proc_table[i]->p_pid ){
            if(proc_table[i]->proc_status != Zombie){
                proc_table[i]->p_pid = 0;
                proc_table[i]->proc_status = Orphan;
            }
        }
        
    }

    if(exit_proc->proc_status != Zombie){
        if(exit_proc->proc_status != Orphan){
            cv_broadcast(exit_proc->proc_cv, proctable_lock);
            exit_proc->proc_status = Zombie;
            exit_proc->exitcode = exitcode;
        }
        else{
            proctable_remove(exit_proc->pid);
            proc_destroy(curproc);
        }
    }
    // lock_release(proctable_lock);

    // //change all child to orphan
    // lock_acquire(proctable_lock);

    lock_release(proctable_lock);
    
    //proc_remthread(curthread);
    
    thread_exit_new();
}


// int
// sys_execv(const char *program, char **args){
//     int ret;

//     char* program_name = kmalloc(PATH_MAX);
//     ret = copyinstr((const_userptr_t) program, program_name, PATH_MAX, NULL);

//     int arg_copy;
//     ret = copy_arg(args,&arg_copy);

// }

void
switch_usermode(void *p, unsigned long arg){
    (void) arg;
	void *tf;
    tf = (void *) curthread->t_stack + 16;

	memcpy(tf, p, sizeof(struct trapframe));
	kfree(p);

	as_activate();
	mips_usermode(tf);
}

// int
// copy_arg(char **args, int *arg_ret){
    
// }