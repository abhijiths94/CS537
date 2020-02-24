#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "pstat.h"

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

/* Queue for MLFQ */
struct proc* q3[NPROC];
struct proc* q2[NPROC];
struct proc* q1[NPROC];
struct proc* q0[NPROC];

void copy_pstat(struct pstat* pstat_ptr)
{
    int i;
    for(i = 0; i< NPROC; i++)
    {
        pstat_ptr->inuse[i] = ptable.proc[i].inuse;
        pstat_ptr->pid[i] = ptable.proc[i].pid;
        pstat_ptr->priority[i] = ptable.proc[i].priority;
        pstat_ptr->state[i] = ptable.proc[i].state;
        pstat_ptr->ticks[i][0] = ptable.proc[i].ticks_accum[0];
        pstat_ptr->ticks[i][1] = ptable.proc[i].ticks_accum[1];
        pstat_ptr->ticks[i][2] = ptable.proc[i].ticks_accum[2];
        pstat_ptr->ticks[i][3] = ptable.proc[i].ticks_accum[3];

        pstat_ptr->wait_ticks[i][0] = ptable.proc[i].wait_ticks[0];
        pstat_ptr->wait_ticks[i][1] = ptable.proc[i].wait_ticks[1];
        pstat_ptr->wait_ticks[i][2] = ptable.proc[i].wait_ticks[2];
        pstat_ptr->wait_ticks[i][3] = ptable.proc[i].wait_ticks[3];
    }
}


/* Num of processes in each level */
int q3_size = 0, q2_size = 0, q1_size = 0, q0_size = 0;

int find_index(struct proc **q, int q_size, struct proc *ele)
{
    int i;
    for(i = 0; i < q_size; i++)
    {
        ////cprintf("Searching for %x in %x...\n", ele, q[i]);
        if(q[i] == ele)
        {
            return i;
        }
    }
    cprintf("!!!! Error element not in queue !!!\n");
    return -1;
}

int delete_entry(struct proc **q, int *q_size, int pos)
{
    int i;
    if(pos == 0 || *q_size == 1)
    {
        q[pos] = 0;
    }
    else if(pos != *q_size -1)
    {
        for(i = pos; i< *q_size; i++)
            q[i] = q[i+1];
    }
   
    q[*q_size] = 0;
    *q_size = *q_size - 1;

    return 0;
}

int add_entry(struct proc **q, int *q_size, struct proc * ele)
{
    q[*q_size] = ele;
    *q_size = *q_size + 1;
    return 0;
}

void accum_wait_ticks(struct proc *p)
{
    struct proc* tp = ptable.proc;
    for(tp = ptable.proc; tp < &ptable.proc[NPROC]; tp++)
    {
        if((tp != p) && (tp->inuse) && (tp->state == RUNNABLE))
        {
            tp->wait_ticks[tp->priority] ++;
        }
    }
}

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;


  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);


  /* Add to queue */

  p->inuse = 1;
  p->priority = 3;

  p->wait_ticks[0] = 0;
  p->wait_ticks[1] = 0;
  p->wait_ticks[2] = 0;
  p->wait_ticks[3] = 0;

  p->ticks_accum[0] = 0;
  p->ticks_accum[1] = 0;
  p->ticks_accum[2] = 0;
  p->ticks_accum[3] = 0;
  p->rr_time = 0;


 // cprintf("Allocproc called .. %x \n", p);

  // Allocate kernel stack if possible.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;
  
  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;
  
  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];
  
  p = allocproc();
  acquire(&ptable.lock);
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  /* Add userinit to queue */
  q3[q3_size++] = p;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  
  sz = proc->sz;
  if(n > 0){
    if((sz = allocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(proc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  proc->sz = sz;
  switchuvm(proc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;

  //cprintf("Fork called by %d .....\n", proc->pid);
  // Allocate process.
  if((np = allocproc()) == 0)
    return -1;

  //cprintf("Fork created %d .....\n", np->pid);
  
  // Copy process state from p.
  if((np->pgdir = copyuvm(proc->pgdir, proc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = proc->sz;
  np->parent = proc;
  *np->tf = *proc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(proc->ofile[i])
      np->ofile[i] = filedup(proc->ofile[i]);
  np->cwd = idup(proc->cwd);
 
  pid = np->pid;
  np->state = RUNNABLE;
  safestrcpy(np->name, proc->name, sizeof(proc->name));
  
  q3[q3_size] = np;
  q3_size++;
  
  //cprintf("fork called for %s... pid : %d at %x : parent = %d \n",np->name, np->pid, np, proc->pid);
  
  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(void)
{
  struct proc *p;
  int fd;

  if(proc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(proc->ofile[fd]){
      fileclose(proc->ofile[fd]);
      proc->ofile[fd] = 0;
    }
  }

  iput(proc->cwd);
  proc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(proc->parent);

  //cprintf("Called exit for %s... pid : %d in %x \n",proc->name, proc->pid, proc);

  int ind;

  if(proc->priority == 3)
  {
    ind = find_index(q3, q3_size, proc);
    if(ind != -1)
    {
        delete_entry(q3, &q3_size, ind);
    }
  }
  else if(proc->priority == 2)
  {
    ind = find_index(q2, q2_size, proc);
    if(ind != -1)
    {
        delete_entry(q2, &q2_size, ind);
    }
  }
  else if(proc->priority == 1)
  {
    ind = find_index(q1, q1_size, proc);
    if(ind != -1)
    {
        delete_entry(q1, &q1_size, ind);
    }
  }
  else
  {
    ind = find_index(q0, q0_size, proc);
    if(ind != -1)
    {
        delete_entry(q0, &q0_size, ind);
    }
  }

  proc->inuse = 0;

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == proc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  proc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(void)
{
  struct proc *p;
  int havekids, pid;

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for zombie children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != proc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->state = UNUSED;
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || proc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(proc, &ptable.lock);  //DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p;


  int index;
  int count;
  for(;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);

    /*
    for(tp = ptable.proc; tp < &ptable.proc[NPROC]; tp++)
    {
        if(tp->state == RUNNABLE)
            count++;
    }*/

    if(1) //q3_size > 0)
    {
        count = 0;
        for(index = 0; index < NPROC; index ++)
        {
            p = q3[index]; 
            if(p == 0 || p == NULL)
            {
                count ++;
                continue;
            }
            if(p->state != RUNNABLE)
            {
                count ++;
                continue;
            }

            p->rr_time = 0;
            while(p->rr_time < 1)
            {
                // Switch to chosen process.  It is the process's job
                // to release ptable.lock and then reacquire it
                // before jumping back to us.
                proc = p;
                switchuvm(p);
                p->state = RUNNING;
                //cprintf("q3 : Starting to run pid %s : %d out of %d process : %d %d \n",p->name, p->pid, count, proc->ticks_accum[proc->priority], proc->wait_ticks[proc->priority]);
                
                swtch(&cpu->scheduler, proc->context);
                switchkvm();

                // Process is done running for now.
                // It should have changed its p->state before coming back.
                //
                proc->ticks_accum[proc->priority] += 1; 
                accum_wait_ticks(proc);
                proc->rr_time += 1;
            }
            //Check accum ticks and demote
            if(proc->ticks_accum[3] == 8)
            {
                add_entry(q2, &q2_size, proc);
                delete_entry(q3, &q3_size, index);
                proc->priority = 2;
                //cprintf("Deleting entry at %d  and adding to q2 : q3_size = %d\n",index, q3_size);
            }
            
            goto endloop;

        }
    }
    if(count == NPROC) //q3 has no runnable process
    {
        count = 0;
        for(index = 0; index < NPROC; index ++)
        {
            p = q2[index]; 
            if(p == 0 || p == NULL)
            {
                count ++;
                continue;
            }
            if(p->state != RUNNABLE)
            {
                count ++;
                continue;
            }

            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            proc = p;
            switchuvm(p);
            p->state = RUNNING;
            //cprintf("q2 : Starting to run pid %s : %d out of %d process : %d %d \n",p->name, p->pid, count, proc->ticks_accum[proc->priority], proc->wait_ticks[proc->priority]);
            
            swtch(&cpu->scheduler, proc->context);
            switchkvm();

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            //
            proc->ticks_accum[proc->priority] += 1; 
            accum_wait_ticks(proc);

            //Check accum ticks and demote
            if(proc->ticks_accum[2] == 16)
            {
                add_entry(q1, &q1_size, proc);
                delete_entry(q2, &q2_size, index);
                proc->priority = 1;
                //cprintf("Deleting entry and adding to q1 : q2_size = %d\n", q2_size);
            }
            
            goto endloop;

        }
    }
    if(count == NPROC) //q2 is empty 
    {
        count = 0;
        for(index = 0; index < NPROC; index ++)
        {
            p = q1[index]; 
            if(p == 0 || p == NULL)
            {
                count ++;
                continue;
            }
            if(p->state != RUNNABLE)
            {
                count ++;
                continue;
            }

            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            proc = p;
            switchuvm(p);
            p->state = RUNNING;
            //cprintf("q1 : Starting to run pid %s : %d out of %d process : %d %d \n",p->name, p->pid, count, proc->ticks_accum[proc->priority], proc->wait_ticks[proc->priority]);
            
            swtch(&cpu->scheduler, proc->context);
            switchkvm();

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            //
            proc->ticks_accum[proc->priority] += 1; 
            accum_wait_ticks(proc);

            //Check accum ticks and demote
            if(proc->ticks_accum[1] == 32)
            {
                add_entry(q0, &q0_size, proc);
                delete_entry(q1, &q1_size, index);
                proc->priority = 0;
                //cprintf("Deleting entry and adding to q0 : q1_size = %d\n", q1_size);
            }
            
            goto endloop;

        }
    }
    if(count == 64) //q1 was empty 
    {
        count = 0;
        for(index = 0; index < NPROC ; index ++)
        {
            p = q0[index]; 
            if(p == NULL || p == 0)
            {
                continue;
            }
            if(p->state != RUNNABLE)
            {
                continue;
            }

            // Switch to chosen process.  It is the process's job
            // to release ptable.lock and then reacquire it
            // before jumping back to us.
            proc = p;
            switchuvm(p);
            p->state = RUNNING;
            //cprintf("q0 : Starting to run pid %s : %d out of %d process : %d %d \n",p->name, p->pid, count, proc->ticks_accum[proc->priority], proc->wait_ticks[proc->priority]);
            
            swtch(&cpu->scheduler, proc->context);
            switchkvm();

            // Process is done running for now.
            // It should have changed its p->state before coming back.
            //
            proc->ticks_accum[proc->priority] += 1; 
            accum_wait_ticks(proc);

        }

    }
endloop:
    release(&ptable.lock);

  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state.
void
sched(void)
{
  int intena;

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(cpu->ncli != 1)
    panic("sched locks");
  if(proc->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = cpu->intena;
  swtch(&proc->context, cpu->scheduler);
  cpu->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  acquire(&ptable.lock);  //DOC: yieldlock
  proc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);
  
  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{

  if(proc == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }

  // Go to sleep.

  //cprintf("pid %d going to sleep ...\n", proc->pid);
/* 
  //delete entry from queue
  if(proc->priority == 3)
  {
      ind = find_index(q3, q3_size, proc);
      delete_entry(q3, &q3_size, ind);
  }
  else if(proc->priority == 2)
  {
      cprintf("---- Deleting from q \n");
      ind = find_index(q2, q2_size, proc);
      delete_entry(q2, &q2_size, ind);
      cprintf("size = %d", q2_size);
  }
  else if(proc->priority == 1)
  {
      ind = find_index(q1, q1_size, proc);
      delete_entry(q1, &q1_size, ind);
  }
  else // (proc->priority == 0)
  {
      ind = find_index(q0, q0_size, proc);
      delete_entry(q0, &q0_size, ind);
  }
*/
  proc->chan = chan;
  proc->state = SLEEPING;
  sched();

  // Tidy up.
  proc->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
    {
      //cprintf("Wakeup called ...\n");
      p->state = RUNNABLE;  
  
/*
      //add entry into queue
      if(proc->priority == 3)
      {
          add_entry(q3, &q3_size, p);
      }
      else if(proc->priority == 2)
      {
          add_entry(q2, &q2_size, p);
      }
      else if(proc->priority == 1)
      {
          add_entry(q1, &q1_size, p);
      }
      else // (proc->priority == 0)
      {
          add_entry(q0, &q0_size, p);
      }
      */
    }
}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];
  
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}


