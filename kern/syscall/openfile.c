#include <types.h>
#include <lib.h>
#include <vnode.h>
#include <openfile.h>
#include <synch.h>
#include <vfs.h>



struct openfile*
open_make(struct vnode* node,int open_mode){

    struct openfile* new_open;
    new_open=(struct openfile*)kmalloc(sizeof(struct openfile));
    if(new_open==NULL){
        return NULL;
    }
    new_open->lock_file=lock_create("");
    if(new_open->lock_file==NULL){
        kfree(new_open);
        return NULL;
    }
    new_open->ref_count=1;
    new_open->flag=open_mode;
    new_open->offset=0;
    new_open->v_file=node;

    return new_open;
}

void
file_delete(struct openfile* file){
    KASSERT(file!=NULL);
    lock_release(file->lock_file);
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
        file_delete(file_name);
    }
    else{
        file_name->ref_count--;
        lock_release(file_name->lock_file);
    }

}

int
open_new(char * path,int open_mode,mode_t mode,struct openfile **ret_file){

    struct vnode*node;
    struct openfile*file;
    int flag;
    flag=vfs_open(path,open_mode,mode,&(node));
    if(flag==1){
        return 1;// open the vnode failed
    }

    file=open_make(node,open_mode);
    *ret_file=file;

    return 0;

}
