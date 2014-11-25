/*
 * process.h
 *
 *  	Created on: Mar 8, 2014
 *      Author: Babu
 */

#ifndef PROCESS_H_
#define PROCESS_H_

#include <spinlock.h>

struct trapframe;

/*
 * Process table which holds all information about the process
 * such as pid, parent process, threads belongs to that process
 *
 */
struct process {

	pid_t p_pid_self;  /* process id of the process */

	struct thread *p_thread;

	pid_t p_pid_parent;

	// Variables for process exit
	bool p_exited;

	int p_exitcode;

	struct semaphore *p_exitsem;
};



/** process table as linked list */
struct process_list
{
	struct process *process_info;
	struct process_list *next;
};

struct process_list *proc_table;

struct spinlock proc_lock;

/**
 * When a process exits it should invoke this method so that
 * this will add the allocated pid to free pool which can be assigned
 * for other process  - Added by Babu
 */
void add_pid_to_pool(pid_t);

/**
 * Allocate pid based on the pids available in the pool or
 * generate from the counter if the pool is empty  - Added by Babu
 */
pid_t allocate_pid(void);

/**
 * function to destroy process and it related book keeping stuffs
 */
void process_destroy(struct process *process);


/**
 * getpid system call which fetches the pid of the calling process
 */
pid_t sys_getpid(int32_t *retval);

/**
 * getppid system call which fetches the pid of the calling process parent
 */
pid_t sys_getppid(int32_t *retval);

/**
 * exit system call which allows the calling process to exit
 */
void sys__exit(int exitcode);

/**
 * wait system call allows the calling process' parent to collect the status of child process
 */
int sys_waitpid(int32_t *retval, pid_t pid, int32_t *exitcode, int32_t flags);

/**
 * fork system call which creates clone of the calling process
 */
int sys_fork(int32_t *retval, struct trapframe *tf);

/**
 * execv system call allows create process from the file and loads into address space and executes
 */
int sys_execv(userptr_t prgname, userptr_t argv[]);

/**
 * Entry point function for the child process/thread created by fork()
 */
void child_entrypoint(void *data1, unsigned long data2);

/**
 * Find process in process table
 */
struct process_list * process_lookup(pid_t pid_lookup);

/**
 * Find process in process table
 */
void addto_proc_table(struct process *proc);


#endif /* PROCESS_H_ */
