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
    strcpy(copy,"con:");

    struct filetable *ft;
    ft = (struct filetable*)kmalloc(sizeof(struct filetable));
    if(ft == NULL){
        return NULL;
    }

    for(int i=0;i<OPEN_MAX;i++){
        ft->table[i] = NULL;
    }

    ft->lock = lock_create("ft_lock");
    if(ft->lock==NULL){
        kfree(ft);
        return NULL;
    }

    result1 = open_new(copy, O_RDONLY, 0664, &stdin);
    if(result1==1){
        return NULL;
    }
    result2 = open_new(copy, O_WRONLY, 0664, &stdout);
        if(result2==1){
        return NULL;
    }
    result3 = open_new(copy, O_WRONLY, 0664, &stderr);
        if(result3==1){
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

int
filetable_add (struct filetable *ft, struct openfile *file){
    for(int i=3;i<OPEN_MAX;i++){
        if(ft->table[i] == NULL){
            ft->table[i] = file;
            //return index, file is successfully added
            return i;
        }
    }
     //return -1 for add file is not successful
    return -1;
}


int
filetable_remove (struct filetable *ft, int index){
    if(ft->table[index] != NULL){
        ft->table[index] = NULL;

        //return 1, removal is success
        return 1;
    }

    //return 0 for no file to remove
    return 0;
}