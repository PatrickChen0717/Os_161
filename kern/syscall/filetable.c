#include <types.h>
#include <lib.h>
#include <limits.h>
#include <synch.h>
#include <vnode.h>
#include <filetable.h>
#include <kern/fcntl.h>

struct filetable *
filetable_create (void){
    int result1;
    int result2;
    int result3;
    struct openfile* stdin;
    struct openfile* stdout;
    struct openfile* stderr;
    char copy[32];

    struct filetable *ft;
    ft = (struct filetable*)kmalloc(sizeof(struct filetable));
    if(ft == NULL){
        return NULL;
    }

    for(int i=0;i<OPEN_MAX;i++){  //initialized the all the entries of new filetable to be null 
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
        kprintf("stdin: error\n");
        return NULL;
    }

    //Add stdout into filetable index 1
    strcpy(copy,"con:");
    result2 = open_new_file(copy, O_WRONLY, 0664, &stdout);
    if(result2){
        kprintf("stdout: error\n");
        return NULL;
    }

    //Add stderr into filetable index 2
    strcpy(copy,"con:");
    result3 = open_new_file(copy, O_WRONLY, 0664, &stderr);
    if(result3){
        kprintf("stderr: error\n");
        return NULL;
    }

    ft->table[0] = stdin;
    ft->table[1] = stdout;
    ft->table[2] = stderr;

    return ft;
}

void
filetable_delete(struct filetable *table_to_delete){
    for(int i=0; i<OPEN_MAX; i++){
        //destroy opened file
        count_decrea(table_to_delete->table[i]);
        table_to_delete->table[i] = NULL;
    }

    lock_destroy(table_to_delete->lock);
    kfree(table_to_delete);
}
/*Return the new add integer number which is greater than 2 
 *and less than or euqal the OPEN_MAX-1,if the return value
 *is -1,which means the current filtable is full and we can
 *not add new files to the filetable at this point.  
 */
int
filetable_add (struct filetable *ft, struct openfile *file){
    for(int i=3;i<OPEN_MAX;i++){
        if(ft->table[i] == NULL){
            ft->table[i] = file;
            return i;
        }
    }

    return -1;
}


int
filetable_remove (struct filetable *ft, int index,struct openfile **file){

    if(ft->table[index] != NULL){
        *file=ft->table[index];
        ft->table[index] = NULL;

        //return 1, removal is success
        return 0;
    }

    //return 0 for no file to remove
    return 1;
}
