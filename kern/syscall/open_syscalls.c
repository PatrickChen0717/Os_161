#include <types.h>
#include <copyinout.h>
#include <syscall.h>
#include <vnode.h>
#include <filetable.h>
#include <current.h>
#include <limits.h>
#include <vfs.h>
#include <openfile.h>
#include <uio.h>

int
sys_open(userptr_t userpath, int openflags, mode_t mode, int *retv){
    int err;
    char *path;
    path = kmalloc(__PATH_MAX);
    struct openfile **file;
    

    err = copyinstr(userpath,path, __PATH_MAX, NULL);
    if(err == 1){
      return 1;
    }

    err = open_new(path, openflags, mode, file);
    if(err){
      kprintf("open file failed");
      return 1;
    }

    *retv = filetable_add (curproc->p_ft, *file);
    return 0;//the sys call si done 

}

int
sys_close(int ft_index){
  
  if(ft_index >= OPEN_MAX){
    kprintf("Sys_close: file_index out of bound");
    return -1;
  }

  if(curproc->p_ft->table[ft_index]->v_file != NULL){
    vfs_close(curproc->p_ft->table[ft_index]->v_file);

    count_decrea(curproc->p_ft->table[ft_index]);

    filetable_remove (curproc->p_ft, ft_index);

    return 0;
  }
  
  kprintf("Sys_close: file does not exit");
  return -1;
  
  
}


//read data from the file descriptor
int
sys_read(int ft_index, userptr_t buf, size_t buflen, int *ret){
    struct uio u;
    struct iovec user_iov;

    if(ft_index >= OPEN_MAX){
      kprintf("sys_read: file_index out of bound");
      return -1;
    }

    //set up read buffer
    uio_kinit(&user_iov, &u, &buf, buflen, 0, UIO_READ);

    //read vnode into buffer
    VOP_READ(curproc->p_ft->table[ft_index]->v_file, &u);

    *ret =  buflen - u.uio_resid;
    return 1;
}

//writes up to buflen bytes to the file 
int
sys_write(int ft_index, userptr_t buf, size_t nbytes, int *ret){
    struct uio u;
    struct iovec user_iov;

    if(ft_index >= OPEN_MAX){
      kprintf("sys_write: file_index out of bound");
      return -1;
    }

    //set up write buffer
    uio_kinit(&user_iov, &u, &buf, buflen, 0, UIO_WRITE);

    VOP_WRITE(curproc->p_ft->table[ft_index]->v_file, &u);

    *ret =  nbytes - u.uio_resid;
    return 1;
}