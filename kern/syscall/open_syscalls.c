#include <types.h>
#include <copyinout.h>
#include <syscall.h>
#include <vnode.h>
#include <filetable.h>
#include <vfs.h>
#include <openfile.h>
#include <limits.h>
#include <current.h>
#include <proc.h>
#include <uio.h>
#include <spinlock.h>
#include <kern/errno.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <kern/fcntl.h>



/*
 * Open a file with the provide path,and openflags  
 * and mode and .return the the filedescriptor if 
 * open is successful.
 * 
 */
int
sys_open(const_userptr_t userpath, int openflags,mode_t mode,int *retv){
    int err;
    char *path;
    struct openfile *file;
    const int setflag=O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_APPEND | O_NOCTTY;
    path=kmalloc(PATH_MAX);

    if(path==NULL){
      return ENOMEM;
    }
    
    if((openflags & setflag)!=openflags ){
      kfree(path);
      *retv=-1;
      return EINVAL;
    }
    
    err=copyinstr(userpath,path,PATH_MAX,NULL);
    if(err){
      *retv=-1;
      kfree(path);
      return err;
    }
    err=open_new_file(path,openflags,mode,&file);
    if(err){
      kfree(path);
      return err;
    }
    kfree(path);

    lock_acquire(curproc->p_ft->lock);
    err=filetable_add(curproc->p_ft,file);
    if(err<0){
      lock_release(curproc->p_ft->lock);
      return EMFILE;
    }
    *retv = err;
    lock_release(curproc->p_ft->lock);

    return 0;//the sys call is done 

}


int 
sys_close(int fd){
    struct filetable*ft;
    ft=curproc->p_ft;

    if(fd<0 || fd>=OPEN_MAX){
      return EBADF;
    }

    lock_acquire(ft->lock);
    if(ft->table[fd]==NULL){
      lock_release(ft->lock);
      return EBADF;
    }

    count_decrea(ft->table[fd]);
    ft->table[fd]=NULL;
    lock_release(ft->lock);
    return 0;
}


int
sys_read(int fd, userptr_t buf, size_t buflen, int *ret){
    struct uio u;
    struct iovec iov;
    struct openfile *file;
    struct filetable *ft;
    struct addrspace *procaddr;
    int err;
    size_t readsize;
    procaddr=curproc->p_addrspace;
    ft=curproc->p_ft;

    if(fd >=OPEN_MAX || fd<0){
      return EBADF;
    }

    file=ft->table[fd];
    if(file == NULL){
      return EBADF;
    }

    lock_acquire(file->lock_file);
    if(file->flag & O_WRONLY ){
      lock_release(file->lock_file);
      return EBADF;
    }

    //set up read buffer
    uio_init_user(&iov, &u, buf, buflen,file->offset, UIO_READ,procaddr);

    //read vnode into buffer
    err=VOP_READ(file->v_file, &u);
    if(err){
      lock_release(file->lock_file);
      return err;
    }
    readsize = buflen - u.uio_resid;
    file->offset=file->offset+(off_t)readsize;
    lock_release(file->lock_file);

    *ret =  (int)readsize;

    return 0;
}

//writes up to buflen bytes to the file 
int
sys_write(int fd, userptr_t buf, size_t nbytes, int *ret){
    struct uio u;
    struct iovec iov;
    struct openfile *file;
    struct filetable *ft;
    struct addrspace *procaddr;
    int err;
    size_t writelen;
    procaddr=curproc->p_addrspace;

    ft=curproc->p_ft;
    if(fd >= OPEN_MAX || fd<0){
      return EBADF;
    }

    file=ft->table[fd];
    if(file == NULL){
      return EBADF;
    }

    lock_acquire(file->lock_file);
    if(!(file->flag & (O_WRONLY | O_RDWR))){
      lock_release(file->lock_file);
      return EBADF;
    }

    //set up write buffer
    uio_init_user(&iov, &u, buf, nbytes,file->offset, UIO_WRITE,procaddr);

    err = VOP_WRITE(file->v_file,&u);
    if(err){
      lock_release(file->lock_file);
      return err;
    }
    writelen = nbytes - u.uio_resid;
    file->offset = file->offset+(off_t) writelen ;
    lock_release(file->lock_file);
    
    *ret = (int)writelen;

    return 0;
}




int
sys_lseek(int fd, off_t pos, int whence, off_t *ret){
    struct openfile *file;
    off_t new_pos;

    //fd is not a valid file handle
    if(fd >= OPEN_MAX || fd<0){
      return EBADF;
    }

    file = curproc->p_ft->table[fd];

    if(file == NULL){
      return EBADF;
    }

    //fd refers to an object which does not support seeking
    if(VOP_ISSEEKABLE(file->v_file)==0){
      return ESPIPE; 
    }

    lock_acquire(file->lock_file);
    if(whence == SEEK_SET){
      new_pos = pos;
    }
    else if(whence == SEEK_CUR){
      new_pos = file->offset + pos;
    }
    else if(whence == SEEK_END){
      struct stat file_stat;

      VOP_STAT(file->v_file, &file_stat);
      new_pos = file_stat.st_size + pos;
    }
    else{
      lock_release(file->lock_file);
      return EINVAL;
    }

    if(new_pos<0){
      lock_release(file->lock_file);
      return EINVAL;    
    }
    
    file->offset = new_pos;
    *ret = new_pos;
    lock_release(file->lock_file);

    return 0;
}


int
sys_chdir(const_userptr_t pathname){
  int err;
  int result;
  char *path = kmalloc(PATH_MAX);
  
  err = copyinstr(pathname, path, PATH_MAX, NULL);
  if(err){
    kfree(path);
    return err;
  }

  result = vfs_chdir(path);

  kfree(path);
  return result;
}

int
sys__getcwd(const_userptr_t buf, size_t buflen, int *retval){
    int result;
    struct uio u;
    struct iovec iov;
    struct addrspace *procaddr;

    procaddr = curproc->p_addrspace;

    uio_init_user(&iov, &u, (userptr_t)buf, buflen, 0, UIO_READ, procaddr);

    result = vfs_getcwd(&u);
    if (result) {
      return result;
    }

    *retval = buflen - u.uio_resid;

    return 0;
}

int
sys_dup2(int oldfd,int newfd,int *retval){
    struct openfile*file1;
    struct openfile*file2;
    struct filetable *ft;
    if(oldfd>=OPEN_MAX || newfd>=OPEN_MAX || oldfd<0 || newfd<0){
        return EBADF;
    }
    ft=curproc->p_ft;

    lock_acquire(ft->lock);
    file1=ft->table[oldfd];
    if(file1== NULL){
      lock_release(ft->lock);
      return EBADF;
    }
    if(oldfd==newfd){ // if the oldfd and newfd are the same we need do nothing and the syscall will be successful atomatically and we just return the newfd 
      lock_release(ft->lock);
      *retval=newfd;
      return 0;
    }

    file2=ft->table[newfd];
    if(file2!=NULL){
      count_decrea(file2);
    }
    ft->table[newfd]=NULL;
    ft->table[newfd]=file1;
    count_inc(file1);
    *retval=newfd;
    lock_release(ft->lock);

    return 0;
}