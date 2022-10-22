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
    struct lock *lock;
    struct openfile *table[OPEN_MAX];
};


struct filetable * filetable_create (void);

void filetable_delete (struct filetable *table_to_delete);

int filetable_add (struct filetable *ft, struct openfile *file);

int filetable_remove (struct filetable *ft, int index);

#endif /*_FILETABLE_H_*/