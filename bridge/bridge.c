#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>


volatile int ncars;			// total number of cars (randomly arriving)
volatile int n;				// how many cars can the bridge handle
volatile int crossing[2];	// how many cars are currently crossing (left/right side)
volatile int blocked[2];	// how many cars are waiting to cross (left/right side)
volatile int limit[2];		// max cars going to be unblocked (left/right side)
							// gets incemented by unlocked cars until it is equal to bridge size

pthread_mutex_t cs_common_mtx;	// cs mutex
pthread_mutex_t queue_mtx[2];	// blocks cars from crossing bridge...
								// ...when full or when being crossed by the other side

volatile int cars_done = 0;		// used only to prevent main from returning before
pthread_mutex_t main_mtx;		// the end of the simulation


// initializes mutexes
void mtx_init() {

	crossing[0] = 0;
	crossing[1] = 0;
	blocked[0] = 0;
	blocked[1] = 0;
	limit[0] = 1;
	limit[1] = 1;

	// cs_common_mtx = 1
	if (pthread_mutex_init(&cs_common_mtx, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}

	// used to block left side cars from entering
	// queue_mtx[left] = 0
	if (pthread_mutex_init(&queue_mtx[0], NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}
	if (pthread_mutex_lock(&queue_mtx[0])) {
		perror("pthread_mutex_lock");
		exit(1);
	}
	// used to block right side cars from entering
	// queue_mtx[right] = 0
	if (pthread_mutex_init(&queue_mtx[1], NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}
	if (pthread_mutex_lock(&queue_mtx[1])) {
		perror("pthread_mutex_lock");
		exit(1);
	}


	// used by main to block until the simulation ends
	// main_mtx = 0
	if (pthread_mutex_init(&main_mtx, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}
	if (pthread_mutex_lock(&main_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}
}


void *car_func(void *arg) {
	int side;

	side = *((int *)arg);
	srand(time(NULL));

	// down(cs_common_mtx)
	if (pthread_mutex_lock(&cs_common_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	// if there are cars from the other side crossing/wanting_to_cross
	// wait and give them priority
	if (crossing[!side] + blocked[!side] > 0) {

		blocked[side]++;
		// up(cs_common_mtx)
		if (pthread_mutex_unlock(&cs_common_mtx)) {
			perror("pthread_mutex_unlock");
			exit(1);
		}
		// down(queue_mtx[side])
		if (pthread_mutex_lock(&queue_mtx[side])) {
			perror("pthread_mutex_lock");
			exit(1);
		}

		// when signaled... unblock 1-by-1 only up to max_bridge_cars cars
		// last car unblocked unblocks next
		// up to n cars are getting unblocked... (this limit solves a starvation problem)
		if ((blocked[side] > 0)&&(crossing[side] < n)&&(limit[side] < n)){
			blocked[side]--;
			limit[side]++;
			crossing[side]++;

			// unblocks next car
			// up(queue_mtx)
			if (pthread_mutex_unlock(&queue_mtx[side])) {
				perror("pthread_mutex_unlock");
				exit(1);
			}
		}
		else {

			// the car crossing the bridge that decides to unblock the other lane  or
			// its lane does not call up(cs_common_mtx) to prevent newcoming cars
			// running code at the same time with the unblocking ones

			// up(cs_common_mtx) is called here by the last unblocked car
			if (pthread_mutex_unlock(&cs_common_mtx)) {
				perror("pthread_mutex_unlock");
				exit(1);
			}
		}
	}
	// if there are no cars on the other side just pass the bridge...
	else {
		// ...but only if it is not full...
		if (crossing[side] < n) {
			crossing[side]++;

			// up(cs_common_mtx);
			if (pthread_mutex_unlock(&cs_common_mtx)) {
				perror("pthread_mutex_unlock");
				exit(1);
			}
		}
		// ...otherwise car has to wait (block).
		else {
			blocked[side]++;
			// up(cs_common_mtx)
			if (pthread_mutex_unlock(&cs_common_mtx)) {
				perror("pthread_mutex_unlock");
				exit(1);
			}
			// down(queue_mtx[side])
			if (pthread_mutex_lock(&queue_mtx[side])) {
				perror("pthread_mutex_lock");
				exit(1);
			}
			// when signaled.. unblock 1-by1 only up to max_bridge_cars cars
			// this 'limit' solves a starvation problem
			if ((blocked[side] > 0)&&(crossing[side] < n)&&(limit[side] < n)){
				blocked[side]--;
				limit[side]++;
				crossing[side]++;

				// up(queue_mtx[side]
				if (pthread_mutex_unlock(&queue_mtx[side])) {
					perror("pthread_mutex_unlock");
					exit(1);
				}
			}
			else {
				// last unblocked care calls 'up'
				// up(cs_common_mtx) in this else for the same reason as before
				if (pthread_mutex_unlock(&cs_common_mtx)) {
					perror("pthread_mutex_unlock");
					exit(1);
				}
			}
		}
	}


	/* crossing bridge */
	sleep(rand()%3+1);
	printf("%s: car crossing bridge\n", (side?"right":"left"));

	// down(cs_common_mtx)
	if (pthread_mutex_lock(&cs_common_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	// one less car crossing the bridge
	crossing[side]--;

	// once all cars crossing the bridge are done...
	if ((crossing[side] == 0)) {
		// give priotity to the opposite side (unblock) if there are cars blocked
		if (blocked[!side] > 0) {
			printf("%s: giving priority to other side (cars waiting there)\n", (side?"right":"left"));
			blocked[!side]--;
			limit[!side] = 1;
			crossing[!side]++;

			// unblock the first car of the opposite side
			// up(queue_mtx[!side])
			if (pthread_mutex_unlock(&queue_mtx[!side])) {
				perror("pthread_mutex_unlock");
				exit(1);
			}
		}
		else  {
			// ...else give your lane priority
			if (blocked[side] > 0) {
				printf("%s: no other cars on the opposite side..but there are cars waiting on the same side\n", (side?"right":"left"));
				blocked[side]--;
				limit[side] = 1;
				crossing[side]++;

				// unblock the first car of the same lane
				// up(queue_mtx[side])
				if (pthread_mutex_unlock(&queue_mtx[side])) {
					perror("pthread_mutex_unlock");
					exit(1);
				}
			}
			else {
				// note that, previously no up(cs_common_mtx) was called. it is called
				// here by the last car, after it realizes there are no cars blocked generally
				// In the previous cases 'up' will be called by the last (n'th) unblocked car
				printf("%s: no waiting cars in general. first car arriving will take priority\n", (side?"right":"left"));
				if (pthread_mutex_unlock(&cs_common_mtx)) {
					perror("pthread_mutex_unlock");
					exit(1);
				}
			}
		}
	}
	else {
		// not the last car. just leave cs.
		// up(cs_common_mtx)
		if (pthread_mutex_unlock(&cs_common_mtx)) {
			perror("pthread_mutex_unlock");
			exit(1);
		}
	}


	// last car in general notifies main to terminate
	cars_done++;
	if (cars_done == ncars) {
		// up(main_mtx);
		if (pthread_mutex_unlock(&main_mtx)) {
			perror("pthread_mutex_unlock");
			exit(1);
		}
	}
	return(NULL);
}


int main(int argc, char *argv[]) {
	int left = 0;
	int right = 1;
	int i;
	pthread_t *car_thread;

	printf("\nEnter total number of cars on the bridge: ");
	scanf("%d", &n);
	printf("\nEnter total number of cars arriving: ");
	scanf("%d", &ncars);

	car_thread = (pthread_t *)malloc(ncars*sizeof(pthread_t));
	if (car_thread == NULL) {
		printf("memory allocation problems\n");
		exit(1);
	}

	// initalize mutexes
	mtx_init();

	srand(time(NULL));

	// cars arriving randomly
	for (i=0; i<ncars; i++) {
		if (rand()%2) {
			/*printf("right_side: new car arriving\n");*/
			if (pthread_create(&car_thread[i], NULL, car_func, &right)) {
				perror("pthread_create");
				exit(1);
			}
		}
		else {
			/*printf("left_side: new car arriving\n");*/
			if (pthread_create(&car_thread[i], NULL, car_func, &left)) {
				perror("pthread_create");
				exit(1);
			}
		}
		sleep(rand()%1);
	}

	// main thread blocks until simulation has ended (last car awakens it)
	// down(main_mtx);
	if (pthread_mutex_lock(&main_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	printf("main: simulation finished. exiting...\n");
	return(0);
}
