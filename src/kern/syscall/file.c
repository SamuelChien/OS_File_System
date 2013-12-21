/* BEGIN A4 SETUP */
/*
 * File handles and file tables.
 * New for ASST4
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/unistd.h>
#include <file.h>
#include <syscall.h>
#include <kern/fcntl.h>
#include <current.h>
#include <vnode.h>
#include <vfs.h>
#include <thread.h>

/*** openfile functions ***/

/*
 * file_open
 * opens a file, places it in the filetable, sets RETFD to the file
 * descriptor. the pointer arguments must be kernel pointers.
 * NOTE -- the passed in filename must be a mutable string.
 * 
 * A4: As per the OS/161 man page for open(), you do not need 
 * to do anything with the "mode" argument.
 */
int
file_open(char *filename, int flags, int mode, int *retfd)
{
   (void) mode;
   struct vnode *vn;
   struct filedescriptor *fd;
   int err;
   
   fd = (struct filedescriptor *)kmalloc(sizeof(struct filedescriptor));
   if(fd == NULL) {
       return ENOMEM;
   }

   //call vfs_open
   err = vfs_open(filename, flags, mode, &vn);
   
   //if there is an error, return the error code
   if(err) {
        return err;
   }

   //set the fields for the filedescriptor to be added to the filetable
   fd->vn = vn;
   fd->offset = 0;
   fd->flags &= O_ACCMODE;

   //add the filedescriptor to the filetable and set retfd to the fd pointer
   for (int i = 3; i < __OPEN_MAX; i++) { //0, 1, 2 reserved for stdin/out/err
       if(curthread->t_filetable->fdtable[i] == NULL){
           curthread->t_filetable->fdtable[i] = fd;
           *retfd = i;
           break;
       }
       return EMFILE;
   }
   return 0;
}


/* 
 * file_close
 * Called when a process closes a file descriptor.  
 */
int
file_close(int fd)
{
    KASSERT(fd >= 0 && fd < __OPEN_MAX);
    int err = filetable_dropfd(curthread->t_filetable, fd);
    if(err){
        return err;
    }
    return 0;
}

/*** filetable functions ***/

/* 
 * filetable_init
 * pretty straightforward -- allocate the space, set up 
 * first 3 file descriptors for stdin, stdout and stderr,
 * and initialize all other entries to NULL.
 * 
 * Should set curthread->t_filetable to point to the
 * newly-initialized filetable.
 * 
 * Should return non-zero error code on failure.  Currently
 * does nothing but returns success so that loading a user
 * program will succeed even if you haven't written the
 * filetable initialization yet.
 */

int
filetable_init(void)
{
    //first create the filetable
    struct filetable* ft;
    ft = (struct filetable*)kmalloc(sizeof(struct filetable));
    if(ft == NULL){
        return ENOMEM;
    }

    for(int i = 0; i < __OPEN_MAX; i++){
        ft->fdtable[i] = NULL;
    }
    curthread->t_filetable = ft;

    int fd;
    char path[5];
    strcpy(path, "con:");

    //initialize stdin
    int result_0;
    result_0 = file_open(path, O_RDONLY, 0, &fd);
    if(result_0){ //error
        kprintf("Error initializing stdin: %s\n", strerror(result_0));
        filetable_destroy(ft);
        return result_0;
    }
    KASSERT(fd == 0);

    //initialize stdout
    strcpy(path, "con:");
    int result_1 = file_open(path, O_WRONLY, 0, &fd);
    if(result_1){
        kprintf("Error initializing stdout: %s\n", strerror(result_1));
        filetable_destroy(ft);
        return result_1;
    }
    KASSERT(fd == 1);

    //initialize stderr
    strcpy(path, "con:");
    int result_2 = file_open(path, O_WRONLY, 0, &fd);
    if(result_2){
        kprintf("Error initializing stderr: %s\n", strerror(result_2));
        filetable_destroy(ft);
        return result_2;
    }
    KASSERT(fd == 2);
	
    return 0;
}	

/*
 * filetable_destroy
 * closes the files in the file table, frees the table.
 * This should be called as part of cleaning up a process (after kill
 * or exit).
 */
void
filetable_destroy(struct filetable *ft)
{
    for(int i = 0; i < __OPEN_MAX; i++){
        filetable_dropfd(ft, i);
    }
    kfree(ft);
}	


/* 
 * You should add additional filetable utility functions here as needed
 * to support the system calls.  For example, given a file descriptor
 * you will want some sort of lookup function that will check if the fd is 
 * valid and return the associated vnode (and possibly other information like
 * the current file position) associated with that open file.
 */

/*
 * filetable_dropfd
 * drops the fd at index retfd from the filetable ft
 */
int filetable_dropfd(struct filetable* ft, int retfd){
    struct filedescriptor* fd = ft->fdtable[retfd];
    if(fd == NULL){
        return EBADF;
    }
    if(fd->dupcount > 1){
        fd->dupcount--;
        return 0;
    }
    vfs_close(fd->vn);
    kfree(fd);
    fd = NULL;
    return 0;
}

/* END A4 SETUP */
