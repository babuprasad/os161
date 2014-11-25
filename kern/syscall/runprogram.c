/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <lib.h>
#include <thread.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <vfs.h>
#include <syscall.h>
#include <test.h>
#include <synch.h>
#include <unistd.h>
#include <copyinout.h>
/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, char *argv[], unsigned long argc)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;

	struct vnode *o, *i, *e;
	char *con0 = kstrdup("con:");
	char *con1 = kstrdup("con:");
	char *con2 = kstrdup("con:");
	int inpTemp = vfs_open(con0,O_RDONLY,0664,&i);
	int outTemp = vfs_open(con1,O_WRONLY,0664,&o);
	int errTemp = vfs_open(con2,O_WRONLY,0664,&e);
	KASSERT(inpTemp!=1);
	KASSERT(outTemp!=1);
	KASSERT(errTemp!=1);

	struct fTable *input, *output, *error;
	input= kmalloc(sizeof(struct fTable));
	output = kmalloc(sizeof(struct fTable));
	error = kmalloc(sizeof(struct fTable));
	KASSERT(input!=NULL);
	KASSERT(output!=NULL);
	KASSERT(error!=NULL);

	input->name=kstrdup("Standard_Input");
	input->offset=0;
	input->ref_count =0;
	input->status=O_RDONLY;
	input->vn=i;
	input->lock=lock_create("Standard Input");

	output->name=kstrdup("Standard_Output");
	output->offset=0;
	output->ref_count =0;
	output->status=O_WRONLY;
	output->vn=o;
	output->lock=lock_create("Standard Output");

	error->name=kstrdup("Standard_Error");
	error->offset=0;
	error->ref_count =0;
	error->status=O_WRONLY;
	error->vn=e;
	error->lock=lock_create("Standard Error");
	KASSERT(input->lock!=NULL);
	KASSERT(output->lock!=NULL);
	KASSERT(error->lock!=NULL);

	curthread->ft[STDIN_FILENO]=input;
	curthread->ft[STDOUT_FILENO]=output;
	curthread->ft[STDERR_FILENO]=error;
	kfree(con0);
	kfree(con1);
	kfree(con2);
	//kprintf("IO fd's initialized\n");

	unsigned long j = 0;
	int  err, argsize= 0;
	int  totsize = 0 , totsizecnt = 0;



	/*if(argv != NULL)
	{
		while(argv[j] != NULL)
		{
			argc++;
			j++;
		}
	}*/



	char *kbuf[argc];
	int32_t *koffset[argc];
	size_t copylen[argc];

	if(argc != 0)
	{

		/**Copyin user args to kernel buffer */
		totsizecnt = (argc + 1) * sizeof(int32_t);

		j = 0;
		if(argv != NULL)
		{
			//while(argv[j] != NULL)
			for(j=0;j<argc; j++)
			{
				argsize = strlen(argv[j]) + 1;
				totsize = argsize + (4 - (argsize % 4));
				copylen[j] = totsize * sizeof(char);
				kbuf[j] = kmalloc (copylen[j]);
				koffset[j] = kmalloc(sizeof(int32_t));
				memcpy(kbuf[j], argv[j], argsize);

				//err = copyinstr((const_userptr_t)argv[j], kbuf[j], argsize, &copylen[j]);
				//if(copylen[j] != (size_t)argsize)
					//return EFAULT;
				//if(err != 0)
					//return err;

				// Assign the offset
				memcpy(koffset[j], &totsizecnt, sizeof(int32_t));
				//err = copyin((userptr_t)(totsizecnt), koffset[j], sizeof(int32_t));
				//if(err != 0)
					//return err;
				totsizecnt += totsize;
			}
			koffset[j] = NULL;
		}
		else
			kprintf("user args null");

		//char *kprgname;
		//size_t copied;
		//if(progname != NULL)
		//{
			//err = copyinstr((const_userptr_t)progname, kprgname, strlen(progname), &copied);
			//if(copied != strlen(progname))
				//return EFAULT;
			//if(err != 0)
				//return err;
		//}

	}


	/* Open the file. */
	kprintf("opening file : %s",progname);
	result = vfs_open(progname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	KASSERT(curthread->t_addrspace == NULL);

	/* Create a new address space. */
	curthread->t_addrspace = as_create();
	if (curthread->t_addrspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_addrspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		kprintf("Load ELF error\n");
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_addrspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_addrspace */
		return result;
	}

	j = 0;
	// Starting address  of userstack from which args and pointers should be copied
	vaddr_t initstckptr = stackptr - totsizecnt;
	vaddr_t userstckptr = initstckptr;
	vaddr_t userargsptr = userstckptr;
	size_t usercopylen[argc];

	if(argc != 0)
	{
		for(j=0; j<argc; j++)
		{
			userargsptr =  initstckptr + (vaddr_t)(*koffset[j]);

			/* copyout user pointer (in kernel buffer) to user stack */
			err = copyout(&userargsptr, (userptr_t)userstckptr, sizeof(int32_t));
			if(err != 0)
				return err;

			/* Copyout user arguments (in kernel buffer) to user stack*/
			err = copyoutstr((const char *)kbuf[j],(userptr_t)userargsptr, copylen[j], &usercopylen[j]);
			if(err != 0)
				return err;
			userstckptr += sizeof(int32_t);

		}
	}

	userstckptr = initstckptr;
	while(userstckptr != initstckptr + (vaddr_t)(*koffset[0]))
	{
		//kprintf("user address : %x",userstckptr);
		//kprintf("user address : %x",userargsptr);
		//kprintf("user stack : %x\n", *(int32_t *)userstckptr);
		userstckptr+=4;
	}

	/* Warp to user mode. */
	//enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
		//	  stackptr, entrypoint);
	userstckptr = initstckptr + (vaddr_t)(*koffset[0]);
	//kprintf("user space address : %x",userstckptr);

	enter_new_process(argc /*argc*/, (userptr_t)initstckptr /*userspace addr of argv (TODO userstckptr or stackptr?)*/,
			initstckptr, entrypoint);
	
	/* enter_new_process does not return. */
	panic("enter_new_process returned\n");
	return EINVAL;
}

