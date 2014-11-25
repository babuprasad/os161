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
#include <addrspace.h>
#include <vm.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	as->table = NULL;
	as->reg = NULL;
	as->sbase = 0;
	as->stop = 0;
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}
	struct addrspace *new;
	int numRegTab = 0;
		new = as_create();
		if (new==NULL) {
			return ENOMEM;
		}
		new->hbase = old->hbase;
		new->htop = old->htop;
		new->sbase = old->sbase;
		new->stop = old->stop;
		while(old->reg != NULL)
		{
			numRegTab++;
		}
		memmove(new->reg,old->reg,(sizeof(struct region)* numRegTab));
		numRegTab = 0;
		while(old->table != NULL)
		{
			numRegTab++;
		}
		memmove(new->table,old->table,(sizeof(struct pagetable)* numRegTab));
		numRegTab = 0;
		*ret = newas;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 */
	struct region *reg;
	struct pagetable *page;

		while (as->reg != NULL)
		{
			reg = as->reg;
			as->reg = as->reg->next;
			kfree(reg);
		}

		while(as->table != NULL)
		{
			page = as->table;
			if(page->phyaddress != 0)
			{
				page_free(page->phyaddress);
				as->table = as->table->next;
				kfree(page);
			}

		}
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;  // suppress warning until code gets written
	vm_tlbshootdown_all();
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	/*
	 * Write this.
	 */

	(void)readable;
	(void)writeable;
	(void)executable;

	struct region *reg,*tmp;

	sz += vaddr & ~(vaddr_t)PAGE_FRAME; //Aligning Regions
	vaddr &= PAGE_FRAME;

		/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

		// Update heap_start
	as->hbase = vaddr + (sz/PAGE_SIZE) * PAGE_SIZE;
	as->htop = vaddr + (sz/PAGE_SIZE) * PAGE_SIZE;

		// Record region (to be used in vm_fault)
		reg = kmalloc(sizeof(struct region));
		if (reg == NULL)return ENOMEM;
		KASSERT(reg!=NULL);
		reg->viraddress = vaddr;
		reg->numpages = sz / PAGE_SIZE;
		reg->next = NULL;
		/*reg->readable = readable;
		reg->writeable = writeable;
		reg->executable = executable;*/
		if(as->reg == NULL)
			{
				as->reg = reg;
				as->reg->next = NULL;
			}
		else
			{
				tmp = as->reg;
				if (tmp && tmp->next )
				{
				while(tmp->next != NULL)
				{
					tmp = tmp->next;
				}
				tmp->next = reg;
				}
			}
		return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */
	struct region *reg;
	struct pagetable *page,*temp;
	reg = as->reg;
	while(reg!=NULL)
	{
		size_t i;
		size_t cnt = reg->numpages;
		vaddr_t reg_base = reg->viraddress;
		temp = as->table;
		if (temp !=NULL)
		{
			while(temp->next != NULL)
			{
				temp = temp->next;
			}
		}
		for(i=0;i<cnt;i++)
		{
		 	page = (struct pagetable *)kmalloc(sizeof(struct pagetable));
			KASSERT(page!=NULL);
			//virtual address being computed according to the base address for the region.
			page->viraddress = reg_base + i * PAGE_SIZE;
			page->phyaddress = page_alloc();
			page->next = NULL;
			if(temp == NULL)
			{
					as->table=temp=page;
			}
			else
			{
					temp->next = page;
					temp = temp->next;
			}
		}
		reg = reg->next;
	}
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */

	/* Initial user-level stack pointer */
	as->sbase = USERSTACK;
	as->stop =USERSTACK;
	*stackptr = USERSTACK;
	
	return 0;
}

