#include <types.h>
#include <copyinout.h>
#include <syscall.h>
#include <vnode.h>
#include <filetable.h>
#include <vfs.h>
#include <openfile.h>
#include <limits.h>

int
sys_open(const_userptr_t userpath, int openflags){
    int err;
    struct vnode *ret;
    char *path;
    path=kmalloc(__PATH_MAX);
    

    err=copyinstr(userpath,path,_PATH_MAX,NULL);
    if(err==1){
      return NULL;
    }

    int result = vfs_open(path, openflags, 0, &ret);
    if(result){
      kprintf("Could not open %s\n", path, result);
      return -1;
    }

    int err = open_new(path, openflags, 0, &ret);

    if(err){
      kprintf("Could not open_new file %s\n", path, err);
      return -1;
    }

    filetable_add (cur->p_ft, &ret);

}