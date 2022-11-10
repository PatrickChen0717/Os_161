#ifndef _FILETABLE_H_
#define _FILETABLE_H_

#include <limits.h>
#include <lib.h>
#include <synch.h>
#include <types.h>
#include <vnode.h>
#include <openfile.h>


struct filetable
{
    struct lock *lock;                  /*lock used for filetable synchronization*/
    struct openfile *table[OPEN_MAX];   /*array to store all the openfile entries*/
};

/*create an empty filetable with OPEN_MAX entries*/
struct filetable * filetable_create (void);

/*delete the filetable and free memory*/
void filetable_delete (struct filetable *table_to_delete);

/*add a new openfile into the filetable*/
int filetable_add (struct filetable *ft, struct openfile *file);

/*remove an existing file in the filetable*/
int filetable_remove (struct filetable *ft, int index);

int filetable_copy (struct filetable * ft_old, struct filetable * ft_new);

#endif /*_FILETABLE_H_*/