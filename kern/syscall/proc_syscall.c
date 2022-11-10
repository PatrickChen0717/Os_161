#include <types.h>
#include <copyinout.h>
#include <syscall.h>
#include <vnode.h>
#include <filetable.h>
#include <proctable.h>
#include <proc.h>


int
sys_getpid(pid_t *retval){
    lock_acquire(proctable_lock);
    *retval = //pid;
    lock_release(proctable_lock);

    return 0;
}


int sys_fork(struct trapfram *tf, pid_t *retval){
    struct proc *child_proc;

    child_proc = proc_create("Child_proc");
    if(child_proc==NULL){
        return ENOMEM;
    }

    int ret;
    ret = proctable_add(child_proc);
    if(ret){
        proc_destroy(child_proc);
        return ret;
    }

    //copy addressspace of parent to child
    ret = as_copy(curproc->p_addrspace, &child_proc->p_addrspace);
    if(ret){
        proctable_remove(child_proc);
        proc_destroy(child_proc);
        return ret;
    }

    //copy current working directory of parent to child
    child_proc->p_cwd = curproc->p_cwd;
    VOP_INCREF(curproc->p_cwd);

    //copy filetable
    lock_aquire(curproc->p_ft->lock);
    filetable_copy (curproc->p_ft, child_proc->p_ft);
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


    //Todo: Generate pid for child
    *retval = //childpid;

    return 0;
}

int
sys_waitpid(pid_t pid, int options){
    if(options != 0){
        return EINVAL;
    }

    

    while()


}