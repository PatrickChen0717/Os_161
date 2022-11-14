
#include <types.h>
#include <spl.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <copyinout.h>
#include <syscall.h>
#include <vnode.h>
#include <filetable.h>
#include <proctable.h>
#include <proc.h>
#include <synch.h>
#include <kern/errno.h>
#include <mips/trapframe.h>
#include <execv.h>
#include <kern/limits.h>
#include <kern/wait.h>



int
sys_getpid(pid_t *retval){
    //return curproc pid
    *retval = curproc->pid;

    return 0;
}


int sys_fork(struct trapframe *tf, pid_t *retval){
    struct proc *child_proc;

    //create a new process
    child_proc = proc_create_new(curproc->p_name);
    if(child_proc==NULL){
        return ENOMEM;
    }

    //add the new child process into proctable
    int ret;
    ret = proctable_add(&(child_proc->pid));
    if(ret){
        //on err, destroy the newproc
        proc_destroy(child_proc);
        return ret;
    }

    //copy addressspace of parent to child
    ret = as_copy(curproc->p_addrspace, &child_proc->p_addrspace);
    if(ret){
        //on err, destroy the newproc and remove from protable
        proctable_remove(child_proc->pid);
        proc_destroy(child_proc);
        return ret;
    }

    //copy current working directory of parent to child, and increment ref count
    spinlock_acquire(&curproc->p_lock);
    child_proc->p_cwd = curproc->p_cwd;
    VOP_INCREF(curproc->p_cwd);
    spinlock_release(&curproc->p_lock);

    //copy filetable
    lock_acquire(curproc->p_ft->lock);
    ret = filetable_copy (curproc->p_ft, &(child_proc->p_ft));
    lock_release(curproc->p_ft->lock);
    if(ret){
        //on err, destroy the newproc, destroy the new addresspace, remove from protable
        as_destroy(child_proc->p_addrspace);
        proctable_remove(child_proc->pid);
        proc_destroy(child_proc);
        return ret;
    }

    //set up trapframe for child process
    struct trapframe *child_tf;
    child_tf = kmalloc(sizeof(struct trapframe));
    if (child_tf == NULL) {
        //on err, destroy the newproc, destroy the new addresspace, remove from protable
        as_destroy(child_proc->p_addrspace);
        proctable_remove(child_proc->pid);
        proc_destroy(child_proc);
		return ENOMEM;
	}

    //copy the content of tf into child_tf
    memcpy(child_tf, tf, sizeof(struct trapframe));

    child_tf->tf_v0 = 0;  //return 0 for child
    child_tf->tf_a3 = 0;
    child_tf->tf_epc += 4;  // increment PC by 4

    //fork a new kernel thread for child proc
    ret = thread_fork("child", child_proc, switch_usermode, child_tf, 0);
    if(ret){
        //on err, destroy the newproc, destroy the new addresspace, remove from protable, free child tf
        as_destroy(child_proc->p_addrspace);
        proctable_remove(child_proc->pid);
        proc_destroy(child_proc);
        kfree(child_tf);
        return ret;
    }

    //return child pid
    *retval = child_proc->pid;

    
    return 0;
}

int
sys_waitpid(pid_t pid, userptr_t status, int options, pid_t *retval){

    int eixtstatus;
    int err;

    //call waitpid helper
    err=proctable_waitpid(pid,options,&eixtstatus,retval);
    if(err){
        return err;
    }
    
    //copy the exitstatus for return
    if(status!=NULL){
        err=copyout(&eixtstatus,status,sizeof(int));
        if(err){    
            return err;
        }
    }
    return 0;

}

void
sys__exit(int exitcode){
    
    proctable_exit(curproc,_MKWAIT_EXIT(exitcode));
    kprintf("Exit\n");
}


int
sys_execv(userptr_t name,userptr_t args)
{
    int ret;
    int arg_len;
    char *program_name;
    struct execv_store* exe;

    //create a storage structure for kernel args
    exe=kmalloc(sizeof(struct execv_store));
    if(exe==NULL){
        return ENOMEM;
    }
    
    //initialize storage structure
    init(exe);

    exe->kernel_data=kmalloc(__ARG_MAX);

    if(exe->kernel_data==NULL){
        panic("exe kernel data alloc fail");
        return ENOMEM;
    }
    

    program_name = kmalloc(PATH_MAX);
    if(program_name==NULL){
        kfree(exe->kernel_data);
        kfree(exe);
        return ENOMEM;
    }
    
    //copy the program name pointer into kernel
    ret = copyinstr(name, program_name, PATH_MAX, NULL);
    if(ret){

        kfree(exe->kernel_data);
        kfree(program_name);
        kfree(exe);
        return ret;
    }

    //copy the argument from userspace into kernal, store inside exe struct
    ret = copy_args(exe,args);
    if(ret){
        kfree(exe->kernel_data);
        kfree(exe);

        kfree(program_name);
        return ret;
    }
    
    struct vnode *v;

    //open program file as read only into vnode
    ret = vfs_open(program_name, O_RDONLY, 0, &v);
	if (ret) {
        kfree(program_name);

        //clean up exe storage
        kfree(exe->kernel_data);
        kfree(exe);

		return ret;
	}

    //copy a new addresspace;
    vaddr_t entrypoint;
    struct addrspace *cur_as = proc_getas();
    struct addrspace *new_as;

    //create an empty addresspace
    new_as = as_create();
	if (new_as == NULL) {
        kfree(program_name);
		vfs_close(v);

        //clean up exe storage
        kfree(exe->kernel_data);
        kfree(exe);

		return ENOMEM;
	}

    //set new as to cur as
    proc_setas(new_as);
	as_activate();

    //load the ELF executable
    ret = load_elf(v, &entrypoint);
    if (ret) {
        kfree(program_name);
        vfs_close(v);

        //clean up exe storage
        kfree(exe->kernel_data);
        kfree(exe);

        //load_elf failed, reset as
        proc_setas(cur_as);
        as_activate();
        as_destroy(new_as);
		return ret;
    }

    //define a new stack
    vaddr_t stackptr;
    ret = as_define_stack(new_as, &stackptr);
    if (ret) {
        kfree(program_name);
        vfs_close(v);

        //clean up exe storage
        kfree(exe->kernel_data);
        kfree(exe);

        //switch back addresspace
        proc_setas(cur_as);
        as_activate();
        as_destroy(new_as);
		return ret;
    }


    //copy args to the new user address space
    ret=copy_args_out(exe,&stackptr,&args);
    if (ret) {
        kfree(program_name);
        vfs_close(v);

        //clean up exe storage
        kfree(exe->kernel_data);
        kfree(exe);

        //switch back addresspace
        proc_setas(cur_as);
        as_activate();
        as_destroy(new_as);
        return ret;
    }
    

    //copy out the length of content
    arg_len=exe->args_count;

    //free old addresspace
    as_destroy(cur_as);
    kfree(program_name);
    kfree(exe->kernel_data);
    kfree(exe);

    vfs_close(v);

    /* Warp to user mode. */
	enter_new_process(arg_len, args,
			  NULL /*userspace addr of environment*/,
			  stackptr, entrypoint);
              
	return EINVAL;
}


/**
 * switch the process to user mode, copy trapframe to stack
 */
void
switch_usermode(void *p, unsigned long arg){
    (void) arg;

    struct trapframe user_tf;
	struct trapframe *new_tf = p;

	user_tf = *new_tf;
	kfree(new_tf);

	mips_usermode(&user_tf);
}

