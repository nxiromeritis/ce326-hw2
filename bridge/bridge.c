#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>

#define L 0
#define R 1

volatile int ncars;			// total cars randomly arriving
volatile int n;				// how many cars can the bridge handle
volatile int crossing[2];	// how many cars are currently crossing (left/right side)
volatile int blocked[2];	// how many cars are waiting to cross (left/right side)

pthread_mutex_t cs_common_mtx;
pthread_mutex_t queue_mtx[2];	// blocks cars from crossing bridge
								// when full or when being crossed by the other side

volatile int cars_done = 0;
pthread_mutex_t main_mtx;

// initializes mutexes
void mtx_init() {

	crossing[0] = 0;
	crossing[1] = 0;
	blocked[0] = 0;
	blocked[1] = 0;

	pthread_mutex_init(&cs_common_mtx, NULL);	// cs_common_mtx = 1

	// queue_mtx[left] = 0
	pthread_mutex_init(&queue_mtx[0], NULL);
	pthread_mutex_lock(&queue_mtx[0]);

	// queue_mtx[right] = 0
	pthread_mutex_init(&queue_mtx[1], NULL);
	pthread_mutex_lock(&queue_mtx[1]);

	pthread_mutex_init(&main_mtx, NULL);
	pthread_mutex_lock(&main_mtx);
}


void *car_func(void *arg) {
	int side;

	side = *((int *)arg);
	srand(time(NULL));

	pthread_mutex_lock(&cs_common_mtx);

	// if there are cars from the other side crossing/wanting_to_cross
	// wait/give them priority
	if (crossing[!side] + blocked[!side] > 0) {

		blocked[side]++;
		pthread_mutex_unlock(&cs_common_mtx);
		pthread_mutex_lock(&queue_mtx[side]);
		// when signaled... unblock 1-by-1 only up to max_bridge_cars cars
		if (blocked[side] > 0) {
			if (crossing[side] < n) {
				blocked[side]--;
				crossing[side]++;
				/*printf("Cars on bridge: %d (+1)\n", crossing[side]);*/
				pthread_mutex_unlock(&queue_mtx[side]);
			}
		}
	}
	// if there are no cars on the other side just pass the bridge...
	else {
		// only if it is not full...
		if (crossing[side] < n) {
			crossing[side]++;
			/*printf("Cars on bridge: %d (+1)\n", crossing[side]);*/
			pthread_mutex_unlock(&cs_common_mtx);
		}
		// ...otherwise car has to wait.
		else {
			blocked[side]++;
			pthread_mutex_unlock(&cs_common_mtx);
			pthread_mutex_lock(&queue_mtx[side]);
			// when signaled.. unblock 1-by1 only up to max_bridge_cars cars
			if (blocked[side] > 0) {
				if (crossing[side] < n) {
					blocked[side]--;
					crossing[side]++;
					/*printf("Cars on bridge: %d (+1)\n", crossing[side]);*/
					pthread_mutex_unlock(&queue_mtx[side]);
				}
			}
		}
	}


	/* crossing bridge */
	sleep(rand()%5);
	printf("%s: car crossing bridge\n", (side?"right":"left"));


	pthread_mutex_lock(&cs_common_mtx);
	crossing[side]--;
	/*printf("Cars on bridge: %d (-1)\n", crossing[side]);*/

	// once all cars crossing the bridge are done...
	// give priotity to the opposite side (unblock signal)
	if ((crossing[side] == 0) && (blocked[!side] > 0)) {
		printf("%s: giving priority to other side (cars waiting there)\n", (side?"right":"left"));
		blocked[!side]--;
		crossing[!side]++;
		/*printf("Cars on bridge: %d (+1)\n", crossing[!side]);*/
		pthread_mutex_unlock(&queue_mtx[!side]);
	}
	else {
		if (crossing[side] == 0) {
			printf("%s: no other cars on the opposite side..Any car comming first will take bridge\n", (side?"right":"left"));
		}

	}
	pthread_mutex_unlock(&cs_common_mtx);


	cars_done++;
	if (cars_done == ncars) {
		pthread_mutex_unlock(&main_mtx);
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

	mtx_init();

	srand(time(NULL));

	for (i=0; i<ncars; i++) {
		if (rand()%2) {
			/*printf("right_side: new car arriving\n");*/
			pthread_create(&car_thread[i], NULL, car_func, &right);
		}
		else {
			/*printf("left_side: new car arriving\n");*/
			pthread_create(&car_thread[i], NULL, car_func, &left);
		}
		sleep(rand()%1);
	}

	pthread_mutex_lock(&main_mtx);
	printf("main: simulation finished. exiting...\n");
	return(0);
}
