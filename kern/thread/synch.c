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
sem_create(const char *name, unsigned initial_count)
{
	struct semaphore *sem;

	sem = kmalloc(sizeof(*sem));
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

	/* Use the semaphore spinlock to protect the wchan as well. */
	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
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
		wchan_sleep(sem->sem_wchan, &sem->sem_lock);
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
	wchan_wakeone(sem->sem_wchan, &sem->sem_lock);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(*lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}

	// add stuff here as needed , adding stuff @ 1136pm 13Feb to make locks work
	lock->lk_wchan = wchan_create(lock->lk_name);
	if(lock->lk_wchan == NULL){
		kfree(lock->lk_name);
		kfree(lock);
		return NULL ;
	}
	spinlock_init(&lock->lk_lock);
	lock->thread_having_lock = NULL ;// intially no one is holding the lock
	lock->islocked = 1 ; // intially unlocked 0 means locked
  	return lock;
}

void
lock_destroy(struct lock *lock)
{
	KASSERT(lock != NULL);

	// add stuff here as needed 1246am 13Feb2016
    KASSERT(lock->thread_having_lock==NULL);
	//lock->thread_having_lock = NULL ;
     // to make sure no one is holding the lock
    spinlock_cleanup(&lock->lk_lock);
    wchan_destroy(lock->lk_wchan);
   
	kfree(lock->lk_name);

	//lock->islocked++;
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	// Write this 111am 13Feb2016
	KASSERT(lock != NULL);
	KASSERT(curthread->t_in_interrupt == false);

	spinlock_acquire(&lock->lk_lock) ;
	while(lock->islocked == 0){
		
		wchan_sleep(lock->lk_wchan, &lock->lk_lock);
		//spinlock_release(&lock->lk_lock) ;
		
		//wchan_wakeone(lock->lk_wchan, &lock->lk_lock);
		//spinlock_acquire(&lock->lk_lock) ;
	    
	}
  //  wchan_wakeone(lock->lk_wchan, &lock->lk_lock);
	// isLocked >0 initial
	KASSERT(lock->islocked >0) ; // to make sure its unlocked now
	lock->islocked = 0 ;
	lock->thread_having_lock = curthread ;// leaving the lock to the current thread
	KASSERT(lock->thread_having_lock==curthread) ;
	spinlock_release(&lock->lk_lock) ;
	//(void)lock;  // suppress warning until code gets written
}

void
lock_release(struct lock *lock)
{
	// Writing @ 423am on 14 feb 2016 Rupesh
	KASSERT(lock != NULL) ;
	KASSERT(curthread->t_in_interrupt == false);
	spinlock_acquire(&lock->lk_lock);
	
	KASSERT(lock_do_i_hold(lock)); // to check weather i have the lock or not not before releasing
	
	//lock->islocked++ ; // 0 means locked else everything is unlocked 
	lock->islocked = 1 ;
	
	KASSERT(lock->islocked == 1) ; // Making sure lock is realesed 
	lock->thread_having_lock = NULL ;
	
	wchan_wakeall (lock->lk_wchan,&lock->lk_lock);
	
	//wchan_wakeone (&lock->lk_lock);
	

	spinlock_release(&lock->lk_lock); 
	//(void)lock;  // suppress warning until code gets written
}

bool
lock_do_i_hold(struct lock *lock)
{
	// Writing @ 440am on 14 Feb 2016
	int doi=0;
	KASSERT(lock != NULL);
	KASSERT(curthread->t_in_interrupt == false);

	//spinlock_acquire(&lock->lk_lock);
	if(lock->islocked==0){

	if(lock->thread_having_lock==curthread){
		doi=1 ;
	}
	//spinlock_release(&lock->lk_lock); 
}

	//spinlock_release(&lock->lk_lock); 	
	//(void)lock;  // suppress warning until code gets written
	if(doi==1)
		{
			return true;
		}
	else{

		return false;
		} // dummy until code gets written
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(*cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstrdup(name);
	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}
	

	// add stuff here as needed
	
	cv->cv_wchan = wchan_create(cv->cv_name);
	if(cv->cv_wchan == NULL){
		kfree(cv->cv_name);
		kfree(cv);
		return NULL ;
	}
	
	spinlock_init(&cv->cv_lock);
	//lock->thread_having_lock = NULL ;// intially no one is holding the lock
	//lock->islocked = 1 ; // intially unlocked 0 means locked
  	
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);

	// add stuff here as needed
	
	spinlock_cleanup(&cv->cv_lock);
    wchan_destroy(cv->cv_wchan);

	kfree(cv->cv_name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	// Write this
	KASSERT(curthread->t_in_interrupt == false);

	spinlock_acquire(&cv->cv_lock);
	lock_release(lock);
	wchan_sleep(cv->cv_wchan, &cv->cv_lock);
	//wchan_wakeone (cv->cv_wchan,&cv->cv_lock);
	spinlock_release(&cv->cv_lock);
	lock_acquire(lock);
	
	//(void)cv;    // suppress warning until code gets written
	//(void)lock;  // suppress warning until code gets written
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	// Write this
	KASSERT(curthread->t_in_interrupt == false);
	KASSERT(lock_do_i_hold(lock));
	spinlock_acquire(&cv->cv_lock);
	wchan_wakeone (cv->cv_wchan,&cv->cv_lock);
	spinlock_release(&cv->cv_lock);	
	//(void)cv;    // suppress warning until code gets written
	//(void)lock;  // suppress warning until code gets written
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	// Write this
	KASSERT(lock_do_i_hold(lock));
	KASSERT(curthread->t_in_interrupt == false);

	spinlock_acquire(&cv->cv_lock);
	
	wchan_wakeall(cv->cv_wchan,&cv->cv_lock);
	spinlock_release(&cv->cv_lock);	
	//(void)cv;    // suppress warning until code gets written
	//(void)lock;  // suppress warning until code gets written
}

struct rwlock* rwlock_create(const char *name)
{
  (void) name;
  return NULL;

}

void
rwlock_destroy(struct rwlock *rw)
{
(void)rw;
}

void
rwlock_acquire_read(struct rwlock *rw)
{
(void) rw;

}


void
rwlock_release_read(struct rwlock *rw)
{
(void) rw;
}

void 
rwlock_acquire_write(struct rwlock *rw)
{
(void)rw;
}

void
rwlock_release_write(struct rwlock *rw)
{
(void)rw;
}

