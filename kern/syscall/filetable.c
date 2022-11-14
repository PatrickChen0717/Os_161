#include <types.h>
#include <lib.h>
#include <limits.h>
#include <synch.h>
#include <vnode.h>
#include <filetable.h>
#include <kern/fcntl.h>
#include <kern/errno.h>



/*
 * create an empty filetable with OPEN_MAX entries
 */
struct filetable *
filetable_create (void){
    int result1;
    int result2;
    int result3;
    struct openfile* stdin;
    struct openfile* stdout;
    struct openfile* stderr;
    char copy[32];

    //malloc space for the new filetable
    struct filetable *ft;
    ft = (struct filetable*)kmalloc(sizeof(struct filetable));
    if(ft == NULL){
        return NULL;
    }

    //initialized the all the entries of new filetable to be null 
    for(int i=0;i<OPEN_MAX;i++){  
        ft->table[i] = NULL;
    }

    ft->lock = lock_create("ft_lock");
    if(ft->lock==NULL){
        kfree(ft);
        return NULL;
    }
    
    /*set up the stdin,stdout,and stderr*/
    //Add stdin into filetable index 0
    strcpy(copy,"con:");
    result1 = open_new_file(copy, O_RDONLY, 0664, &stdin);  
    if(result1){
        return NULL;
    }

    //Add stdout into filetable index 1
    strcpy(copy,"con:");
    result2 = open_new_file(copy, O_WRONLY, 0664, &stdout);
    if(result2){
        return NULL;
    }

    //Add stderr into filetable index 2
    strcpy(copy,"con:");
    result3 = open_new_file(copy, O_WRONLY, 0664, &stderr);
    if(result3){
        return NULL;
    }

    //Assign stdin,stdout,and stderr to index 0,1,2
    ft->table[0] = stdin;
    ft->table[1] = stdout;
    ft->table[2] = stderr;

    return ft;
}


/* decrement the refcount of all the open files inside filetable to be deleted
 * destroy the lock
 * free the mem space used by the filetable
 */
void
filetable_delete(struct filetable *table_to_delete){
    for(int i=0; i<OPEN_MAX; i++){
        //decrement the refcount
        count_decrea(table_to_delete->table[i]);

        //remove reference
        table_to_delete->table[i] = NULL;
    }

    //free memory
    lock_destroy(table_to_delete->lock);
    kfree(table_to_delete);
}


/*Return the new add integer number which is greater than 2 
 *and less than or euqal the OPEN_MAX-1,if the return value
 *is -1,which means the current filetable is full and we can
 *not add new files to the filetable at this point.  
 */
int
filetable_add (struct filetable *ft, struct openfile *file){
    for(int i=3;i<OPEN_MAX;i++){
        //check if the filetable index is empty
        if(ft->table[i] == NULL){
            ft->table[i] = file;
            return i;
        }
    }

    return -1;
}

/* remove a specific file descriptor inside the filetable
 * by derement refcount and remove reference
 */
int
filetable_remove (struct filetable *ft, int index){
    //check if fd is valid
    if(ft->table[index] != NULL){

        //decrement the refcount
        count_decrea(ft->table[index]);

        //remove reference
        ft->table[index] = NULL;

        //return 0, removal is success
        return 0;
    }

    //return 1, for no file to remove
    return 1;
}


int
filetable_copy (struct filetable * ft_old, struct filetable ** ft_new){
    struct filetable* dest;

    if(ft_old==NULL){
        *ft_new=NULL;
        return 0;
    }
    dest=filetable_create();
    if(dest==NULL){
        return ENOMEM;
    }
    
    for(int i=3; i<OPEN_MAX; i++){
        if(ft_old->table[i]!=NULL){
            dest->table[i]=ft_old->table[i];
            count_inc(ft_old->table[i]);
        }
    }
    *ft_new=dest;

    return 0;
}