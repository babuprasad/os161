/*
 * process.c
 *
 *  Created on: Mar 8, 2014
 *  Author: Babu
 */

#include <types.h>
#include <kern/errno.h>
#include <kern/syscall.h>
#include <lib.h>
#include <mips/trapframe.h>
#include <thread.h>
#include <current.h>
#include <syscall.h>
#include <addrspace.h>
#include <spl.h>
#include <synch.h>
#include <limits.h>
#include <kern/fcntl.h>
#include <copyinout.h>
#include <vfs.h>
#include <kern/wait.h>
#include <process.h>
#include <copyinout.h>

//extern struct spinlock proc_lock;

/**
 * Added by Babu:
 * Pid pool to maintain the free available pids
 */
struct pid_pool
{
	pid_t pid_avail; // Available pid
	struct pid_pool *next; // Point to next available pid
};


// Golbal PID counter
pid_t global_pid_count = 1;

/* Head and tail pointer for query and insert operation from pool which
 * follows FIFO approach - Added by Babu
 */
struct pid_pool *head = NULL;
struct pid_pool *tail = NULL;



/**
 * When a process exits it should invoke this method so that
 * this will add the allocated pid to free pool which can be assigned
 * for other process  - Added by Babu
 */
void
add_pid_to_pool(pid_t pid_free)
{
	if(tail == NULL)
	{
		tail = kmalloc(sizeof(struct pid_pool));
		if(tail != NULL)
			tail->pid_avail = pid_free;
		else
			panic("not able to add pid");
	}
	else
	{
		tail->next = kmalloc(sizeof(struct pid_pool));
		if(tail->next != NULL)
		{
			tail = tail->next;
			tail->pid_avail = pid_free;
		}
	}
	if(head == NULL)
		head = tail;

}

/**
 * Allocate pid based on the pids available in the pool or
 * generate from the counter if the pool is empty  - Added by Babu
 */
pid_t
allocate_pid()
{
	pid_t pid_alloc;
	struct pid_pool *temp;
	if(head == NULL)
	{
		if(global_pid_count < PID_MAX)
			pid_alloc = global_pid_count++;
		else
			return ENOMEM;
	}
	else
	{
		pid_alloc = head->pid_avail;
		temp = head;
		if(head == tail)
		{
			head = NULL;
			tail = NULL;
		}
		else
			head = head->next;
		kfree(temp);
	}
	return pid_alloc;
}

/**
 * Added by Babu : 04/02/2014
 * Function to destroy process structure
 */
void
process_destroy(struct process *process)
{
   //int retval = -1;
	/* Adding pid to available pool so that it can be allocated to other processes	 */
	add_pid_to_pool(process->p_pid_self);
	sem_destroy(process->p_exitsem);
	//processtable[(int)process->p_pid_self] = NULL;
	(process_lookup(process->p_pid_self))->process_info = NULL;

	kfree(process);
	return;
}

/**
 * Added by Babu on : 04/01/2014
 * Get pid of the calling process
 */
pid_t
sys_getpid(int32_t *retval)
{
	if(curthread != NULL)
	{
		if(curthread->t_process != NULL)
		{
			//*retval = *curthread->t_process->p_pid_self;
			memcpy(retval, &curthread->t_process->p_pid_self, sizeof(pid_t));
			return 0;
		}
	}
	return 1;
}

/**
 * Added by Babu on : 04/01/2014
 * Get pid of the calling process
 */
pid_t
sys_getppid(int32_t *retval)
{
	if(curthread != NULL)
	{
		if(curthread->t_process != NULL)
		{
			//*retval = *curthread->t_process->p_pid_parent;
			memcpy(retval, &curthread->t_process->p_pid_parent, sizeof(pid_t));
			return 0;
		}
	}
	return 1;
}

/**
 * Added by Babu : 04/02/2014warn
 * Waitpid will wait for the process to change status/destroyed and collect the return status
 * process which are not collected the return status remain as 'zombies'
 */
int
sys_waitpid(int32_t *retval, pid_t pid, int32_t *exitcode, int32_t flags)
{
	//kprintf("Waiting for %d",pid);
	int err = -1;

	struct process *childprocess = (process_lookup(pid))->process_info;
	if(childprocess == NULL)
		return EFAULT;

	if(pid < 1 || pid > MAX_RUNNING_PROCS)
		return EINVAL;

	if(childprocess == NULL)
		return EINVAL;

	if(exitcode == NULL)
    	return EFAULT;

    if(exitcode == (void *)0x80000000) // Kernel pointer check
        return EFAULT;

    if(exitcode == (void *)0x40000000)  // Invalid pointer check
        return EFAULT;

    int i=0;
    char * checkptr = (char *)exitcode;
    while(checkptr[i] != 0)
    {
    	checkptr++;
    	i++;
    }
	if(i%4 != 0)
		return EFAULT;

	if(sizeof(checkptr)%4 != 0)
		return EFAULT;

	// if flags are not proper
	if(flags < 0 || flags > 2)
		return EINVAL;

	// Dont wait for yourself :)
	if(pid == curthread->t_process->p_pid_self)
		return EFAULT;

	if(pid == curthread->t_process->p_pid_parent)
		return EFAULT;

	/*Dont wait for siblings child*/
	if(childprocess->p_pid_parent != -1)
	{
		if(curthread->t_process->p_pid_parent == childprocess->p_pid_parent)
			return ECHILD;

	}

	if(childprocess->p_exited)
	{
		/*collect the exitstatus and return*/
		//memcpy(exitcode, &childprocess->p_exitcode, sizeof(int));
		//memcpy(retval, &childprocess->p_pid_self, sizeof(pid_t));
		if(sizeof(*exitcode) != 4)
			return EFAULT;
		*exitcode =  childprocess->p_exitcode;
		*retval = childprocess->p_pid_self;
		process_destroy(childprocess);
		err = 0;
	}
	else
	{
		/*else wait on sem until the thread exits then collect exit status and return*/
		P(childprocess->p_exitsem);
		//memcpy(exitcode, childprocess->p_exitcode, sizeof(int));
		//memcpy(retval, childprocess->p_pid_self, sizeof(pid_t));
		*exitcode = childprocess->p_exitcode;
		*retval = childprocess->p_pid_self;
		process_destroy(childprocess);
		err = 0;
	}
	return err;
}

/**
 * Added by Babu : 04/02/2014
 * sysexit will stop the current thread execution and destroy it immediately
 */
void
sys__exit(int exitstatus)
{
	int32_t pid = 0;
	sys_getpid(&pid);
	//kprintf("Exiting for %d",pid);
	if(curthread->t_process != NULL)
	{
		if(!curthread->t_process->p_exited)
		{
			curthread->t_process->p_exitcode = _MKWAIT_EXIT(exitstatus);
			curthread->t_process->p_exited = true;
			/**
			 *
			 */
			/* Free thread structure and destroy all the thread related book keeping stuffs*/
			//kprintf("thread exited successfully.\n");
			thread_exit();
		}
	}

	return;
}

/**
 * Added by Babu on 03/09/2014:
 *
 * fork() creates a new process by duplicating the calling process. The new process,
 * referred to as the child, is an exact duplicate of the calling process, referred
 * to as the parent.
 *
 * return value(retval) 0 for child process and child pid for parent process
 *
 */
int
sys_fork(int32_t *retval, struct trapframe *tf)
{
	struct addrspace *child_addrspce = NULL;
	struct process *child = NULL;
//	/int spl = 0;
	struct process *parent = NULL;
	struct trapframe *child_trapframe = NULL;
	int retvalfork = 0;
	/**
	 * Create
	 */

	parent = curthread->t_process;
	if(parent == NULL)
	{
		*retval = ENOMEM;
		return -1;
	}

		//panic("parent process retrieve operation failed");

	parent->p_thread = curthread;
	//parent_trapframe = tf;

	/*
	 * Child process creation
	 */
	child  = kmalloc(sizeof(struct process));
	if(child == NULL)
	{
		*retval = ENOMEM;
		return -1;
	}
		//panic("Child process creation failed");

	/* Child process struct init */
	child->p_pid_parent = parent->p_pid_self;
	child->p_pid_self = allocate_pid();
	child->p_exited = false;
	child->p_exitsem = sem_create("p_exitsem", 0);

	/* To protect from multiple thread generating pid same time - Babu*/
	spinlock_acquire(&proc_lock);

	child->p_pid_self = allocate_pid();

	/* Assigning in process table */
	addto_proc_table(child);

	/* release lock */
	spinlock_release(&proc_lock);

	/* create a copy of trapframe using memcpy */
	child_trapframe = kmalloc(sizeof(struct trapframe));
	if(child_trapframe == NULL)
	{
		*retval = ENOMEM;
		return -1;
	}
	memcpy(child_trapframe, tf, sizeof(struct trapframe));

	/* Addres space and file table cloning from parent */
	as_copy(parent->p_thread->t_addrspace, &child_addrspce);

	/* disable interrupts*/
	//spl  = splhigh();

	/**
	 * copy parent trapframe to the child process
	 * and invoke creating child thread
	 */
	//tf->tf_a0 = (uint32_t) child_addrspce;
	retvalfork = thread_fork("child process", child_entrypoint, child_trapframe,(unsigned long) child_addrspce, &child->p_thread);
	//kprintf("return value of thread_fork : %d\n",retvalfork);

	/*Assigning the process structure to the thread just got created*/
	if(child == NULL)
	{
		*retval = ENOMEM;
		return -1;
	}
	if(child->p_thread == NULL)
	{
		*retval = ENOMEM;
		return -1;
	}

	if(child->p_thread->t_process == NULL)
	{
		*retval = ENOMEM;
		return -1;
	}
	child->p_thread->t_process = child;

	/* Return values as child process pid */
	*retval = child->p_pid_self;
	//copyout(&child->p_pid_self, (userptr_t) retval, sizeof(child->p_pid_self));

	/* enable interrupts again*/
	//splx(spl);

	/* Return success or error code  */
	return retvalfork;

}

/**
 * Child entry point function which will be invoked
 * immediately after the child process creation
 */
void
child_entrypoint(void *data1, unsigned long data2)
{
	struct trapframe tf_copy;
	struct trapframe *child_tf = (struct trapframe *) data1;
	struct addrspace *child_addrspce = (struct addrspace *) data2;


	if(child_addrspce == NULL || child_tf == NULL) /* To indicate failure of child fork */
	{
		child_tf->tf_v0 = ENOMEM;
		child_tf->tf_a3 = 1;
		return;
		//panic("child entry point failed");
	}
	else /* To indicate success of child fork */
	{
		child_tf->tf_v0 = 0;
		child_tf->tf_a3 = 0;
	}

	/*
	* Now, advance the program counter, to avoid restarting
	* the syscall over and over again.
	*/
	child_tf->tf_epc += 4;

	/** Loading child's address space into current thread address space */
	curthread->t_addrspace = child_addrspce;
	as_activate(curthread->t_addrspace);

	/* Copy modified trap frame*/
	memcpy(&tf_copy, child_tf, sizeof(struct trapframe));

	/* And enter user mode*/
	mips_usermode(&tf_copy);

	//kprintf("user mode entered successfully /n");

}


/** *
 * Added by Babu
 * execv() system call creates a process by reading from ELF file and
 * loading it into the address space.
 *
 */
int
sys_execv(userptr_t userprgname, userptr_t userargv[])
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result, j=0, err, argsize= 0;
	int argc = 0, totsize = 0 , totsizecnt = 0;
	char *prgname = (char *)userprgname;
	char **argv = (char **)userargv;

	if(userprgname == NULL || userargv == NULL)
		return EFAULT;

	if((void *)userprgname == (void *)0x40000000 || (void *)userargv == (void *)0x40000000)
		return EFAULT;

	if((void *)userprgname == (void *)0x80000000 || (void *)userargv == (void *)0x80000000)
		return EFAULT;

	if((char *)userprgname == "\0" || (char *)userprgname == "")
		return EINVAL;

	if(*argv == "\0" || *argv == "")
		return EINVAL;

	if(strlen(prgname) == 0 || strlen(*argv) == 0)
		return EINVAL;

	if(strcmp(prgname,"\0") == 0 || strcmp(prgname,"") == 0)
		return EINVAL;

	if(strcmp(*argv,"\0") == 0 || strcmp(*argv,"") == 0)
		return EINVAL;


	if(argv != NULL)
	{
		while(argv[j] != NULL)
		{
			if((void *)userargv[j] == (void *)0x80000000 ||  (void *)userargv[j] == (void *)0x40000000)
				return EFAULT;
			argc++;
			j++;
		}
	}

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
			while(argv[j] != NULL)
			{
				argsize = strlen(argv[j]) + 1;
				totsize = argsize + (4 - (argsize % 4));
				copylen[j] = totsize * sizeof(char);
				kbuf[j] = kmalloc (copylen[j]);
				koffset[j] = kmalloc(sizeof(int32_t));
				//memcpy(kbuf[j], argv[j], argsize);
				err = copyinstr((const_userptr_t)argv[j], kbuf[j], argsize, &copylen[j]);
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
				j++;
			}
			koffset[j] = NULL;
		}
		else
			kprintf("user args null");

	}

	/*char *kprgname;
	size_t copied;
	if(prgname != NULL)
	{
		err = copyinstr((const_userptr_t)prgname, kprgname, strlen(prgname)+1, &copied);
		if(copied != strlen(prgname))
			return EFAULT;
		if(err != 0)
			return err;
	}*/

	/* Open the file. */
	result = vfs_open(prgname, O_RDONLY, 0, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	//KASSERT(curthread->t_addrspace == NULL);

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

/**
 * Lookup process using pid in process table
 */
struct process_list *
process_lookup(pid_t pid_lookup)
{
	struct process_list *local_ptable = proc_table;
	while(local_ptable != NULL)
	{
		if(local_ptable->process_info->p_pid_self == pid_lookup)
			return local_ptable;

		local_ptable = local_ptable->next;
	}

	return NULL;
}


/**
 * Add process entry to process table
 */
void
addto_proc_table(struct process *proc)
{
	struct process_list *local_ptable = proc_table;
	if(local_ptable == NULL)
	{
		proc_table = kmalloc(sizeof(struct process_list));
		proc_table->process_info = proc;
		proc_table->next = NULL;
	}
	else
	{
		while(local_ptable->next != NULL)
		{
			local_ptable = local_ptable->next;
		}
		local_ptable->next = kmalloc(sizeof(struct process_list));
		local_ptable->next->process_info = proc;
		local_ptable->next->next = NULL;
	}

}
