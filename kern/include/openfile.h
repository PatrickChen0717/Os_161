#ifndef _OPENFILE_H_
#define _OPENFILE_H_

#include <vnode.h>
#include <limits.h>
#include <lib.h>
#include <synch.h>
#include <types.h>

struct openfile{
    int ref_count;          /*counter of times the file is opened*/
    struct vnode *v_file;   /*abstract representation of the open file*/
    int flag;               /*flag that the file is opened with*/
    off_t offset;           /*offset of the file*/
    struct lock *lock_file; /*lock for openfile synchronization*/

};

struct openfile* openfile_make(struct vnode *,int mode );

void count_inc(struct openfile* );

void file_delete(struct openfile*);

void count_decrea(struct openfile *);

int open_new_file(char* path,int open_mode ,mode_t mode,struct openfile**);

#endif /*_OPENFILE_H_*/