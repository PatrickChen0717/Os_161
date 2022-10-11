/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>



#define N_LORD_FLOWERKILLER 8
#define NROPES 16

/* Global Variables */
static int Cutted_rope = 0; 		/*Number of ropes been cut*/
static int thread_count; 			/*Number of thread currently running*/
static int thread_sleep;			/*Number of thread sleeping in CV*/
static bool balloon_status = false; /*Indicator of balloon set up*/

/* Data structures for rope mappings 
 *
 * Hook: Structure that hold the stake number it's attaching and hold the lock used for 
 * 		synchronize that rope. It also holds an indicator for tracking whether the rope has been cut.
 * Stack: Structure that hold a variable hook it's attaching to and an indicator 
 * 		for tracking whether the rope has been cut.
 * 
 * hook_array: Array holds all hook structures. 
 * stake_array: Array holds all stake structures. 
 * 
 */
struct hook
{
	int stake; 				/*the stake rope is attaching to*/
	volatile int status; 	/*the status of the rope, 0 = cut, 1 = not cut*/
	struct lock *hook_lk;	/*the lock for the rope on the hook*/
};

struct stake
{
	int hook; 				/*the stake rope is attaching to*/
	volatile int status; 	/*the status of the rope, 0 = cut, 1 = not cut*/
};

struct hook *hook_array;	/*Array of all hook structures*/
struct stake *stake_array;	/*Array of all stake structures*/

/* Synchronization primitives */
struct lock *Cutted_rope_lk;	/*Synchronize Cutted_rope lock*/
struct lock *Thread_lk;			/*Synchronize thread_sleep and thread_count*/
struct lock *public_lk;			/*Synchronization lock for multiple FlowerKiller acquiring the same hook lock*/
struct cv *Thread_cv;			/*Sleep queue of all threads ready to exit*/

/*
 * locking protocols:
 * 
 * Cutted_rope_lk: 
 *	Cutted_rope_lk is used to prevent race condition when threads 
 *	increment Cutted_rope concurrently.
 * 	When Marigold and Dandelion cut a rope, they must acquire the 
 * 	lock to increment Cutted_rope counter. 
 *  Balloon thread must acquire lock for constantly check if 
 * 	Cutted_rope equals to NROPES. Cutted_rope_lk are used in all 
 * 	three cases
 * 
 * Thread_lk:
 *  Thread_lk is used to prevent race condition when threads 
 *	update thread_count and thread_sleep concurrently.
 *  All running threads need to increment thread_count when they 
 * 	are forked, and decrement thread_count when they exit.
 *  All running threads need to increment thread_sleep when they 
 * 	are ready to exit.
 * 	Thread_lk must be acquired in those cases at the spawning 
 * 	phase and exit phase of all threads and release after the 
 * 	counters are updated
 * 
 * public_lk:
 *  public_lk is used to prevent deadlock senerio when multiple 
 * 	FlowerKiller attempt to acquire same pair of lock back to back.
 * 	public_lk must be acquired when FlowerKiller thread is trying to 
 *  acquire a hook lock.
 * 	public_lk will be released as soon as the hook locks are acquired.
 *  public_lk will also be released if any one of the hook lock acquire 
 *  is unsuccessful 
 * 
 * Thread_cv:
 *	Thread_cv is the sleep queue for all threads (except for main thread) 
 *	that are ready to exit.
 *	They will sleep in Thread_cv until the main threads broadcast, and 
 *	then they will exit together from thread_yield calls
 *
 * Rope_lk:
 *  Rope_lk are located inside each hook struct.
 * 	Must acquire lock when Marigold, Dandelion attempt to cut the ropes
 * 	Must acquire lock when FollowerKiller attempt to switch the stake that the ropes 
 * 	are attached to
 * 	Release when Marigold, Dandelion, FollowerKiller finished modifying the structs
 * 	
 */

/*
 * Exit condition:
 *	Exit condition is maintained by Cutted_rope. All threads will keep 
 *	executing and fetching for new ropes until Cutted_rope == NROPES
 *  Cutted_rope is initialized in the Main thread, and will increment by 
 * 	Marigold and Dandelion when they cut a rope.
 *  Cutted_rope is constantly checked by all thread using while(true) 
 * 	loops and the loops will break if the condition holds
 *  Upon exit, threads will queue in CV and wait for the main thread to 
 *  broadcast exit
 */

/*
 * Dandelion Thread:
 *
 * The execution starts when balloon_status assert true.
 * Cut a rope and hook array every run
 * If the rope is successfully cut, increment Cutted_rope and then context switch
 * When all ropes are cut, announce Dandelion thread done. (check by Cutted_rope == NROPES)
 * Then sleep in CV and wait for exit.
 */
static
void
dandelion(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	//Thread starts, Increment thread_count
	lock_acquire(Thread_lk);
	thread_count ++;
	lock_release(Thread_lk);
	kprintf("Dandelion thread starting\n");

	thread_yield();

	//Attempt to cut rope when the Cutted rope != NROPES
	while(true)
	{
		//start exection condition, wait for balloon set up finish
		if(balloon_status == true)
		{
			/*Exit condition
			 Check if all ropes are cut, if so, exit */
			lock_acquire(Cutted_rope_lk);
			if(Cutted_rope == NROPES)
			{
				lock_release(Cutted_rope_lk);
				break;
			}
			lock_release(Cutted_rope_lk);

			//Get a random hook number
			int hook_index = random() % NROPES;
			
			//Check status of the rope on hook
			if(hook_array[hook_index].status == 1)
			{ 
				lock_acquire(hook_array[hook_index].hook_lk);

				//Acquired the right lock, but rope's status has been changed by other threads
				if(hook_array[hook_index].status == 0)
				{
					lock_release(hook_array[hook_index].hook_lk);
				}
				else
				{
					kprintf("Dandelion severed rope %d\n",hook_index);

					//Cut the rope by changing the status to 0 in both its stake and hook
					hook_array[hook_index].status = 0;
					stake_array[hook_array[hook_index].stake].status = 0;

					//Increment # of ropes has been cut
					lock_acquire(Cutted_rope_lk);
					Cutted_rope ++ ;
					lock_release(Cutted_rope_lk);
					
					lock_release(hook_array[hook_index].hook_lk);
				}

			}
		}
		//Yield to other thread after each cut attempt
		thread_yield();
	}

	//Sleep Dandelion thread, ready to exit	
	lock_acquire(Thread_lk);

	//Increment thread_sleep for Main thread, indicate current thread has been sleep
	thread_sleep ++;
	kprintf("Dandelion thread done\n");
	cv_wait(Thread_cv, Thread_lk);

	//thread going to exit, decrement counter
	thread_count --;
	lock_release(Thread_lk);

	thread_exit();
}

/*
 * marigold thread:
 *
 * The execution starts when balloon_status assert true.
 * Cut a rope and stake array every run.
 * If the rope is successfully cut, increment Cutted_rope and then context switch
 * When all ropes are cut, announce Marigold thread done.
 * (check by Cutted_rope == NROPES)
 * Then sleep in CV and wait for exit.
 */
static
void
marigold(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	//Thread starts, Increment thread_count
	lock_acquire(Thread_lk);
	thread_count ++;
	lock_release(Thread_lk);
	kprintf("Marigold thread starting\n");

	thread_yield();

	//Attempt to cut rope when the Cutted rope != NROPES
	while(true)
	{
		//start exection condition, wait for balloon set up finish
		if(balloon_status == true)
		{
			/*Exit condition
			 Check if all ropes are cut, if so, exit */
			lock_acquire(Cutted_rope_lk);
			if(Cutted_rope == NROPES)
			{
				lock_release(Cutted_rope_lk);
				break;
			}
			lock_release(Cutted_rope_lk);

			//Get a random stake number
			int stake_index = random() % NROPES;

			//Check the status of rope on stake
			if(stake_array[stake_index].status == 1)
			{
				int temphook = stake_array[stake_index].hook;

				lock_acquire(hook_array[temphook].hook_lk);

				//Acquired the right lock, but rope's status has been changed by other threads
				if(stake_array[stake_index].status == 0 
					&& temphook == stake_array[stake_index].hook)
				{
					lock_release(hook_array[stake_array[stake_index].hook].hook_lk);
				}
				// Ensure the hook of the stake attached to is not changed, if so, retry
				else if(temphook != stake_array[stake_index].hook)
				{
					lock_release(hook_array[temphook].hook_lk);
				}
				else
				{
					kprintf("Marigold severed rope %d from stake %d\n", stake_array[stake_index].hook, stake_index);

					//Cut the rope by changing the status to 0 in both its stake and hook
					stake_array[stake_index].status = 0;
					hook_array[stake_array[stake_index].hook].status = 0;
					
					//Increment # of ropes has been cut
					lock_acquire(Cutted_rope_lk);
					Cutted_rope ++ ;
					lock_release(Cutted_rope_lk);

					lock_release(hook_array[stake_array[stake_index].hook].hook_lk);
				}

			}
		}
		//Yield to other thread after each cut attempt
		thread_yield();
	}

	//Sleep Marigold thread, ready to exit
	lock_acquire(Thread_lk);

	//Increment thread_sleep for Main thread, indicate current thread has been sleep
	thread_sleep ++;
	kprintf("Marigold thread done\n");
	cv_wait(Thread_cv, Thread_lk);

	//thread going to exit, decrement counter
	thread_count --;
	lock_release(Thread_lk);

	thread_exit();
}


/*
 * FlowerKiller Thread:
 *
 * The execution starts when balloon_status assert true.
 * These thread will try to switch the hooks that stakes are attaching to.
 * The switching is completed by swaping the hook that stacks are attaching to.
 * It will also switch the status of the stake, indicating if the rope on the stake has being cut.
 * When all ropes are cut or only one rope left, meaning there is no more ropes to move. 
 * (check by Cutted_rope >= NROPES-1)
 * It will complete operation. Then sleep in CV and wait for exit.
 * 
 */
static
void
flowerkiller(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	//Thread starts, Increment thread_count
	lock_acquire(Thread_lk);
	thread_count ++;
	lock_release(Thread_lk);
	kprintf("Lord FlowerKiller thread starting\n");
	
	thread_yield();

	while(true)
	{
		/*Exit condition
		 Check if all ropes are cut, if so, exit */
		lock_acquire(Cutted_rope_lk);
		if(Cutted_rope >= NROPES-1)
		{
			lock_release(Cutted_rope_lk);
			break;
		}
		lock_release(Cutted_rope_lk);
		
		//start exection condition, wait for balloon set up finish
		if(balloon_status == true)
		{
			//Randomly select two index for swap
			int stake_index1 = random() % NROPES;
			int temphook1 = stake_array[stake_index1].hook;
			int stake_index2 = random() % NROPES;
			int temphook2 = stake_array[stake_index2].hook;

			//Ensure both stake are attached with a rope; Ensure two stake are not the same
			if(stake_array[stake_index1].status == 1 
				&& stake_array[stake_index2].status == 1 
				&& stake_index1 != stake_index2)
			{
				//Prevent Dead lock by always acquiring the smaller lock
				if(temphook1<temphook2){ 
					lock_acquire(hook_array[temphook2].hook_lk);
					lock_acquire(hook_array[temphook1].hook_lk);
				}
				else{
					lock_acquire(hook_array[temphook1].hook_lk);
					lock_acquire(hook_array[temphook2].hook_lk);
				}

				//Ensure that the rope locks acquired are correct
				if(temphook1 != stake_array[stake_index1].hook 
					|| temphook2 != stake_array[stake_index2].hook)
				{
					//release locks in the order they are acquired
					if(temphook1 < temphook2){ 
						lock_release(hook_array[temphook2].hook_lk);
						lock_release(hook_array[temphook1].hook_lk);
					}
					else{
						lock_release(hook_array[temphook1].hook_lk);
						lock_release(hook_array[temphook2].hook_lk);
					}
					continue;
				}
				//Ensure the status of the stakes are not changed, if so, retry
				else if(stake_array[stake_index1].status == 0 
					|| stake_array[stake_index2].status == 0)
				{
					//release locks in the order they are acquired
					if(temphook1 < temphook2){ 
						lock_release(hook_array[temphook2].hook_lk);
						lock_release(hook_array[temphook1].hook_lk);
					}
					else{
						lock_release(hook_array[temphook1].hook_lk);
						lock_release(hook_array[temphook2].hook_lk);
					}
					continue;
				}
				else
				{
					kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\nLord FlowerKiller switched rope %d from stake %d to stake %d\n", 
							stake_array[stake_index1].hook, stake_index1, stake_index2, stake_array[stake_index2].hook, stake_index2, stake_index1);


					//swap the status of both stake
					int tempstatus = stake_array[stake_index1].status;
					stake_array[stake_index1].status = stake_array[stake_index2].status;
					stake_array[stake_index2].status = tempstatus;

					//swap the hook numbers stakes are connecting
					int rope_num = stake_array[stake_index1].hook;
					stake_array[stake_index1].hook = stake_array[stake_index2].hook;
					stake_array[stake_index2].hook = rope_num;

					//swap the stake numbers hooks are connecting
					int stake_num = hook_array[stake_array[stake_index1].hook].stake;
					hook_array[stake_array[stake_index1].hook].stake = hook_array[stake_array[stake_index2].hook].stake;
					hook_array[stake_array[stake_index2].hook].stake = stake_num;


					//release the locks and yield
					if(temphook1 < temphook2){ 
						lock_release(hook_array[temphook1].hook_lk);
						lock_release(hook_array[temphook2].hook_lk);
					}
					else{
						lock_release(hook_array[temphook2].hook_lk);
						lock_release(hook_array[temphook1].hook_lk);
					}
		
				}	
			}
		}
		thread_yield();
	}

	//complete exit stage
	lock_acquire(Thread_lk);

	//Increment thread_sleep for Main thread, indicate current thread has been sleep
	thread_sleep ++;
	cv_wait(Thread_cv, Thread_lk);
	kprintf("Lord FlowerKiller thread done\n");

	//thread going to exit, decrement counter
	thread_count --;
	lock_release(Thread_lk);

	thread_exit();
}

/*
 * Balloon Thread:
 *
 * Wait until all the ropes are cut
 * Announce Prince Dandelion escaped and exits. 
 * Then wait for exit.
 */

static
void
balloon(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	//Thread starts, Increment thread_count
	lock_acquire(Thread_lk);
	thread_count ++;
	lock_release(Thread_lk);
	kprintf("Balloon thread starting\n");

	while(true)
	{
		/*Exit condition
		 Check if all ropes are cut, if so, exit */
		lock_acquire(Cutted_rope_lk);
		if(Cutted_rope == NROPES)
		{
			lock_release(Cutted_rope_lk);
			kprintf("Balloon freed and Prince Dandelion escapes!\n");
			break;
		}
		lock_release(Cutted_rope_lk);
		thread_yield();
	}

	//Ready to exit, sleep in the CV waiting for exit
	lock_acquire(Thread_lk);

	//Increment thread_sleep for Main thread, indicate current thread has been sleep
	thread_sleep ++;
	cv_wait(Thread_cv, Thread_lk);
	kprintf("Balloon thread done\n");

	//thread going to exit, decrement counter
	thread_count --;
	lock_release(Thread_lk);

	thread_exit();
}

/*
 * Main Thread:
 *
 *	Set up the all the data sturctures for airballon senerio at start.
 * 	Set up the balloon and forking all threads
 *  Constantly context switch between thread before they exit
 *  Clean up data structures when all thread exit
 * 	Deallocate memory after use
 */

int
airballoon(int nargs, char **args)
{

	int err = 0, i;

	(void)nargs;
	(void)args;

	//Initialize General locks and CV 
	Cutted_rope_lk  = lock_create("Cutted_rope_lk");
	Thread_lk  = lock_create("Thread_lk");
	public_lk  = lock_create("public_lk");
	Thread_cv  = cv_create("Thread_cv");
	
	//Set up initial value of Cutted_rope: no rope is cut at start
	lock_acquire(Cutted_rope_lk);
	Cutted_rope = 0;
	balloon_status = false;
	lock_release(Cutted_rope_lk);

	//Set up the initial value of thread_sleep and thread_count
	lock_acquire(Thread_lk);
	thread_sleep = 0;
	thread_count = 1;
	lock_release(Thread_lk);

	//Create arrays for storing hook and stake
	hook_array = kmalloc(sizeof(struct hook) * NROPES);
	stake_array = kmalloc(sizeof(struct stake) * NROPES);

	//Store indexes, labels and rope locks into arrays 
 	for(int i=0;i<NROPES;i++)
	{
		stake_array[i].hook = i;
		hook_array[i].stake = i;
		stake_array[i].status = 1; 
		hook_array[i].status = 1;

		hook_array[i].hook_lk = lock_create("lock");
	}

	/*Set balloon_status to indicate Balloon set up complete
		The balloon_status will start the cut rope execution
		in dandelion, Marigold, and swap rope execution
		in FlowerKiller. */
	lock_acquire(Cutted_rope_lk);
	balloon_status = true;
	lock_release(Cutted_rope_lk);


	err = thread_fork("Marigold Thread",
			  NULL, marigold, NULL, 0);
	if(err)
		goto panic;

	err = thread_fork("Dandelion Thread",
			  NULL, dandelion, NULL, 0);
	if(err)
		goto panic;

	for (i = 0; i < N_LORD_FLOWERKILLER; i++) {
		err = thread_fork("Lord FlowerKiller Thread",
				  NULL, flowerkiller, NULL, 0);
		if(err)
			goto panic;
	}

	err = thread_fork("Air Balloon",
			  NULL, balloon, NULL, 0);
	if(err)
		goto panic;

	goto done;
panic:
	panic("airballoon: thread_fork failed: %s)\n",
	      strerror(err));

done:
	
	//Context switch when there are still ropes not been cut
	while(Cutted_rope < NROPES)
	{
		thread_yield();
	}

	//Wait until all threads completed and sleep
	while(thread_sleep < 3 + N_LORD_FLOWERKILLER)
	{
		thread_yield();
	}

	//Wake up all sleeping threads for exit
	lock_acquire(Thread_lk);
	cv_broadcast(Thread_cv, Thread_lk);
	lock_release(Thread_lk);

	//Exit all threads
	while(thread_count > 1)
	{
		thread_yield();
	}

	//Destory locks in hook array
	for (int i = 0; i < NROPES; i++)
	{
		lock_destroy(hook_array[i].hook_lk);
	}

	//Clean up general locks, CV, array
	cv_destroy(Thread_cv);
	lock_destroy(Thread_lk);
	lock_destroy(Cutted_rope_lk);
	lock_destroy(public_lk);
	
	kfree(hook_array);
	kfree(stake_array);
	kprintf("Main thread done\n");
	return 0;
}
