#ifndef _OPENFILE_H_
#define _OPENFILE_H_

#include <vnode.h>
#include <limits.h>
#include <lib.h>
#include <synch.h>
#include <types.h>

struct openfile{
    int ref_count;
    struct vnode *v_file;
    int flag;
    off_t offset;
    struct lock *lock_file;

};

struct openfile* open_make(struct vnode *,int mode );

void count_inc(struct openfile* );

void file_delete(struct openfile*);

void count_decrea(struct openfile *);

int open_new(char* path,int open_mode ,mode_t mode,struct openfile**);


#endif /*_OPENFILE_H_*/