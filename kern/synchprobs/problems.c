/*
 * Copyright (c) 2001, 2002, 2009
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
 * Driver code for whale mating problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>
#include <wchan.h>

/*Locks for each quadrant - Babu*/
struct lock *lockquad0;
struct lock *lockquad1;
struct lock *lockquad2;
struct lock *lockquad3;

/*Get the corresponding lock for the destination quadrant */
struct lock *getlock(int destQuadrant);

struct whalemating
{
	volatile int num_male_whale;
	volatile int num_female_whale;
	volatile int num_matchmaker_whale;
	struct wchan *male_wchan;
	struct wchan *female_wchan;
	struct wchan *matchmaker_wchan;
	struct wchan *match_wchan;
	volatile int match_found;
	struct lock *lock;
	volatile int match_male;
	volatile int match_female;
};
struct whalemating *whale_mating;


/*
 * 08 Feb 2012 : GWA : Driver code is in kern/synchprobs/driver.c. We will
 * replace that file. This file is yours to modify as you see fit.
 *
 * You should implement your solution to the whalemating problem below.
 */

// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

void whalemating_init() {


	whale_mating = kmalloc(sizeof(struct whalemating));
	if (whale_mating == NULL)
	{
		return;
	}
	whale_mating->male_wchan = wchan_create("name");
	if (whale_mating->male_wchan == NULL)
	{
		kfree(whale_mating);
		return;
	}
	whale_mating->female_wchan = wchan_create("name");
	if (whale_mating->female_wchan == NULL)
	{
		wchan_destroy(whale_mating->male_wchan);
		kfree(whale_mating);
		return;
	}
	whale_mating->matchmaker_wchan = wchan_create("name");
	if (whale_mating->matchmaker_wchan == NULL)
	{
		wchan_destroy(whale_mating->male_wchan);
		wchan_destroy(whale_mating->female_wchan);
		kfree(whale_mating);
		return;
	}
	whale_mating->match_wchan = wchan_create("name");
	if (whale_mating->matchmaker_wchan == NULL)
		{
			wchan_destroy(whale_mating->male_wchan);
			wchan_destroy(whale_mating->female_wchan);
			wchan_destroy(whale_mating->match_wchan);
			kfree(whale_mating);
			return;
		}
	whale_mating->lock = lock_create("name");
	if (whale_mating->lock == NULL)
	{
		wchan_destroy(whale_mating->male_wchan);
		wchan_destroy(whale_mating->female_wchan);
		wchan_destroy(whale_mating->matchmaker_wchan);
		wchan_destroy(whale_mating->match_wchan);
		kfree(whale_mating);
		return;
	}
	whale_mating->num_male_whale = 0;
	whale_mating->num_female_whale = 0;
	whale_mating->num_matchmaker_whale = 0;
	whale_mating->match_found=0;
	whale_mating->match_male=0;
	whale_mating->match_female=0;


  return;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void whalemating_cleanup() {
	wchan_destroy(whale_mating->male_wchan);
	wchan_destroy(whale_mating->female_wchan);
	wchan_destroy(whale_mating->matchmaker_wchan);
	wchan_destroy(whale_mating->match_wchan);
	lock_destroy(whale_mating->lock);
	kfree(whale_mating);
  return;
}

void
male(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which;
  
 lock_acquire(whale_mating->lock);
 male_start();
	// Implement this function

 while(whale_mating->num_male_whale>=1)
 {
   wchan_lock(whale_mating->male_wchan);
   lock_release(whale_mating->lock);
   wchan_sleep(whale_mating->male_wchan);
   lock_acquire(whale_mating->lock);
 }
 whale_mating->num_male_whale++;
 wchan_wakeall(whale_mating->match_wchan);
 while(!(whale_mating->match_found==1))
 {
	 wchan_lock(whale_mating->match_wchan);lock_release(whale_mating->lock);wchan_sleep(whale_mating->match_wchan);lock_acquire(whale_mating->lock);
 }
 whale_mating->match_male=1;
 male_end();
 lock_release(whale_mating->lock);

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}

void
female(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which;
  
  lock_acquire(whale_mating->lock);
  female_start();

	// Implement this function

   while(whale_mating->num_female_whale>=1)
   {
     wchan_lock(whale_mating->female_wchan);lock_release(whale_mating->lock);wchan_sleep(whale_mating->female_wchan);lock_acquire(whale_mating->lock);
   }
   whale_mating->num_female_whale++;
   wchan_wakeall(whale_mating->match_wchan);
   while(!(whale_mating->match_found==1))
   {
  	 wchan_lock(whale_mating->match_wchan);lock_release(whale_mating->lock);wchan_sleep(whale_mating->match_wchan);lock_acquire(whale_mating->lock);
   }
  whale_mating->match_female=1;
  female_end();
  lock_release(whale_mating->lock);

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}


void
matchmaker(void *p, unsigned long which)
{
	struct semaphore * whalematingMenuSemaphore = (struct semaphore *)p;
  (void)which;
  lock_acquire(whale_mating->lock);
  matchmaker_start();
  while(whale_mating->num_matchmaker_whale>=1)
  {
     wchan_lock(whale_mating->matchmaker_wchan);lock_release(whale_mating->lock);wchan_sleep(whale_mating->matchmaker_wchan);lock_acquire(whale_mating->lock);
  }
  whale_mating->num_matchmaker_whale++;
  wchan_wakeall(whale_mating->match_wchan);
  while((whale_mating->num_male_whale == 0) || (whale_mating->num_female_whale == 0))
  {
	  wchan_lock(whale_mating->match_wchan);lock_release(whale_mating->lock);wchan_sleep(whale_mating->match_wchan);lock_acquire(whale_mating->lock);
  }
  whale_mating->match_found=1;
  wchan_wakeall(whale_mating->match_wchan);
  while(whale_mating->match_male!=1 || whale_mating->match_female!=1)
  {
  lock_release(whale_mating->lock);
  lock_acquire(whale_mating->lock);
  }
  whale_mating->match_male=0;
  whale_mating->match_female=0;
  whale_mating->num_male_whale--;
  whale_mating->num_female_whale--;
  whale_mating->num_matchmaker_whale--;

  wchan_wakeall(whale_mating->male_wchan);
  wchan_wakeall(whale_mating->female_wchan);
  wchan_wakeall(whale_mating->matchmaker_wchan);

	// Implement this function
  whale_mating->match_found=0;
  matchmaker_end();
  lock_release(whale_mating->lock);

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // whalemating driver can return to the menu cleanly.
  V(whalematingMenuSemaphore);
  return;
}


/*
 * You should implement your solution to the stoplight problem below. The
 * quadrant and direction mappings for reference: (although the problem is,
 * of course, stable under rotation)
 *
 *   | 0 |
 * --     --
 *    0 1
 * 3       1
 *    3 2
 * --     --
 *   | 2 | 
 *
 * As way to think about it, assuming cars drive on the right: a car entering
 * the intersection from direction X will enter intersection quadrant X
 * first.
 *
 * You will probably want to write some helper functions to assist
 * with the mappings. Modular arithmetic can help, e.g. a car passing
 * straight through the intersection entering from direction X will leave to
 * direction (X + 2) % 4 and pass through quadrants X and (X + 3) % 4.
 * Boo-yah.
 *
 * Your solutions below should call the inQuadrant() and leaveIntersection()
 * functions in drivers.c.
 */

/**
 * Added by Babu : 27 Feb 2012
 * Solving Stop light problem with the help
 * of locks and predefined semaphores.
 */


// 13 Feb 2012 : GWA : Adding at the suggestion of Isaac Elbaz. These
// functions will allow you to do local initialization. They are called at
// the top of the corresponding driver code.

void stoplight_init() {

	lockquad0 = lock_create("quad0lk");
	lockquad1 = lock_create("quad1lk");
	lockquad2 = lock_create("quad2lk");
	lockquad3 = lock_create("quad3lk");
	return;
}

// 20 Feb 2012 : GWA : Adding at the suggestion of Nikhil Londhe. We don't
// care if your problems leak memory, but if you do, use this to clean up.

void stoplight_cleanup() {
	lock_destroy(lockquad0);
	lock_destroy(lockquad1);
	lock_destroy(lockquad2);
	lock_destroy(lockquad3);
	return;
}

void
gostraight(void *p, unsigned long direction)
{
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
	unsigned long destQuadrant1 = direction;
	unsigned long destQuadrant2 = (direction + 3) % 4;
	kprintf("go straight....\n");

	lock_acquire(getlock(destQuadrant1));
	lock_acquire(getlock(destQuadrant2));

	inQuadrant(destQuadrant1);
	inQuadrant(destQuadrant2);
	leaveIntersection();

	lock_release(getlock(destQuadrant2));
	lock_release(getlock(destQuadrant1));

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  V(stoplightMenuSemaphore);
  return;
}

void
turnleft(void *p, unsigned long direction)
{
	kprintf("turn left....\n");
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
	unsigned long destQuadrant1 = direction;
	unsigned long destQuadrant2 = (direction + 3) % 4;
	unsigned long destQuadrant3 = (direction + 2) % 4;

	lock_acquire(getlock(destQuadrant1));
	lock_acquire(getlock(destQuadrant2));
	lock_acquire(getlock(destQuadrant3));

	inQuadrant(destQuadrant1);
	inQuadrant(destQuadrant2);
	inQuadrant(destQuadrant3);
	leaveIntersection();

	lock_release(getlock(destQuadrant3));
	lock_release(getlock(destQuadrant2));
	lock_release(getlock(destQuadrant1));
  
  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  V(stoplightMenuSemaphore);
  return;
}

void
turnright(void *p, unsigned long direction)
{
	kprintf("turn right....\n");
	struct semaphore * stoplightMenuSemaphore = (struct semaphore *)p;
	unsigned long destQuadrant1 = direction;

	lock_acquire(getlock(destQuadrant1));

	inQuadrant(destQuadrant1);
	leaveIntersection();

	lock_release(getlock(destQuadrant1));

  // 08 Feb 2012 : GWA : Please do not change this code. This is so that your
  // stoplight driver can return to the menu cleanly.
  V(stoplightMenuSemaphore);
  return;
}

/*
 * Added by Babu
 * Function which serve as a wrapper to get the correct inQuadrant
 * locks based on the direction passed
 */
struct lock *
getlock(int destQuadrant)
{
	struct lock *lockquad;
	switch (destQuadrant)
	{
		case 0:
			lockquad = lockquad0;
			break;
		case 1:
			lockquad = lockquad1;
			break;
		case 2:
			lockquad = lockquad2;
			break;
		case 3:
			lockquad = lockquad3;
			break;
		default:
			panic("unknown direction");
			break;
	}

	/* If the lock is not held before, then acquire it */
	/*if(lock_do_i_hold(lockquad))
	{
		lock_acquire(lockquad);
		inQuadrant(destQuadrant);
		lock_release(lockquad);
	}*/
	return lockquad;

}
