#include <types.h>
#include <lib.h>
#include <vnode.h>
#include <openfile.h>
#include <synch.h>
#include <vfs.h>
#include <kern/errno.h>


struct openfile*
openfile_make(struct vnode* node,int open_mode){ 

    struct openfile* new_file;
    new_file=(struct openfile*)kmalloc(sizeof(struct openfile));
    if(new_file==NULL){
        return NULL;
    }
    new_file->lock_file=lock_create("");
    if(new_file->lock_file==NULL){
        kfree(new_file);
        return NULL;
    }
    new_file->ref_count=1;
    new_file->flag=open_mode;
    new_file->offset=0;
    new_file->v_file=node;

    return new_file;
}

void
file_delete(struct openfile* file){
    KASSERT(file!=NULL);
    lock_destroy(file->lock_file);
    kfree(file);
};

void
count_inc(struct openfile* file_name){

    lock_acquire(file_name->lock_file);
    file_name->ref_count++;
    lock_release(file_name->lock_file);
}

void 
count_decrea(struct openfile* file_name){

    lock_acquire(file_name->lock_file);
    if(file_name->ref_count==1){
        lock_release(file_name->lock_file);
        file_delete(file_name);
    }
    else{
        file_name->ref_count--;
        lock_release(file_name->lock_file);
    }

}

int
open_new_file(char * path,int open_mode,mode_t mode,struct openfile **ret_file){

    struct vnode*node;
    struct openfile*file;
    int err;
    err=vfs_open(path,open_mode,mode,&(node));
    if(err){
        return err;// open the vnode failed
    }

    file=openfile_make(node,open_mode);
    if(file==NULL){
        vfs_close(node);
        return ENOMEM;
    }
    *ret_file=file;

    return 0;

}
