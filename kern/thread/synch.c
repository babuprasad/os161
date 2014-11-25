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
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, int initial_count)
{
	struct semaphore *sem;

	KASSERT(initial_count >= 0);

	sem = kmalloc(sizeof(struct semaphore));
	if (sem == NULL) {
			return NULL;
	}

	sem->sem_name = kstrdup(name);
	if (sem->sem_name == NULL) {
			kfree(sem);
			return NULL;
	}

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
    sem->sem_count = initial_count;

    return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
		spinlock_cleanup(&sem->sem_lock);
		wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void 
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

        spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 * Bridge to the wchan lock, so if someone else comes
		 * along in V right this instant the wakeup can't go
		 * through on the wchan until we've finished going to
		 * sleep. Note that wchan_sleep unlocks the wchan.
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_lock(sem->sem_wchan);
		spinlock_release(&sem->sem_lock);
        wchan_sleep(sem->sem_wchan);
		spinlock_acquire(&sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
    KASSERT(sem != NULL);
	spinlock_acquire(&sem->sem_lock); 

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        lock = kmalloc(sizeof(struct lock));
        if (lock == NULL) {
                return NULL;
        }

        lock->lk_name = kstrdup(name);

        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }

        //added by vasanth
        lock->lock_owner=NULL;
        lock->lock_wchan = wchan_create(lock->lk_name);
		if (lock->lock_wchan == NULL)
		{
			kfree(lock->lk_name);
			kfree(lock);
			return NULL;
		}
		spinlock_init(&lock->spn_lock);

        // add stuff here as needed
        return lock;
}

void
lock_destroy(struct lock *lock)
{

        KASSERT(lock != NULL);
        //added by vasanth
        //KASSERT(lock->lock_owner == NULL);
        spinlock_cleanup(&lock->spn_lock);
        wchan_destroy(lock->lock_wchan);

        // add stuff here as needed
        
        kfree(lock->lk_name);
        kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
		//added by vasanth
		KASSERT(lock != NULL);
		if(!lock_do_i_hold(lock))
		{
			spinlock_acquire(&lock->spn_lock);
			while (lock->lock_owner!=NULL)
			{
				wchan_lock(lock->lock_wchan);
				spinlock_release(&lock->spn_lock);
				wchan_sleep(lock->lock_wchan);
				spinlock_acquire(&lock->spn_lock);
			}
			lock->lock_owner=curthread;
			spinlock_release(&lock->spn_lock);
		}
        // Write this

       // (void)lock;  // suppress warning until code gets written
}

void
lock_release(struct lock *lock)
{
		//added by vasanth
 		KASSERT(lock != NULL);
		spinlock_acquire(&lock->spn_lock);
		if(lock->lock_owner==curthread)
		{
			lock->lock_owner=NULL;
			wchan_wakeall(lock->lock_wchan);
		}
		spinlock_release(&lock->spn_lock);
		// Write this


        //(void)lock;  // suppress warning until code gets written
}

bool
lock_do_i_hold(struct lock *lock)
{
        // Write this
		bool status;
		spinlock_acquire(&lock->spn_lock);
		if (lock->lock_owner==curthread)
		{
			status = true;
		}
		else
		{
			status = false;
		}
		spinlock_release(&lock->spn_lock);
		return status;

        //(void)lock;  // suppress warning until code gets written

        //return true; // dummy until code gets written
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(struct cv));
        if (cv == NULL) {
        	return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
            kfree(cv);
            return NULL;
        }
        
        // add stuff here as needed
        /* Added by Babu :
        * Adding a wait channel for making the threads wait during cv_acquire
        */
        cv->cv_waitchan = wchan_create(cv->cv_name);

        
        return cv;
}


void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

        // add stuff here as needed
        /*
         * Added by Babu : wchan destructor
         */
        wchan_destroy(cv->cv_waitchan);
        
        kfree(cv->cv_name);
        kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
    // Write this
	/** Release lock, wait and then acquire once awake */
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);

	wchan_lock(cv->cv_waitchan);
	if(lock_do_i_hold(lock))
			lock_release(lock);
	wchan_sleep(cv->cv_waitchan);

	if(!lock_do_i_hold(lock))
		lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
    // Write this
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);



	//kprintf("Signaling CV\n");

	wchan_wakeone(cv->cv_waitchan);
	//wchan_unlock(cv->cv_waitchan);



	(void) lock;
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);

	wchan_wakeall(cv->cv_waitchan);

	(void) lock;
}

struct rwlock *
rwlock_create(const char *name)
{
	struct rwlock *rwlock;
	rwlock = kmalloc(sizeof(struct rwlock));
	if (rwlock == NULL)
	{
		return NULL;
	}
	rwlock->rwlock_name=kstrdup(name);
	if (rwlock->rwlock_name == NULL)
	{
	    kfree(rwlock);
	    return NULL;
	}
	rwlock->rlock_wchan = wchan_create(rwlock->rwlock_name);
	if (rwlock->rlock_wchan == NULL)
	{
		kfree(rwlock->rwlock_name);
		kfree(rwlock);
		return NULL;
	}
	rwlock->wlock_wchan = wchan_create(rwlock->rwlock_name);
	if (rwlock->wlock_wchan == NULL)
	{
		kfree(rwlock->rwlock_name);
		wchan_destroy(rwlock->rlock_wchan);
		kfree(rwlock);
		return NULL;
	}
	rwlock->num_reader=0;
	rwlock->num_writer=0;
	rwlock->rw_lock = lock_create(rwlock->rwlock_name);
	if (rwlock->rw_lock == NULL)
	{
		kfree(rwlock->rwlock_name);
		wchan_destroy(rwlock->rlock_wchan);
		wchan_destroy(rwlock->wlock_wchan);
		kfree(rwlock);
		return NULL;
	}
	return rwlock;

}
void
rwlock_destroy(struct rwlock *rwlock)
{
	KASSERT(rwlock != NULL);
	lock_destroy(rwlock->rw_lock);
	wchan_destroy(rwlock->rlock_wchan);
	wchan_destroy(rwlock->wlock_wchan);
	kfree(rwlock->rwlock_name);
	kfree(rwlock);
}

void
rwlock_acquire_read(struct rwlock *rwlock)
{
	KASSERT(rwlock != NULL);
	lock_acquire(rwlock->rw_lock);
	while (rwlock->num_writer>0)
	{
		wchan_lock(rwlock->rlock_wchan);
		lock_release(rwlock->rw_lock);
		wchan_sleep(rwlock->rlock_wchan);
		lock_acquire(rwlock->rw_lock);
	}
	rwlock->num_reader++;
	lock_release(rwlock->rw_lock);
}
void
rwlock_release_read(struct rwlock *rwlock)
{
	KASSERT(rwlock != NULL);
	KASSERT(rwlock->num_reader > 0);
	lock_acquire(rwlock->rw_lock);
	if(rwlock->num_reader > 0)
	{
		rwlock->num_reader--;
	}
	wchan_wakeall(rwlock->wlock_wchan);
	wchan_wakeall(rwlock->rlock_wchan);
	lock_release(rwlock->rw_lock);
}
void
rwlock_acquire_write(struct rwlock *rwlock)
{
	KASSERT(rwlock != NULL);
	lock_acquire(rwlock->rw_lock);
	while ((rwlock->num_reader>0) || (rwlock->num_writer > 0))
	{
		wchan_lock(rwlock->wlock_wchan);
		lock_release(rwlock->rw_lock);
		wchan_sleep(rwlock->wlock_wchan);
		lock_acquire(rwlock->rw_lock);
	}
	rwlock->num_writer++;
	lock_release(rwlock->rw_lock);
}
void
rwlock_release_write(struct rwlock *rwlock)
{
	KASSERT(rwlock != NULL);
	KASSERT(rwlock->num_writer > 0);
	lock_acquire(rwlock->rw_lock);
	rwlock->num_writer--;
	wchan_wakeall(rwlock->rlock_wchan);
	wchan_wakeall(rwlock->wlock_wchan);
	lock_release(rwlock->rw_lock);
}
