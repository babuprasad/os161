/*
 * file_syscalls.c
 *
 *  Created on: Mar 8, 2014
 *      Author: trinity
 */
#include <types.h>
#include <copyinout.h>
#include <lib.h>
#include <thread.h>
#include <syscall.h>
#include <vfs.h>
#include <vnode.h>
#include <fcntl.h>
#include <current.h>
#include <synch.h>

#include <uio.h>
#include <kern/iovec.h>
#include <seek.h>
#include <stat.h>
#include <kern/errno.h>

int
open(userptr_t filename, int flags,int *err)
{
	char *iobuff;
	int rFlag,vfs_ret;
	size_t aSize;
	if (filename == NULL)
	{
		*err = EFAULT;
		 return -1;
	}
	int tflags = flags & O_ACCMODE;
	if ( tflags != O_RDONLY && tflags != O_WRONLY && tflags !=O_RDWR )
	{
		kprintf("Failing in arguments\n");
		*err = EINVAL;
		return -1;
	}
	int i;
	for ( i=0;i<OPEN_MAX;i++)
	{
		if (curthread->ft[i] == NULL)
		{
			curthread->ft[i] = kmalloc(sizeof(struct fTable));
			if (curthread->ft[i] == NULL)
			{
				*err = ENOMEM;
				return -1;
			}
			curthread->ft[i]->lock = lock_create(curthread->t_name);
			if (curthread->ft[i]->lock == NULL)
			{
				kfree(curthread->ft[i]);
				curthread->ft[i] = NULL;
				*err = ENOMEM;
				return -1;
			}
			curthread->ft[i]->offset=0;
			curthread->ft[i]->ref_count=(curthread->ft[i]->ref_count) + 1;
			curthread->ft[i]->status = flags;
			iobuff = (char *)kmalloc(PATH_MAX*sizeof(char));
			if (iobuff == NULL)
			{
				lock_destroy(curthread->ft[i]->lock);
				kfree(curthread->ft[i]);
				curthread->ft[i] = NULL;
				*err = ENOMEM;
				return -1;
			}
			rFlag = copyinstr((const_userptr_t)filename,iobuff,PATH_MAX,&aSize);
			if(rFlag)
			{
				kfree(iobuff);
				lock_destroy(curthread->ft[i]->lock);
				kfree(curthread->ft[i]);
				curthread->ft[i] = NULL;
				*err = EFAULT;
				return -1;
			}
			vfs_ret=vfs_open((char *)filename,flags,0664,&(curthread->ft[i]->vn));
			if(vfs_ret)
			{
				kfree(iobuff);
				lock_destroy(curthread->ft[i]->lock);
				kfree(curthread->ft[i]);
				curthread->ft[i] = NULL;
				*err = vfs_ret;
				return -1;
			}
			*err = vfs_ret;
			return i;
		}
	}
	return 0;
}
int close(int fd)
{
	if ( fd < 0 || fd > OPEN_MAX)
	{
		return EBADF;
	}
	if (curthread->ft[fd] == NULL)
	{
		return EBADF;
	}
	lock_acquire(curthread->ft[fd]->lock);
	curthread->ft[fd]->ref_count=(curthread->ft[fd]->ref_count)-1;
	if(curthread->ft[fd]->ref_count == 0)
	{
		vfs_close(curthread->ft[fd]->vn);
		lock_release(curthread->ft[fd]->lock);
		lock_destroy(curthread->ft[fd]->lock);
		kfree(curthread->ft[fd]);
		curthread->ft[fd]=NULL;
		return 0;
	}
	else
	{
		lock_release(curthread->ft[fd]->lock);
		curthread->ft[fd]=NULL;
		return 0;
	}
	return 0;
}

int
read(int fd, userptr_t buf, size_t buflen,int *err)
{
	int ret;
	if ( fd < 0 || fd > OPEN_MAX)
	{
		*err = EBADF;
		return -1;
	}
	if (curthread->ft[fd] == NULL)
	{
		*err = EBADF;
		return -1;
	}
	if (buf == NULL)
	{
		*err = EFAULT;
		return -1;
	}
	lock_acquire(curthread->ft[fd]->lock);
	struct iovec iov;
	struct uio uio;

	iov.iov_ubase = buf;
	iov.iov_len = buflen;

	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = curthread->ft[fd]->offset;
	uio.uio_resid = buflen;
	uio.uio_segflg = UIO_USERSPACE;
	uio.uio_space = curthread->t_addrspace;
	uio.uio_rw=UIO_READ;
	if((ret = VOP_READ(curthread->ft[fd]->vn,&uio))!=0)
	{
		*err = ret;
		return -1;
	}
	int diff = uio.uio_offset - curthread->ft[fd]->offset;
	curthread->ft[fd]->offset=uio.uio_offset;
	lock_release(curthread->ft[fd]->lock);
	*err = 0;
	return diff;
}
int
write(int fd, userptr_t buf, size_t buflen, int *err)
{
	int ret;
	if ( fd < 0 || fd > OPEN_MAX)
	{
		*err = EBADF;
		return -1;
	}
	if (curthread->ft[fd] == NULL)
	{
		*err = EBADF;
		return -1;
	}
	if (buf == NULL)
	{
		*err = EFAULT;
		return -1;
	}
	lock_acquire(curthread->ft[fd]->lock);
	struct iovec iov;
	struct uio uio;
	iov.iov_ubase = buf;
	iov.iov_len = buflen;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = curthread->ft[fd]->offset;
	uio.uio_resid = buflen;
	uio.uio_segflg = UIO_USERSPACE;
	uio.uio_space = curthread->t_addrspace;
	uio.uio_rw=UIO_WRITE;
	if((ret = VOP_WRITE(curthread->ft[fd]->vn,&uio))!=0)
	{
		*err = ret;
		return -1;
	}
	int diff = uio.uio_offset - curthread->ft[fd]->offset ;
	curthread->ft[fd]->offset=uio.uio_offset;
	lock_release(curthread->ft[fd]->lock);
	*err=0;
	return diff;
}
off_t
lseek(int fd,off_t pos, int whence,int *err)
{
	off_t nPos=0;
	struct stat eoFILE;
	//kprintf("lseek entered\n");
	if ( fd < 0 || fd > OPEN_MAX)
		{
			*err = EBADF;
			 return -1;
		}
	if (curthread->ft[fd] == NULL)
	{
		*err = EBADF;
		 return -1;
	}
	//kprintf("curthread->ft[fd] == NULL\n");

	//kprintf("fd < 0 || fd > OPEN_MAX\n");
	if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END)
	{
		*err = EINVAL;
		return -1;
	}
	//kprintf("whence != SEEK_SET || whence != SEEK_CUR || whence != SEEK_END\n");
	//kprintf("Acquiring lock\n");
	lock_acquire(curthread->ft[fd]->lock);
	//kprintf("Stat gathering\n");
	VOP_STAT(curthread->ft[fd]->vn,&eoFILE);
	if (whence == SEEK_SET)
	{
		nPos = pos;
	}
	if (whence == SEEK_CUR)
	{
		nPos = curthread->ft[fd]->offset+pos;
	}
	if (whence == SEEK_END)
	{
		nPos = eoFILE.st_size+pos;
	}
	if (nPos < 0)
	{
		*err = EINVAL;
		lock_release(curthread->ft[fd]->lock);
		return -1;
	}
	//kprintf("NPos %llu\n",nPos);
	*err = VOP_TRYSEEK(curthread->ft[fd]->vn,nPos);
	if (*err)
	{
		lock_release(curthread->ft[fd]->lock);
	    return -1;
	}
	curthread->ft[fd]->offset = nPos;
	lock_release(curthread->ft[fd]->lock);
	//kprintf("New position  %llu\n",nPos);
	return curthread->ft[fd]->offset;
}
int
dup2(int ofile_desc, int nfile_desc,int *err)
{
		if ( ofile_desc < 0 || ofile_desc > OPEN_MAX || nfile_desc < 0 ||
				nfile_desc > OPEN_MAX || curthread->ft[ofile_desc] == NULL || curthread->ft[nfile_desc] == NULL)
		{
			*err = EBADF;
			return -1;
		}
		if (curthread->ft[ofile_desc] == curthread->ft[nfile_desc] || ofile_desc == nfile_desc)
		{
		    return ofile_desc;
		}
		if (curthread->ft[nfile_desc] != NULL)
		{
		   *err = close(nfile_desc);
		   if (*err)
		      return -1;
		}
		curthread->ft[nfile_desc] = curthread->ft[ofile_desc];
		lock_acquire(curthread->ft[nfile_desc]->lock);
		curthread->ft[nfile_desc]->ref_count++;
		lock_release(curthread->ft[nfile_desc]->lock);
		return nfile_desc;
}

int
chdir(const_userptr_t pathname)
{
	char new_path[PATH_MAX];
	size_t get;

	if (pathname == NULL)
	{
		return EFAULT;
	}

	int ret = copyinstr(pathname,new_path,PATH_MAX,&get);

	if (ret)
	{
		return EFAULT;
	}

	return vfs_chdir(new_path);
}
int
__getcwd(userptr_t buf, size_t buflen,int *err)
{
	char path[PATH_MAX];
	struct iovec iov;
	struct uio uio;
	uio_kinit(&iov,&uio,path,PATH_MAX,0,UIO_READ);
	*err = vfs_getcwd(&uio);
	if (*err)
	{
		return -1;
	}
	*err = copyout((userptr_t)path,buf,buflen);
	if (*err)
	{
		return -1;
	}
	return uio.uio_offset;
}
int
sys_remove(userptr_t p)
{
	char pbuf_store[PATH_MAX];
	int err;

	err = copyinstr(p, pbuf_store, sizeof(pbuf_store), NULL);
	if (!err) {
		return vfs_remove(pbuf_store);
	}
	else {
		return err;
	}
}
