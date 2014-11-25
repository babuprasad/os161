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

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <thread.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <synch.h>
#include <clock.h>

/*
 * Working VM which is carved out of VM assignment :)
 * Author : Babu
 */

/* under dumbvm, always have 48k of user stack */
//#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

struct spinlock spnlock_coremap;

uint32_t vol_pagefree =  0;

//uint32_t P_INVAL = 0;
//uint32_t P_READ = 1;
//uint32_t P_WRITE = 2;

/*
 * flag to check whether the vm_bootstrap has been initialized
 */
bool is_vm_bootstrapped = false;

void
vm_bootstrap(void)
{
	paddr_t paddr_first = 0;
	paddr_t paddr_last = 0;
	paddr_t paddr_free = 0;

	spinlock_init(&spnlock_coremap);

	/* Initialize coremap */
	ram_getsize(&paddr_first, &paddr_last);


	//page_num = ROUNDUP(paddr_last, PAGE_SIZE) / PAGE_SIZE;// :| Why rounddown?
	//page_num = ROUNDDOWN(paddr_last, PAGE_SIZE) / PAGE_SIZE;
	paddr_t ramsize = (paddr_last - paddr_first);
	page_num =  (ramsize - (ramsize % 4)) / PAGE_SIZE;


	/* pages should be a kernel virtual address !!  */
	pages = (struct page *) PADDR_TO_KVADDR(paddr_first);
	paddr_free = paddr_first + (page_num * CME_SIZE); // core map size

	// Mapping coremap elements(page table entry - PTE) to their respective virtual memory.
	paddr_t tmp_addr = paddr_first;
	for (uint32_t i = 0; i < page_num; i++)
	{
		(pages + i)-> virtual_addr = PADDR_TO_KVADDR(tmp_addr);
		(pages + i)->addrspce = NULL;
		(pages + i)-> num_pages = 1;
		(pages + i)-> timestamp = 0xffffffff;
		if(tmp_addr < paddr_free)
			(pages + i)-> page_state = FIXED;
		else
			(pages + i)-> page_state = FREE;
		tmp_addr += PAGE_SIZE;
	}

	is_vm_bootstrapped = true;

}

/**
 * Get pages by stealing memory from ram.
 * Invoked during boot sequence as the vm might have not been initialized.
 * Should never be called once vm is initialized
 * Modified by : Babu
 */
static
paddr_t
getppages(unsigned long npages)
{
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);

	return addr;
}

/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(int npages)
{

	vaddr_t alloc_mem = 0;
	if(is_vm_bootstrapped)
	{
		alloc_mem = page_nalloc(npages);
	}
	else
	{
		if(getppages(npages) != 0)
			alloc_mem = PADDR_TO_KVADDR(getppages(npages));
		else
			panic("ERROR : Zero address returned.");
	}
	return alloc_mem;
}

void 
free_kpages(vaddr_t addr)
{
	//kprintf("before spin");

	spinlock_acquire(&spnlock_coremap);

	struct page *find_page = pages;

	uint32_t page_cnt = 0;
	//while(find_page->virtual_addr != addr)
	for(page_cnt=0;page_cnt < page_num;page_cnt++)
	{
		if(find_page->virtual_addr == addr)
			break;
		find_page ++;
	}

//	kprintf("free_k : 1");

	//Check whether this is the page we are looking for to de-allocate.
	//KASSERT(find_page->virtual_addr == addr);
	if(find_page->virtual_addr != addr)
	{
		//lock_release(lock_coremap);
		spinlock_release(&spnlock_coremap);
		return;
	}

	//kprintf("free_k : 2");
	// if the address to be freed belongs to kernel then dont do anything
	// else free it.

	int no_dealloc = find_page->num_pages;
	for (int i = 0; i < no_dealloc; ++i)
	{
		find_page->page_state =  FREE;
		find_page->timestamp = 0;
		find_page->num_pages = 1;
		//as_destroy(find_page->addrspce);
		find_page ++;

	}

	//kprintf("free_k : 3");

	spinlock_release(&spnlock_coremap);
	return;
	//kprintf("after spin");


}


void
page_free(vaddr_t addr)
{
	spinlock_acquire(&spnlock_coremap);

	struct page *find_page = pages;

	uint32_t page_cnt = 0;
	//while(find_page->virtual_addr != addr)
	for(page_cnt=0;page_cnt < page_num;page_cnt++)
	{
		if(find_page->virtual_addr == addr)
			break;
		find_page ++;
	}

	//Check whether this is the page we are looking for to de-allocate.
	//KASSERT(find_page->virtual_addr == addr);
	if(find_page->virtual_addr != addr)
	{
		spinlock_release(&spnlock_coremap);
		return;
	}

	// if the address to be freed belongs to kernel then dont do anything
	// else free it.
	if(find_page->page_state != FIXED)
	{
		int32_t no_dealloc = find_page->num_pages;
		for (int32_t i = 0; i < no_dealloc; ++i)
		{
			find_page->page_state =  FREE;
			find_page->timestamp = 0;
			find_page->num_pages = 1;
			//as_destroy(find_page->addrspce); user space ---??

			find_page ++;;
		}

	}
	else
		kprintf("kernel page. cannot deallocate");

	spinlock_release(&spnlock_coremap);

	//kprintf("after spin");


}


/**
 * page_alloc - Page allocation for user program
 */
vaddr_t
page_alloc(struct addrspace *as, vaddr_t virtual_addr)
{
	spinlock_acquire(&spnlock_coremap);

	/**
	 * Find some free memory and try to allocate to the caller
	 */
	struct page *free_page = pages;

	//kprintf("page_n : 1");
	uint32_t start_page = 0;

	uint32_t free_page_cnt = 0;
	for(start_page=0;start_page < page_num;start_page++)
	{
		if(free_page->page_state == FREE)
		{
			free_page_cnt++;
			break;
		}
		free_page ++;
	}

	if(free_page_cnt == 0)
	{
		//kprintf("page not free.. so oldest one..");
		free_page = pages;
		uint64_t oldest_page_timestamp = 0xffffffff;
		uint32_t oldest_page_num = 0, i;
		for(i=0;i < page_num; i++)
		{
			// TODO : allocate the pages if its not fixed untill swapping is implemented
			/*Logic for page swapping - FIFO*/
			if(free_page -> timestamp < oldest_page_timestamp && free_page->page_state != FIXED)
			{
				oldest_page_timestamp = free_page -> timestamp;
				oldest_page_num = i;
			}
			free_page ++;
		}

		start_page = oldest_page_num;
		free_page_cnt = 1;
	}


	free_page = pages + start_page;

	// Make the entire page available
	make_page_avail(free_page);

	free_page->addrspce = as;
	free_page->num_pages = 1;
	free_page->vaddr = virtual_addr;

	time_t current_time = -1;
	uint32_t nanosec;
	gettime(&current_time, &nanosec);
	if(current_time != -1)
		free_page->timestamp = current_time;
	else
	{
		spinlock_release(&spnlock_coremap);
		panic("time error");
	}


	spinlock_release(&spnlock_coremap);


	return free_page->virtual_addr;
}

vaddr_t
page_nalloc(unsigned long npages)
{
	//kprintf("before spin");


	spinlock_acquire(&spnlock_coremap);
	/**
	 * Find some free memory and try to allocate to the caller
	 */
	struct page *free_page = pages;

	//kprintf("page_n : 1");
	uint32_t start_page = 0;
	uint32_t free_page_cnt = 0;
	for(start_page=0;start_page < page_num;start_page++)
	{
		if(free_page->page_state == FREE)
		{
			for (free_page_cnt=1; free_page_cnt<npages; free_page_cnt++)
			{
				free_page ++;
				if(free_page->page_state != FREE)
					break;
			}

			if(free_page_cnt == npages)
				break;
			else
				start_page += free_page_cnt - 1;
		}
		free_page ++;
	}


	if(free_page_cnt == 0)
	{
		//kprintf("page not free.. so oldest one..");
		free_page = pages;
		uint64_t oldest_page_timestamp = 0xffffffff;
		uint32_t oldest_page_num = 0, i;
		for(i=0;i < page_num; i++)
		{
			// TODO : allocate the pages if its not fixed untill swapping is implemented
			/*Logic for page swapping - FIFO*/
			if(free_page -> timestamp < oldest_page_timestamp && free_page->page_state != FIXED)
			{
				oldest_page_timestamp = free_page -> timestamp;
				oldest_page_num = i;
			}
			free_page ++;
		}


		start_page = oldest_page_num;
		free_page_cnt = npages;
	}

	//kprintf("page_n : 3");

	// Check at the end of the iteration whether we got the
	// number of pages we want
	KASSERT(free_page_cnt == npages);

	free_page = pages + start_page;

	//kprintf("page_n : 4");

	// Make the entire page available

	for (free_page_cnt=0; free_page_cnt<npages; free_page_cnt++)
	{
		make_page_avail(free_page);

		//free_page->addrspce = as_create(); // TODO : ????
		free_page->num_pages = npages;

		time_t current_time = -1;
		uint32_t nanosec;
		gettime(&current_time, &nanosec);
		if(current_time != -1)
			free_page->timestamp = current_time;
		else
		{
			spinlock_release(&spnlock_coremap);
			panic("time error");
		}

		// Zeroing the page before returning
		//bzero((void *)free_page->virtual_addr, PAGE_SIZE);

		free_page ++;

	}

	//kprintf("page_n : 5");

	// Start of n chunks of free pages
	free_page = pages + start_page;

	spinlock_release(&spnlock_coremap);

	//kprintf("after spin");

	return free_page->virtual_addr;

}


/**
 * This function will make the page available for either by evicting the page to disk
 * a.k.a swapping out or by flushing it
 * Author : Babu
 */
int32_t
make_page_avail(struct page *free_page)
{

	// as of now just making the page state as free by flushing it
	// TODO : implement swapping in this as_create function

	// Allocating it as dirty for the first time as the disk will not have a copy
	free_page->page_state = DIRTY;
	bzero((void *)free_page->virtual_addr, PAGE_SIZE);

	return 0;
}


void
vm_tlbshootdown_all(void)
{

	int i,spl;

	spl = splhigh();

	for (i=0; i<NUM_TLB; i++)
		tlb_write(TLBHI_INVALID(i),TLBLO_INVALID(),i);

	splx(spl);


}



void
vm_tlbshootdown(const struct tlbshootdown *ts)
{

	int i,spl;

	spl = splhigh();

	for (i=0; i<NUM_TLB; i++)
		tlb_write(TLBHI_INVALID(i),TLBLO_INVALID(),i);

	splx(spl);

	(void)ts;
}//

int
vm_fault(int faulttype, vaddr_t faultaddress)
{


	struct addrspace *as = curthread->t_addrspace;
	uint32_t tlb_high, tlb_low;
	paddr_t paddress;
	int spl, index;



	// get page number from MASKING variable
	faultaddress = faultaddress & PAGE_FRAME;
	KASSERT(faultaddress < MIPS_KSEG0);
//	KASSERT(as != NULL);



    //if(as_permissions(as,faultaddress) == 0)
    	//return EFAULT;

    struct pagetable *pte = as_pagetable_entry(as,faultaddress);

    KASSERT(pte != NULL);

    paddress = pte->phyaddress;

    //paddress = paddress << 12;

    //uint32_t page_perm = pte->permissions;

	switch(faulttype)
	{
		case VM_FAULT_READONLY:

			 spl = splhigh();

			tlb_low =  (paddress & TLBLO_PPAGE) | TLBLO_VALID | TLBLO_DIRTY;
			tlb_high = faultaddress & TLBHI_VPAGE;
			index = tlb_probe(faultaddress,0);

            (index < -1) ? tlb_random(tlb_high, tlb_low) : tlb_write(tlb_high, tlb_low, index);

			 splx(spl);


			 return 0;

		case VM_FAULT_READ:
			// then update tlb
			//if(page_perm == 2)
			//{


				spl = splhigh();

				tlb_low =  (paddress & TLBLO_PPAGE) | TLBLO_VALID;
				tlb_high = faultaddress & TLBHI_VPAGE;
				index = tlb_probe(faultaddress,0);

                (index < -1) ? tlb_random(tlb_high, tlb_low) : tlb_write(tlb_high, tlb_low, index);

				tlb_random(tlb_low,tlb_high);

				 splx(spl);

			//}
			///else
				//panic("read panic");

			break;


		case VM_FAULT_WRITE:

				spl = splhigh();

				tlb_low =  (paddress & TLBLO_PPAGE) | TLBLO_VALID | TLBLO_DIRTY;
				tlb_high = faultaddress & TLBHI_VPAGE;
				index = tlb_probe(faultaddress,0);

                (index < -1) ? tlb_random(tlb_high, tlb_low) : tlb_write(tlb_high, tlb_low, index);
				 splx(spl);


			break;
	}


	(void) faulttype;
	(void) faultaddress;
	return 0;
}


