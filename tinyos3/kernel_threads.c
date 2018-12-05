
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "kernel_cc.h"


#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>
#include <setjmp.h>

#include "util.h"




/**
  @brief Create a new thread in the current process.
  */



void start_ptcb_thread()
{
	  int exitval;

	  Task call =  CURTHREAD->owner_ptcb->task;
	  int argl = CURTHREAD->owner_ptcb->argl;
	  void* args = CURTHREAD->owner_ptcb->args;

		exitval = call(argl,args);

	  sys_ThreadExit(exitval);
}


Tid_t sys_CreateThread(Task task, int argl, void* args)
{
	PTCB* ptcb = (PTCB*) malloc(sizeof(PTCB));
	rlnode_init(& ptcb->ptcb_node, ptcb);
  ptcb->exited = 0;
  ptcb->detached = 0;
  ptcb->exit_val = 0;
  ptcb->join_threads = COND_INIT;
  ptcb->ref_count = 0;
	ptcb->task = task;
	ptcb->argl = argl;
  ptcb->args = args;
	ptcb->thread = NULL;

	PCB* curproc = CURPROC;
	rlist_push_back(& curproc->ptcb_list, & ptcb->ptcb_node);


	if(task != NULL)
	{
		curproc->num_threads++;
		ptcb->thread = spawn_thread(curproc, start_ptcb_thread, ptcb);
		wakeup(ptcb->thread);
	}


	return (Tid_t) ptcb;
}



/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) CURTHREAD;
}


/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
	PCB* curproc = CURPROC;
	TCB* curthread = CURTHREAD;

	PTCB* ptcb = (PTCB*) tid;
	TCB* joined_thread = ptcb->thread;

	if(ptcb->exited != 1  &&	ptcb->detached != 1)
	{
		if(joined_thread->owner_pcb == curproc && joined_thread != curthread){
			ptcb->ref_count = 1;
			kernel_wait(& ptcb->join_threads, SCHED_USER);
			return 0;
		}
		else{
			return -1;
		}
	}
	else
		return -1;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
	PTCB* ptcb = (PTCB*) tid;
	TCB* detach_thread = ptcb->thread;
	PCB* curproc = CURPROC;

	if(ptcb->exited != 1){
		if(detach_thread->owner_pcb == curproc){
			kernel_broadcast(& detach_thread->owner_ptcb->join_threads);
			ptcb->detached = 1;
			return 0;
		}
		else{
			return -1;
		}
	}
	else
		return -1;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
	TCB* curthread = CURTHREAD;
	CURPROC->num_threads--;
	kernel_broadcast(& curthread->owner_ptcb->join_threads);
	curthread->owner_ptcb->exited = 1;
	kernel_sleep(EXITED, SCHED_USER);

}
