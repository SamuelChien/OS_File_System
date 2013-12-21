/* BEGIN A4 SETUP */
/* This file existed previously, but has been completely replaced for A4.
 * We have kept the dumb versions of sys_read and sys_write to support early
 * testing, but they should be replaced with proper implementations that 
 * use your open file table to find the correct vnode given a file descriptor
 * number.  All the "dumb console I/O" code should be deleted.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <vfs.h>
#include <vnode.h>
#include <uio.h>
#include <kern/fcntl.h>
#include <kern/unistd.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <copyinout.h>
#include <synch.h>
#include <file.h>
#include <kern/seek.h> /* For lseek */
#include <vm.h>

/* This special-case global variable for the console vnode should be deleted 
 * when you have a proper open file table implementation.
 */
//struct vnode *cons_vnode=NULL; 

/* This function should be deleted, including the call in main.c, when you
 * have proper initialization of the first 3 file descriptors in your 
 * open file table implementation.
 * You may find it useful as an example of how to get a vnode for the 
 * console device.
 */

/*
void dumb_consoleIO_bootstrap()
{
  int result;
  char path[5];
*/
  /* The path passed to vfs_open must be mutable.
   * vfs_open may modify it.
   */
/*
  strcpy(path, "con:");
  result = vfs_open(path, O_RDWR, 0, &cons_vnode);

  if (result) {
*/
    /* Tough one... if there's no console, there's not
     * much point printing a warning...
     * but maybe the bootstrap was just called in the wrong place
     */
/*
    kprintf("Warning: could not initialize console vnode\n");
    kprintf("User programs will not be able to read/write\n");
    cons_vnode = NULL;
  }
}
*/


/*
 * mk_useruio
 * sets up the uio for a USERSPACE transfer. 
 */
static
void
mk_useruio(struct iovec *iov, struct uio *u, userptr_t buf, 
	   size_t len, off_t offset, enum uio_rw rw)
{

	iov->iov_ubase = buf;
	iov->iov_len = len;
	u->uio_iov = iov;
	u->uio_iovcnt = 1;
	u->uio_offset = offset;
	u->uio_resid = len;
	u->uio_segflg = UIO_USERSPACE;
	u->uio_rw = rw;
	u->uio_space = curthread->t_addrspace;
}

/*
 * sys_open
 * just copies in the filename, then passes work to file_open.
 * You have to write file_open.
 * 
 */
int
sys_open(userptr_t filename, int flags, int mode, int *retval)
{
	char *fname;
	int result;
    /*
    struct __userptr { char _dummy; };
    typedef struct __userptr *userptr_t;

    Filename is just a struct with a char _dummy.
    Longest full path name is __PATH_MAX 1024
    ENOMEM means Out of memory

    if fname's kmalloc fails, return out of memory error
    */
	if ( (fname = (char *)kmalloc(__PATH_MAX)) == NULL) {
		return ENOMEM;
	}
    //safely copy the filename user address to fname kernel address
	result = copyinstr(filename, fname, __PATH_MAX, NULL);

    //if error, then return error code
	if (result) {
		kfree(fname);
		return result;
	}

    //open the file up using file_open method
	result =  file_open(fname, flags, mode, retval);
	kfree(fname);
	return result;
}

/* 
 * sys_close
 * You have to write file_close.
 */
int
sys_close(int fd)
{
    if (fd < 0 || fd > __OPEN_MAX) {
        return EBADF;
    }
	return file_close(fd);
}

/* 
 * sys_dup2
 *
 */
int
sys_dup2(int oldfd, int newfd, int *retval)
{
    struct filetable *ft = curthread->t_filetable;
    if(oldfd < 0 || oldfd > __OPEN_MAX || newfd < 0 || \
            newfd > __OPEN_MAX || ft->fdtable[oldfd] == NULL) {
        return EBADF;
    }
    if(oldfd == newfd) {
        *retval = newfd;
        return 0;
    }
    if(ft->fdtable[newfd] == NULL) {
        ft->fdtable[newfd] = ft->fdtable[oldfd];
        ft->fdtable[newfd]->dupcount++;
        *retval = newfd;
        return 0;
    }
    file_close(newfd);
    return 0;
}

/*
 * sys_read
 * calls VOP_READ.
 * 
 * A4: This is the "dumb" implementation of sys_write:
 * it only deals with file descriptors 1 and 2, and 
 * assumes they are permanently associated with the 
 * console vnode (which must have been previously initialized).
 *
 * In your implementation, you should use the file descriptor
 * to find a vnode from your file table, and then read from it.
 *
 * Note that any problems with the address supplied by the
 * user as "buf" will be handled by the VOP_READ / uio code
 * so you do not have to try to verify "buf" yourself.
 *
 * Most of this code should be replaced.
 */
int
sys_read(int fd, userptr_t buf, size_t size, int *retval)
{
	struct uio user_uio;
	struct iovec user_iov;
	int err;
	int offset = 0;

	/* better be a valid file descriptor */
	if (fd < 0 || fd > __OPEN_MAX) {
	  return EBADF;
	}

    struct filedescriptor* f;
    f = curthread->t_filetable->fdtable[fd];
    if(f == NULL) {
        return EBADF;
    }
    if((f->flags != O_RDONLY) && (f->flags != O_RDWR)) {
        return EBADF;
    }
    offset = f->offset;

	/* set up a uio with the buffer, its size, and the current offset */
	mk_useruio(&user_iov, &user_uio, buf, size, offset, UIO_READ);

	/* does the read */
	err = VOP_READ(f->vn, &user_uio);
	if (err) {
		return err;
	}

	/*
	 * The amount read is the size of the buffer originally, minus
	 * how much is left in it.
	 */
	*retval = size - user_uio.uio_resid;
    f->offset += (*retval);
	return 0;
}

/*
 * sys_write
 * calls VOP_WRITE.
 *
 * A4: This is the "dumb" implementation of sys_write:
 * it only deals with file descriptors 1 and 2, and 
 * assumes they are permanently associated with the 
 * console vnode (which must have been previously initialized).
 *
 * In your implementation, you should use the file descriptor
 * to find a vnode from your file table, and then read from it.
 *
 * Note that any problems with the address supplied by the
 * user as "buf" will be handled by the VOP_READ / uio code
 * so you do not have to try to verify "buf" yourself.
 *
 * Most of this code should be replaced.
 */

int
sys_write(int fd, userptr_t buf, size_t len, int *retval) 
{
        struct uio user_uio;
        struct iovec user_iov;
        int err;
        int offset = 0;

        if (fd < 0 || fd > __OPEN_MAX) {
            return EBADF;
        }

        struct filedescriptor* f;
        f = curthread->t_filetable->fdtable[fd];
        if(f == NULL) {
            return EBADF;
        }
        if((f->flags != O_WRONLY) && (f->flags != O_RDWR)) {
            return EBADF;
        }
        offset = f->offset;

        /* set up a uio with the buffer, its size, and the current offset */
        mk_useruio(&user_iov, &user_uio, buf, len, offset, UIO_WRITE);

        /* does the write */
        err = VOP_WRITE(f->vn, &user_uio);
        if (err) {
                return err;
        }

        /*
         * the amount written is the size of the buffer originally,
         * minus how much is left in it.
         */
        *retval = len - user_uio.uio_resid;
        f->offset += (*retval);
        return 0;
}

/*
 * sys_lseek
 * 
 */
int
sys_lseek(int fd, off_t offset, int whence, off_t *retval)
{
    int err;
    struct stat s;
    struct filedescriptor *f;
    
    if(fd < 0 || fd > __OPEN_MAX) {
        return EBADF;
    }
    f = curthread->t_filetable->fdtable[fd];
    if(f == NULL) {
        return EBADF;
    }
    if(whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        return EINVAL;
    }
    if(whence == SEEK_CUR){
        err = VOP_TRYSEEK(f->vn, f->offset + offset);
        if(err){
            return err;
        }
        *retval = f->offset + offset;
        f->offset = (*retval);
        return 0;
    }
    if(whence == SEEK_SET) {
        err = VOP_TRYSEEK(f->vn, offset);
        if(err){
            return err;
        }
        *retval = offset;
        f->offset = (*retval);
        return 0;
    }
    if(whence == SEEK_END) {
        VOP_STAT(f->vn, &s);
        err = VOP_TRYSEEK(f->vn, s.st_size + offset);
        if(err){
            return err;
        }
        *retval = s.st_size + offset;
        f->offset = (*retval);
        return 0;
    }
    return 1;
}


/* really not "file" calls, per se, but might as well put it here */

/* sys_mkdir
 * 
 */
int
sys_mkdir(userptr_t path, int mode) {
    int err;
    char buffer[__NAME_MAX];
    err = copyinstr(path, buffer, __NAME_MAX, NULL);
    if(err){
        return err;
    }
    err = vfs_mkdir(buffer, mode);
    return err;
}

/* sys_rmdir
 * 
 */
int
sys_rmdir(userptr_t path) {
    int err, result;
    char buffer[__NAME_MAX];
    err = copyinstr(path, buffer, __NAME_MAX, NULL);
    if(err){
        return err;
    }
    result = vfs_rmdir(buffer);
    return result;
}

/*
 * sys_chdir
 * 
 */
int
sys_chdir(userptr_t path)
{
    int result;
    char buffer[__NAME_MAX];
    result = copyinstr(path, buffer, __NAME_MAX, NULL);
    if(result){
        return result;
    }
    result = vfs_chdir(buffer);
    return result;
}

/*
 * sys___getcwd
 * 
 */
int
sys___getcwd(userptr_t buf, size_t buflen, int *retval)
{
    struct uio user_uio;
    struct iovec user_iov;
    int result;

    mk_useruio(&user_iov, &user_uio, buf, buflen, 0, UIO_READ);
    result = vfs_getcwd(&user_uio);
    *retval = buflen - user_uio.uio_resid;
    return result;
}

/*
 * sys_fstat
 */
int
sys_fstat(int fd, userptr_t statptr)
{
	struct vnode *vn;
	struct stat kbuf;
	int err;

	/* Use your filetable implementation to get a vnode for fd */
    if(fd < 0 || fd > __OPEN_MAX) {
        return EBADF;
    }
    vn = curthread->t_filetable->fdtable[fd]->vn;

	/* Call VOP_STAT on the vnode */
	err = VOP_STAT(vn, &kbuf);
	if (err) {
		return err;
	}

	return copyout(&kbuf, statptr, sizeof(struct stat));
}

/*
 * sys_getdirentry
 */
int
sys_getdirentry(int fd, userptr_t buf, size_t buflen, int *retval)
{
	struct vnode *vn;
	off_t offset = 0;
	int err;
	struct uio my_uio;
	struct iovec uio_iov;
    struct filedescriptor *f;

	/* Use your filetable implementation to get info for fd */
    f = curthread->t_filetable->fdtable[fd];
    if(f == NULL){
        return EBADF;
    }

	/* Check that directory fd is open for read.
	 * Return EBADF if it is not readable. 
	 */
    if(fd < 0 || fd > __OPEN_MAX || f->flags == O_WRONLY){
        return EBADF;
    }

	/* Initialize vn and offset using your filetable info for fd */
    vn = f->vn;
    offset = f->offset;

	/* set up a uio with the buffer, its size, and the current offset */
	mk_useruio(&uio_iov, &my_uio, buf, buflen, offset, UIO_READ);

	/* does the read */
	err = VOP_GETDIRENTRY(vn, &my_uio);
	if (err) {
		return err;
	}

	/* Set the offset to the updated offset in the uio. 
	 * Save the new offset with your filetable info for fd.
	 */
	offset = my_uio.uio_offset;
    f->offset = offset;
	
	/*
	 * the amount read is the size of the buffer originally, minus
	 * how much is left in it. Note: it is not correct to use
	 * uio_offset for this!
	 */
	*retval = buflen - my_uio.uio_resid;
	return 0;
}

/* END A4 SETUP */
