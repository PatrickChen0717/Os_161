#include <types.h>
#include <copyinout.h>
#include <kern/errno.h>
#include <execv.h>
#include <kern/limits.h>



void
init(struct  execv_store* exe){
        exe->args_len=0;
        exe->args_count=0;
}

int
copy_args(struct execv_store* exe,userptr_t args)
{
        // temporary local varibale used to save the pointer from the userspace
        userptr_t kernel_arg_pointers;
        int err;
        // the actual_len that the copyin copied 
        size_t actual_len;
        
        for(int i=0;i<__ARG_MAX;i++){
                //copy the pointer from the userspace ,pointer means the args[i], we will increase i at the end of the loop
                err=copyin(args,&(kernel_arg_pointers),sizeof(userptr_t));
                if(err){
                        return err;
                }
                //at the end of pointer array, end of the args[]
                if(kernel_arg_pointers==NULL){
                        return err;
                }
                //then copy the actual argument(the actual string ) in the pointer that we just copy from the user-level
                err=copyinstr(kernel_arg_pointers,exe->kernel_data+exe->args_len,__ARG_MAX,&actual_len);
                //if the string is too long we return the that err 
                if(err==ENAMETOOLONG){
                        err=E2BIG;
                        return err;
                }
                if(err){
                        return err;
                }
                exe->args_len=exe->args_len+actual_len;// update the actual length the we copied
                args=args+sizeof(userptr_t);          //increase the i for args[i]
                exe->args_count=exe->args_count+1;  //increase the count of the number of args

        }
        return err;
}


int 
copy_args_out(struct execv_store*exe,vaddr_t *sp,userptr_t *args_start)
{
        int err;

        userptr_t pointer_cur;//args[i]

        userptr_t arg_start;// the start address of the actual content(the strings)

        userptr_t arg_addr;// the addr of the content after the calculation

        size_t copied_len;// how many bits we have already copied 

        size_t len;//actual copied len for the copystrout function 

        vaddr_t local_sp;

        userptr_t NULL_term;

        
        copied_len=0;

        local_sp=*sp; // get the sp value
        //make the sapce the strings
        local_sp=local_sp-(exe->args_len);

        // padding the arg_len
        local_sp=local_sp-(local_sp % sizeof(void *));

        //save the arg_start,which is the actual start of the content
        arg_start=(userptr_t)local_sp; 

        //move the space for the pointers , args
        local_sp=local_sp-(sizeof(userptr_t)*exe->args_count);

        //save the pointer_start,which is the the pointer start
        pointer_cur=(userptr_t)local_sp;

        while (copied_len<(exe->args_len))
        {
                //calculate the start address of arg[i], which is the value that we should wirte in the agr[i]
                arg_addr=arg_start+copied_len;

                err=copyout(&arg_addr,pointer_cur,sizeof(userptr_t));
                if(err){
                        return err;
                }

                //then we copy the actrual content to the address that we just   calculated above , we copy the data from the kenerl space to that addr
                err=copyoutstr(exe->kernel_data+copied_len,arg_addr,exe->args_len,&len);
                if(err){
                        return err;
                }
                //update the copied_len 
                copied_len=copied_len+len;
                //increase the pointer_cur by one 
                pointer_cur=pointer_cur+sizeof(userptr_t);
        }

        //add the NULL terminator to the end of our arg array
        NULL_term=NULL;
        err=copyout(&NULL_term,pointer_cur,sizeof(userptr_t));

        if(err){
                return err;
        }
        
        
        *sp=local_sp;
        *args_start=(userptr_t)local_sp;
        return 0;
        
}