#include <types.h>
#include <lib.h>
#include <vnode.h>
#include <openfile.h>
#include <synch.h>
#include <vfs.h>
#include <kern/errno.h>

/**
 * create an new openfile with a sepcific vnode 
 *  and return that openfile
 * 
*/
struct openfile*
openfile_make(struct vnode* node,int open_mode){ 

    struct openfile* new_file;
    new_file=(struct openfile*)kmalloc(sizeof(struct openfile));
    //if no memory 
    if(new_file==NULL){
        return NULL;
    }
    new_file->lock_file=lock_create("");
    //if lock for that file was created failed 
    if(new_file->lock_file==NULL){
        kfree(new_file);
        return NULL;
    }
    // initialized the openfile 
    new_file->ref_count=1;
    new_file->flag=open_mode;
    new_file->offset=0;
    new_file->v_file=node;

    return new_file;
}
/* delete the openfile and free it memory
 */
void
file_delete(struct openfile* file){
    KASSERT(file!=NULL);
    lock_destroy(file->lock_file);
    kfree(file);
};
/* increase the reference count of the inpu file 
 * 
 */
void
count_inc(struct openfile* file_name){

    lock_acquire(file_name->lock_file);
    file_name->ref_count++;
    lock_release(file_name->lock_file);
}
/* decrease the reference count of the inpu file 
 * 
 */
void 
count_decrea(struct openfile* file_name){

    lock_acquire(file_name->lock_file);
    //if the reference of the input file is 1 and we will delete the file
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
    //if there is an err such the open mode is not 0 1 or 2 
    //vfs_open will return an err 
    if(err){
        return err;// open the vnode failed
    }
    //create an new openfile wi the vnode 
    file=openfile_make(node,open_mode);
    if(file==NULL){
        vfs_close(node);
        return ENOMEM;
    }
    *ret_file=file;

    return 0;

}
