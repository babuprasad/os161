
/*
 * forktest - test fork().
 *
 * This should work correctly when fork is implemented.
 *
 * It should also continue to work after subsequent assignments, most
 * notably after implementing the virtual memory system.
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <err.h>


int
main()
{
	//printf("entering sysexit test");
	//int pid = fork();
	//if(pid == 0)
	//{
	//	warnx("child thread");
	//}
	//else
	//{
	//	warnx("parent thread");
	//}			
	//warnx(" fork pid : %d \n", pid);
	printf("hello world /n");
	//exit(0);
	return 0;
}
