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
 * open is successful.If not -1 will be store in the retv 
 * and the corresponding error number  will also be retured.
 * 
 */
int
sys_open(const_userptr_t userpath, int openflags,mode_t mode,int *retv){
    int err;
    char *path;
    struct openfile *file;

    /* Setflag to 127 because in the kern/fcntl.h we found that
     * flag for open can be 0 1 or 2 and bit wise or with any of the 
     * O_CREAT, O_EXCL ,O_TRUNC,O_APPEND ,O_NOCTTY , or bit wise OR with
     * them all together (equivalent to O_ACCMODE | O_CREAT | O_EXCL | 
     * O_TRUNC | O_APPEND | O_NOCTTY) which can check the input flag whether 
     * is in range. 
     */
    const int setflag = 127; 
    path = kmalloc(PATH_MAX);

    //err check Out of memory
    if(path == NULL){
      return ENOMEM;
    }

    //check whether the flag is valid, also can check is the flag is something larger than 127 or negative value 
    if(openflags > setflag || openflags < 0){
      kfree(path);
      *retv=-1;
      return EINVAL;
    }
    
    //copy userpath pointer and check err
    err=copyinstr(userpath,path,PATH_MAX,NULL);
    if(err){
      *retv=-1;
      kfree(path);
      return err;
    }

    //open file using the copied path and check err
    err=open_new_file(path,openflags,mode,&file);
    if(err){
      kfree(path);
      return err;
    }
    kfree(path);

    //atomic add new file into filetable
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

/* Close file handle fd
 * On success, close returns 0
 * On error,  and errno is retrurned.
 */
int 
sys_close(int fd){
    struct filetable*ft;
    ft=curproc->p_ft;

    //check if fd is a valid file handle
    if(fd<0 || fd>=OPEN_MAX){
      return EBADF;
    }


    lock_acquire(ft->lock);

    //check if the openfile exist on fd
    if(ft->table[fd] == NULL){
      lock_release(ft->lock);
      return EBADF;
    }

    //remove from filetable 
    filetable_remove(ft,fd);
    lock_release(ft->lock);

    return 0;
}

/* reads up to buflen bytes from the file specified by fd in the filetable
 * Return the count of bytes read
 */
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

    //check if fd is a valid file handle
    if(fd >= OPEN_MAX || fd<0){
      return EBADF;
    }

    //check if the openfile exist on fd
    file=ft->table[fd];
    if(file == NULL){
      return EBADF;
    }

    //check if the flag is O_RDONLY
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

    //calculate return length
    readsize = buflen - u.uio_resid;
    file->offset = file->offset + (off_t)readsize;
    lock_release(file->lock_file);

    *ret =  (int)readsize;

    return 0;
}

/* writes up to buflen bytes to the file 
 * Return the count of bytes written
 */
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
    
    //check if fd is a valid file handle
    ft=curproc->p_ft;
    if(fd >= OPEN_MAX || fd < 0){
      return EBADF;
    }

    //check if the openfile exist on fd
    file=ft->table[fd];
    if(file == NULL){
      return EBADF;
    }

    //check if the flag is O_RDONLY
    lock_acquire(file->lock_file);
    if((file->flag &O_ACCMODE) == O_RDONLY){
      lock_release(file->lock_file);
      return EBADF;
    }

    //set up write buffer
    uio_init_user(&iov, &u, buf, nbytes,file->offset, UIO_WRITE,procaddr);

    //write into buffer
    err = VOP_WRITE(file->v_file,&u);
    if(err){
      lock_release(file->lock_file);
      return err;
    }

    //calculate return length
    writelen = nbytes - u.uio_resid;
    file->offset = file->offset+(off_t) writelen ;
    lock_release(file->lock_file);
    

    *ret = (int)writelen;

    return 0;
}

/* lseek alters the current seek position of the file handle filehandle, 
 * seeking to a new position based on pos and whence.
 * On success, lseek returns the new position
 * On error, errno is set
 */
int
sys_lseek(int fd, off_t pos, int whence, off_t *ret){
    struct openfile *file;
    off_t new_pos;

    //fd is not a valid file handle
    if(fd >= OPEN_MAX || fd < 0)
    {
      return EBADF;
    }

    file = curproc->p_ft->table[fd];

    //check if the openfile exist on fd
    if(file == NULL)
    {
      return EBADF;
    }

    //fd refers to an object which does not support seeking
    if(VOP_ISSEEKABLE(file->v_file) == 0)
    {
      return ESPIPE; 
    }

    //calculate the new position: 3 cases
    lock_acquire(file->lock_file);
    if(whence == SEEK_SET)
    {
      new_pos = pos;
    }
    else if(whence == SEEK_CUR)
    {
      new_pos = file->offset + pos;
    }
    else if(whence == SEEK_END)
    {
      struct stat file_stat;

      //retrieve the end of the file
      VOP_STAT(file->v_file, &file_stat);
      new_pos = file_stat.st_size + pos;
    }
    else
    {
      lock_release(file->lock_file);
      return EINVAL;
    }

    //check validity of new position
    if(new_pos < 0)
    {
      lock_release(file->lock_file);
      return EINVAL;    
    }
    
    file->offset = new_pos;
    *ret = new_pos;
    lock_release(file->lock_file);

    return 0;
}

/* Change the current directory of the current process to pathname.
 * On success, chdir returns 0
 * On error,errno is set
 */
int
sys_chdir(const_userptr_t pathname){
  int err;
  int result;
  char *path = kmalloc(PATH_MAX);
  
  //copy the pathname pointer into a new path char*
  err = copyinstr(pathname, path, PATH_MAX, NULL);
  if(err)
  {
    kfree(path);
    return err;
  }

  //change path into new pathname
  result = vfs_chdir(path);

  kfree(path);
  return result;
}


/* get the name of the current directory is computed and stored in buf
 * On success, __getcwd returns the length of the data returned
 * On error,errno is returned 
 */
int
sys__getcwd(const_userptr_t buf, size_t buflen, int *retval){
    int result;
    struct uio u;
    struct iovec iov;
    struct addrspace *procaddr;

    procaddr = curproc->p_addrspace;

    //set up the buf in userspace
    uio_init_user(&iov, &u, (userptr_t)buf, buflen, 0, UIO_READ, procaddr);

    //get the name of current directory
    result = vfs_getcwd(&u);
    if (result) {
      return result;
    }

    //calculate length of buf data
    *retval = buflen - u.uio_resid;

    return 0;
}


/* clones the file handle oldfd onto the file handle newfd
 * On success, returns newfd
 * On error, errno is returned 
 */
int
sys_dup2(int oldfd,int newfd,int *retval){
    struct openfile*file1;
    struct openfile*file2;
    struct filetable *ft;
    // check whether fd1 and fd2 to be valid fd
    if(oldfd>=OPEN_MAX || newfd>=OPEN_MAX || oldfd<0 || newfd<0){
        return EBADF;
    }
    ft=curproc->p_ft;

    lock_acquire(ft->lock);
    file1=ft->table[oldfd];
    //if the file in fd 1 is not an opened file or valid file 
    if(file1== NULL){
      lock_release(ft->lock);
      return EBADF;
    }
    /*if the oldfd and newfd are the same we need do nothing and 
     *the syscall will be successful atomatically and we just return the newfd 
     */

    if(oldfd == newfd){  
      lock_release(ft->lock);
      *retval=newfd;
      return 0;
    }

    file2 = ft->table[newfd];
    //if the fd2 has already exsist a file we shoudl close that file and make the referecence of that file to decrease 1
    if(file2!=NULL){
      count_decrea(file2);
    }
    // set the fd2 to be the fd1  
    ft->table[newfd]=NULL;
    ft->table[newfd]=file1;
    
    //increase the refercence count of that file 
    count_inc(file1);
    *retval=newfd;
    lock_release(ft->lock);

    return 0;
}