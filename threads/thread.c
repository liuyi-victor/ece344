#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include <stdint.h>
#include <stdbool.h>
#include "thread.h"
#include "interrupt.h"
#include <malloc.h>

/* This is the thread control block */
enum thread_state { exited, running, ready, terminated, blocked};
struct thread
{
  Tid id;
  enum thread_state state;  //the thread's current state
  ucontext_t context;
  void* stackpointer;
  struct thread *prev;
  struct thread *next;     //pointer to the next thread in ready queue or the exit queue
};
typedef struct thread threadblock;
static int count = 0;
static threadblock tcbList[THREAD_MAX_THREADS];
static threadblock *readyList = NULL;     //the head pointer to the ready queue
static threadblock *readyTail = NULL;     //the end of node of the ready list
static threadblock *run = NULL;                  //the thread that is currently running

static threadblock *exitList = NULL;
static threadblock *exitTail = NULL;
void thread_stub(void (*thread_main)(void *), void *arg);

void thread_init(void)
{
	/* your optional code here */
  tcbList[0].id = 0;
  tcbList[0].state = running;
  tcbList[0].stackpointer = NULL;
  tcbList[0].prev = NULL;
  tcbList[0].next = NULL;
  run = &tcbList[0];
  count = 1;
}

Tid thread_id()
{
  int isenabled = interrupts_set(0);
  int curr = run->id;
  interrupts_set(isenabled);
  return curr;
  //return run->id;
}

/* thread_create should create a thread that starts running the function
 * fn(arg). Upon success, return the thread identifier. On failure, return the
 * following:
 *
 * THREAD_NOMORE: no more threads can be created.
 * THREAD_NOMEMORY: no more memory available to create a thread stack. */
Tid thread_create(void (*fn) (void *), void *parg)
{
      int isenabled = interrupts_set(0);
      threadblock *temp;
      threadblock *savenext;
      for(temp = exitList;temp != NULL;)
      {
      //
	temp->state = exited;
	//(*temp).context = { 0 };
	temp->prev = NULL;
	savenext = temp->next;
	temp->next = NULL;
	free(temp->stackpointer);
	count--;
	temp = savenext;
      }
      exitList = NULL;
      exitTail = NULL;


  
  if(count >= THREAD_MAX_THREADS)
  {
    interrupts_set(isenabled);                               //always restore signal state to original value before return
    return THREAD_NOMORE;
  }
  //also need to check for enough stack space
  if(readyList == NULL)
  {
    int i = 0;
    for(; i < THREAD_MAX_THREADS; i++)
    {
      //if(tcbList[i].state != running && tcbList[i].state != ready)
      if(tcbList[i].state == exited)
	break;
    }


    
    tcbList[i].id = i;
    tcbList[i].prev = NULL;
    tcbList[i].next = NULL;
    getcontext(&(tcbList[i].context));                                    //initialize the ucontext_t structure for the thread
    tcbList[i].context.uc_mcontext.gregs[REG_RIP] = (long int)thread_stub;
    tcbList[i].context.uc_mcontext.gregs[REG_RDI] = (long int)fn;
    tcbList[i].context.uc_mcontext.gregs[REG_RSI] = (long int)parg;
    tcbList[i].state = ready;
    void *stackptr = malloc(THREAD_MIN_STACK + 25);
    tcbList[i].stackpointer = stackptr;
    unsigned long long almost = ((unsigned long long)(stackptr) + THREAD_MIN_STACK);
    int diff = almost%16;
    /*adding 1 because we want %rbp to point to an address that's 16 aligned and when stub function starts it first pushes the old %rbp to the thread's stack. 
     *Therefore the initialized %rsp is  aligned by 17*/
    //initialization of the stack pointer
    if(diff != 0)
    {
      tcbList[i].context.uc_mcontext.gregs[REG_RSP] = ((16 - diff) + almost) + 8;    //at most adding 16 = 15 + 1 to THREAD_MIN_STACK + base pointer (when diff = 1)
    }
    else
    {
      tcbList[i].context.uc_mcontext.gregs[REG_RSP] = almost + 8;                  //+8 for x86-64 system
    }
    readyList = &tcbList[i];
    readyTail = readyList;
    count = count + 1;
    interrupts_set(isenabled);                               //always restore signal state to original value before return
    return i;    //readyList->id;
  }
  else
  {
    /*
    threadblock *ptr = readyList;
    int i = readyList->id;
    int current = run->id;
    for(; ptr->next != NULL; ptr = ptr->next)
    {
      //ptr points to the current threadblock
      //i = the current thread id pointed to ptr
        if(ptr->next->id != i+1 && (i+1) != current)
	  break;
	else
	  i = ptr->next->id;   //updates i to the next thread's id in the ready queue (equivalent to i++ in most cases)
    }
    //tcbList[i+1].prev = ptr;
    threadblock *created = NULL;
    */
    int i = 0;
    for(; i < THREAD_MAX_THREADS; i++)
    {
      //if(tcbList[i].state != running && tcbList[i].state != ready)
      if(tcbList[i].state == exited)
	break;
    }
    if(i >= THREAD_MAX_THREADS)
    {
	count = THREAD_MAX_THREADS;
	interrupts_set(isenabled);                               //always restore signal state to original value before return
	return THREAD_NOMORE;
    }
    threadblock *created = &tcbList[i];
    created->id = i;
    created->state = ready;
    created->prev = readyTail;
    created->next = NULL;
    readyTail->next = created;
    readyTail = created;
      /*
    if(i+1 == current)
    {
	tcbList[current+1].id = current + 1;
	tcbList[current+1].state = ready;
	tcbList[current+1].prev = ptr;
	created = tcbList[current+1];
	//if(ptr->next == NULL)
	  // tcbList[current+1].next = NULL;
	//else
	  tcbList[current+1].next = ptr->next;
    }
    else
    {
	  tcbList[i+1].id = i + 1;
	  tcbList[i+1].state = ready;
  	  tcbList[i+1].prev = ptr;
	  //tcbList[i+1].next = NULL;
	  created = tcbList[i+1];
	  tcbList[i+1].next = ptr->next;
    }
      */
    getcontext(&(created->context));
    (created->context).uc_mcontext.gregs[REG_RIP] = (long int)thread_stub;
    (created->context).uc_mcontext.gregs[REG_RDI] = (long int)fn;
    (created->context).uc_mcontext.gregs[REG_RSI] = (long int)parg;
    void *stackptr = malloc(THREAD_MIN_STACK + 25);


    /*
    struct mallinfo minfo = mallinfo();
    int allocated_space = minfo.uordblks;
    void *stackptr = malloc(THREAD_MIN_STACK + 25);
    minfo = mallinfo();
    if ((minfo.uordblks - allocated_space) < THREAD_MIN_STACK)
    {
      printf("out of memory\n");
		assert(0);
    }
    */


    
    created->stackpointer = stackptr;
    unsigned long long almost = ((unsigned long long)(stackptr) + THREAD_MIN_STACK);
    int diff = almost%16;
    /*adding 1 because we want %rbp to point to an address that's 16 aligned and when stub function starts it first pushes the old %rbp to the thread's stack. 
     *Therefore the initialized %rsp is  aligned by 17*/
    //initialization of the stack pointer
    if(diff != 0)
    {
        (created->context).uc_mcontext.gregs[REG_RSP] = ((16 - diff) + almost) + 8;    //at most adding 16 to THREAD_MIN_STACK + base pointer when diff = 1
    }
    else
    {
        (created->context).uc_mcontext.gregs[REG_RSP] = almost + 8;
    }
    count = count + 1;
    interrupts_set(isenabled);                               //always restore signal state to original value before return
    return i;         //created->id;
  }
  //	return THREAD_FAILED;
}

Tid thread_yield(Tid want_tid)
{
      int isenabled = interrupts_set(0);         //disable signals and save the original state

  

      threadblock *temp;
      threadblock *savenext;
      for(temp = exitList;temp != NULL;)
      {
      //
	temp->state = exited;
	//(*temp).context = { 0 };
	temp->prev = NULL;
	savenext = temp->next;
	temp->next = NULL;
	free(temp->stackpointer);
	count--;
	temp = savenext;
      }
      exitList = NULL;
      exitTail = NULL;


  
    if(want_tid == THREAD_SELF || want_tid == run->id)
    {
      int self = run->id;
      interrupts_set(isenabled);                               //always restore signal state to original value before return
      return self;
    }
    else if(want_tid == THREAD_ANY)
    {
      /*
      int ret = interrupts_enabled();
      assert(!interrupts_enabled());
      ucontext_t mycontext = {0};
      getcontext(&mycontext);
     */


      
      if(readyList == NULL)
      {
	interrupts_set(isenabled);                               //always restore signal state to original value before return
	return THREAD_NONE;
      }
      /*
      threadblock *temp;
      threadblock *savenext;
      for(temp = exitList;temp != NULL;)
      {
      //
	temp->state = exited;
	//(*temp).context = { 0 };
	temp->prev = NULL;
	savenext = temp->next;
	temp->next = NULL;
	free(temp->stackpointer);
	count--;
	temp = savenext;
      }
      exitList = NULL;
      exitTail = NULL;
      */
      int next = readyList->id;    //the first thread in the ready queue
      if(run->state == exited)
      {
	  readyList->state = running;

	  //remove the designated thread from the ready queue
	  threadblock *following = readyList->next;
	  if(following != NULL)
	      following->prev = NULL;
	  else
  	      readyTail = NULL;
	  readyList = following;
	  tcbList[next].next = NULL;
	  tcbList[next].prev = NULL;

	  exitList = run;
	  exitTail = run;
	  run = &tcbList[next];
          
	  setcontext(&(tcbList[next].context));
	  return next;
      }
      else
      {
	  //ucontext_t mycontext = {0};
	  bool iscalled = false;
	  //getcontext(&mycontext);
	  getcontext(&(run->context));
	  if(iscalled)
	  {
	    interrupts_set(isenabled);                               //always restore signal state to original value before return
	    return next;
	  }
	  if(run->state == running)
	  {
	    //put the current thread to the end of the ready queue
	    readyTail->next = run;
	    run->prev = readyTail;
	    run->next = NULL;
	    readyTail = run;
	    run->state = ready;
	  }

	  //remove the designated thread from the ready queue
	  //unintr_printf("the value of next thread is %d, and state is %d\n",next,tcbList[next].state);
	  assert(tcbList[next].state == ready);
	  readyList->state = running;
	  threadblock *following = readyList->next;          //(NOT TRUE ANY MORE)at least 2 threads in the ready queue since already put the running thread to the end of the ready queue
	  if(following != NULL)
	    following->prev = NULL;
	  else
	    readyTail = NULL;
	  readyList = following;
	  tcbList[next].next = NULL;
	  tcbList[next].prev = NULL;
      
	  assert(tcbList[next].state == running);
      
	  run = &tcbList[next];
      
	  iscalled = true;
	  setcontext(&(tcbList[next].context));
	  interrupts_set(isenabled);                               //always restore signal state to original value before return
	  return next;
      }
    }
    else if(want_tid >= 0 && want_tid < THREAD_MAX_THREADS)
    {
      if(tcbList[want_tid].state != ready)
	return THREAD_INVALID;

      int next = tcbList[want_tid].id;    //the specified thread in the ready queue
      bool iscalled = false;
      getcontext(&(run->context));
      if(iscalled)
      {
	interrupts_set(isenabled);                               //always restore signal state to original value before return
	return next;
      }

      //first put the current thread to the end of the ready queue and then remove the designated thread from the ready queue
      readyTail->next = run;
      run->prev = readyTail;
      run->next = NULL;
      readyTail = run;
      run->state = ready;
      tcbList[want_tid].state = running;

      //remove the designated thread from the ready queue
      threadblock *following = tcbList[want_tid].next;
      following->prev = tcbList[want_tid].prev;
      if(tcbList[want_tid].prev != NULL)         //not the first thread in the ready queue
	(tcbList[want_tid].prev)->next = following;
      else
	readyList = following;
      tcbList[want_tid].next = NULL;
      tcbList[want_tid].prev = NULL;
      run = &tcbList[want_tid];
      
      iscalled = true;
      setcontext(&(tcbList[want_tid].context));
      interrupts_set(isenabled);                               //always restore signal state to original value before return
      return next;
    }
    else
    {
      interrupts_set(isenabled);                               //always restore signal state to original value before return
      return THREAD_INVALID;
    }
    //return THREAD_FAILED;
}

Tid thread_exit()
{
  /*select another thread to run from the ready queue and put the current thread to the exit queue*/
  int isenabled = interrupts_off();
  if(readyList == NULL)
  {
    interrupts_set(isenabled);                               //always restore signal state to original value before return
    return THREAD_NONE;
  }
  /*
  if(exitList == NULL)
  {
      exitList = run;
      exitTail = run;
      run->prev = NULL;
      run->next = NULL;
  }
  else
  {
      exitTail->next = run;
      run->prev = exitTail;
      run->next = NULL;
      exitTail = run;
  }
  */
  run->state = exited;
  return thread_yield(THREAD_ANY);
  //return THREAD_FAILED;
}

Tid thread_kill(Tid tid)
{
  int isenabled = interrupts_set(0);
  if(tid < 1 || tid >= THREAD_MAX_THREADS || tcbList[tid].state != ready)
  {
    interrupts_set(isenabled);                               //always restore signal state to original value before return
    return THREAD_INVALID;
  }
  if(tcbList[tid].prev == NULL && tcbList[tid].next == NULL)
  {
      readyList = NULL;
      readyTail = NULL;
  }
  else
  {
      if(tcbList[tid].prev != NULL)
	(tcbList[tid].prev)->next = tcbList[tid].next;
      else
	readyList = tcbList[tid].next;
      if(tcbList[tid].next != NULL)
	(tcbList[tid].next)->prev = tcbList[tid].prev;
      else
	readyTail = tcbList[tid].prev;
  }

  tcbList[tid].state = exited;
  tcbList[tid].prev = NULL;
  tcbList[tid].next = NULL;
  free(tcbList[tid].stackpointer);
  tcbList[tid].stackpointer = NULL;
  //tcbList[tid].context = {0};
  count = count - 1;
  interrupts_set(isenabled);                               //always restore signal state to original value before return
  return tid;
  //return THREAD_FAILED;
}
void thread_stub(void (*thread_main)(void *), void *arg)
{
	Tid ret;
	interrupts_on();
	thread_main(arg); // call thread_main() function with arg
	ret = thread_exit();
	// we should only get here if we are the last thread. 
	assert(ret == THREAD_NONE);
	// all threads are done, so process should exit
	exit(0);
}
/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* This is the wait queue structure */
struct wait_queue
{
	/* ... Fill this in ... */
  threadblock *blockedList;
  threadblock *blockedTail;
};

struct wait_queue *wait_queue_create()
{
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	//TBD();
	wq->blockedList = NULL;
	wq->blockedTail = NULL;

	return wq;
}

void wait_queue_destroy(struct wait_queue *wq)
{
  //TBD();
  assert(wq->blockedList == NULL);
	free(wq);
}

Tid thread_sleep(struct wait_queue *queue)
{
  //thread is currently running
  //1. put current thread into blocked wait queue
  //2. choose a ready thread to run
  int isenabled = interrupts_set(0);
  if(queue == NULL)
  {
      interrupts_set(isenabled);                               //always restore signal state to original value before returninterrupts_set(isenabled);
      return THREAD_INVALID;
  }
  else if(readyList == NULL)
  {
    assert(readyTail == NULL);
      interrupts_set(isenabled);                               //always restore signal state to original value before return
      return THREAD_NONE;
  }
  else
  {
      run->state = blocked;
      if(queue->blockedList == NULL)
      {
	  assert(queue->blockedTail == NULL);
	  queue->blockedList = run;
	  queue->blockedTail = run;
      }
      else
      {
	queue->blockedTail->next = run;
	run->prev = (queue->blockedTail);
	run->next = NULL;
	queue->blockedTail = run;
      }
      int result = thread_yield(THREAD_ANY);
      interrupts_set(isenabled);                               //always restore signal state to original value before return
      return result;
  }
  //return THREAD_FAILED;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int thread_wakeup(struct wait_queue *queue, int all)
{
  //TBD();
  int isenabled = interrupts_off();
  if(queue == NULL || queue->blockedList == NULL)
  {
    interrupts_set(isenabled);
    return 0;
  }
  //if(all == 0)
  else
  {
    threadblock *following = queue->blockedList->next;
    if(following != NULL)
      following->prev = NULL;
    else
      queue->blockedTail = NULL;
    queue->blockedList->state = ready;
    if(readyList == NULL)
    {
      readyList = queue->blockedList;
      readyTail = queue->blockedList;
      queue->blockedList->prev = NULL;
      queue->blockedList->next = NULL;
    }
    else
    {
      readyTail->next = queue->blockedList;
      queue->blockedList->prev = readyTail;
      queue->blockedList->next = NULL;
      readyTail = queue->blockedList;
    }
    queue->blockedList = following;
    if(all == 0)
    {
      interrupts_set(isenabled);
      return 1;
    }
    else
    {
      int count = 1;
      //threadblock *following;
      while(queue->blockedList != NULL)
      {
        following = queue->blockedList->next;
	if(following != NULL)
	  following->prev = NULL;
	else
	  queue->blockedTail = NULL;

	//putting the blocked thread to the end of the ready queue
	queue->blockedList->state = ready;
	readyTail->next = queue->blockedList;
	queue->blockedList->prev = readyTail;
	queue->blockedList->next = NULL;
	readyTail = queue->blockedList;

	queue->blockedList = following;        //update the head pointer of blocked List to the next thread
	count++;
      }
      interrupts_set(isenabled);
      return count;
    }
  }
}

struct lock
{
	/* ... Fill this in ... */
  int acquired;
  struct wait_queue queue;
};

struct lock *
lock_create()
{
	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	lock->acquired = 0;
	(lock->queue).blockedList = NULL;
	(lock->queue).blockedTail = NULL;
	//TBD();

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);
	assert(!lock->acquired);
	assert(!(lock->queue).blockedList);
	assert(!(lock->queue).blockedTail);
	//TBD();

	free(lock);
}

void
lock_acquire(struct lock *lock)
{
	assert(lock != NULL);
	int isenabled = interrupts_off();
	while(lock->acquired)
	  thread_sleep(&(lock->queue));
	lock->acquired = 1;
	interrupts_set(isenabled);
	//TBD();
}

void
lock_release(struct lock *lock)
{
	assert(lock != NULL);
	int isenabled = interrupts_off();
	lock->acquired = 0;
	thread_wakeup(&(lock->queue), 1);
	interrupts_set(isenabled);
	//TBD();
}

struct cv {
	/* ... Fill this in ... */
  struct wait_queue *queue;
};

struct cv *
cv_create()
{
	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);
	cv->queue = wait_queue_create();
	//TBD();

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);
	assert(!cv->queue->blockedList);
	assert(!cv->queue->blockedTail);
	wait_queue_destroy(cv->queue);
	//TBD();

	free(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);
	int isenabled = interrupts_off();
	if(!lock->acquired)
	{
	    interrupts_set(isenabled);
	    return;
	}  
	lock_release(lock);   //want other threads to make progress if the condition is met/unmet so that current thread can have the chance to run later when condition allows
	thread_sleep(cv->queue);
	lock_acquire(lock);
	interrupts_set(isenabled);
	//TBD();
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);
	int isenabled = interrupts_off();
        if(!lock->acquired)
	{
	    interrupts_set(isenabled);
	    return;
	} 
	thread_wakeup(cv->queue,0);
	interrupts_set(isenabled);
	//TBD();
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	assert(cv != NULL);
	assert(lock != NULL);
	int isenabled = interrupts_off();
        if(!lock->acquired)
	{
	    interrupts_set(isenabled);
	    return;
	} 
	thread_wakeup(cv->queue,1);
	interrupts_set(isenabled);
	//TBD();
}
