// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "spinlock.h"
#include "rand.h"

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
int num_freeList = 0;


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
  for(; p + (PGSIZE) <= (char*)PHYSTOP; p += (PGSIZE))
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
  num_freeList ++;

  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char*
kalloc(void)
{

  struct run *r, *prev;
  acquire(&kmem.lock);
  
  int random = xv6_rand() % num_freeList;

  r = kmem.freelist;
  prev = NULL;

  for(int i = 0; i < random ; i++)
  {
      prev = r;
      r = r->next;
  }

  prev->next = r->next;

  if(r){
    num_freeList --;
    alloc_pages[num_alloc++] =(int*)r;
  }
  release(&kmem.lock);
  cprintf("Allocated page %x ", r);
  return (char*)r;

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

