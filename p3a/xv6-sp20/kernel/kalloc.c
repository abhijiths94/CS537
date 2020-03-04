// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "spinlock.h"

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

extern char end[]; // first address after kernel loaded from ELF file

int * alloc_pages[512] = {0} ; //array to store allocated pages
int num_alloc = 0;


void remove_alloc_page(int * page)
{
    int i =0;
    int found = -1;

    if(num_alloc == 0)
        return;

    for(i = 0; i< num_alloc; i++)
    {
        if(alloc_pages[i] == page)
        {
            found = i;
            break;
        }
    }

    if(found != -1)
    {
        for(i = found; i < num_alloc - 1; i++)
        {
            alloc_pages[i] = alloc_pages[i+1];
        }
        alloc_pages[num_alloc-1] = 0;
        num_alloc --;
    }
}


// Initialize free list of physical pages.
void
kinit(void)
{
  char *p;

  num_alloc = 0;
  
  initlock(&kmem.lock, "kmem");
  p = (char*)PGROUNDUP((uint)end);
  for(; p + (2*PGSIZE) <= (char*)PHYSTOP; p += (2*PGSIZE))
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(char *v)
{
  struct run *r;

  if((uint)v % PGSIZE || v < end || (uint)v >= PHYSTOP) 
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(v, 1, PGSIZE);

  acquire(&kmem.lock);
   
  r = (struct run*)v;
  remove_alloc_page((int*)r);
  r->next = kmem.freelist;
  kmem.freelist = r;


  r = (struct run*)((uint)v + PGSIZE);
  remove_alloc_page((int*)r);
  r->next = kmem.freelist;
  kmem.freelist = r;


  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{
  struct run *r, *r_prot;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
  {

    kmem.freelist = r->next;
    r_prot = kmem.freelist;
    
    if(r_prot)
    {
        kmem.freelist = r_prot->next;
        alloc_pages[num_alloc++] =(int*)r_prot;
        //cprintf("Allocated page %x and %x ... %d \n", r, r_prot, num_alloc);
    }
    else
    {
        kfree((char*)r);
        return (char*)r_prot;        //return null if unavailable
    }
  }
  release(&kmem.lock);
  return (char*)r_prot;
}

int kalloc_dump_allocated(int * frames, int numframes)
{
    int i;
    if((num_alloc < numframes) || (frames == NULL))
        return -1;
    else
    {
        for(i = 0; i < numframes; i++)
        {
            frames[i] = (int)alloc_pages[num_alloc - 1 - i];
        }
    }
    return 0;

}
