#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>
#include <errno.h>

volatile int count;				// total seats taken
pthread_mutex_t queue_mtx;		// queue of passengers waiting to get on the train
pthread_mutex_t ent_wait_mtx;	// train should wait for a passenger to sit before letting anyone else enter
pthread_mutex_t cs_mtx;			// used for cs between passengers
pthread_mutex_t aboard_mtx;		// used to notify the train if its full
pthread_mutex_t trip_mtx;		// used to make passengers wait during trip
pthread_mutex_t unload_mtx;		// passengers should get off the train 1 by 1 when the trip has ended


void mtx_init() {

	// queue_mtx = 0
	// passengers should block if train is not there
	if (pthread_mutex_init(&queue_mtx, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}
	if (pthread_mutex_lock(&queue_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	// ent_wait_mtx = 0
	// used to prevent train from calling mutex_unlock for no reason (chained passenger unblocking)
	if (pthread_mutex_init(&ent_wait_mtx, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}
	if (pthread_mutex_lock(&ent_wait_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	// cs_mtx = 1
	if (pthread_mutex_init(&cs_mtx, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}

	// aboard_mtx = 0
	// used to block train until all passengers are ready for the trip
	// train is unblocked by last passenger
	if (pthread_mutex_init(&aboard_mtx, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}
	if (pthread_mutex_lock(&aboard_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	// trip_mtx = 0
	// passengers block here and wait to be unblocked by train to unload
	if (pthread_mutex_init(&trip_mtx, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}
	if (pthread_mutex_lock(&trip_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	// unload_mtx = 0
	// like ent_wait_mtx we do not want our train to call too many times mutex_unlock
	// before making sure that passenger has successfully unblocked
	if (pthread_mutex_init(&unload_mtx, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}
	if (pthread_mutex_lock(&unload_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

}



void *train_func(void *arg) {
	int n;
	int i;

	n = *((int *)arg);
	srand(time(NULL));

	while(1) {

		printf("\n------------------------------------------------\n");
		printf("Train: Train has arrived...Waiting for passengers\n");
		// let n passengers enter (1 by 1). Waits until passenger i is on before letting another one
		for (i=0; i<n; i++) {
			// up(queue_mtx);
			if (pthread_mutex_unlock(&queue_mtx)) {
				perror("pthread_mutex_unlock");
				exit(1);
			}
			// down(ent_wait_mtx);
			if (pthread_mutex_lock(&ent_wait_mtx)) {
				perror("pthread_mutex_lock");
				exit(1);
			}
		}

		// wait until no free seats are left.
		// down(aboard_mtx);
		if (pthread_mutex_lock(&aboard_mtx)) {
			perror("pthread_mutex_lock");
			exit(1);
		}

		// travel
		printf("\nTrain: Got signal to begin trip\n");
		printf("Train: Traveling...\n");
		sleep(rand()%4+1);

		count = 0;
		printf("\nTrain: End of trip\n");

		// let passengers get off the train (1 by 1 <-> wait for current to get off before allowing next)
		for (i=0; i<n; i++) {
			// up(trip_mtx);
			if (pthread_mutex_unlock(&trip_mtx)) {
				perror("pthread_mutex_unlock");
				exit(1);
			}
			// down(unload_mtx);
			if (pthread_mutex_lock(&unload_mtx)) {
				perror("pthread_mutex_lock");
				exit(1);
			}
		}
		printf("\nTrain: All passengers down\n");
		printf("Train: Returning...");
		printf("\n------------------------------------------------\n");
		sleep(rand()%3+1);
	}
	return(NULL);
}



void *passenger_func(void *arg) {
	int n;
	n = *((int *)arg);

	// passengers wait until train arives and gives them permission to enter
	// train will give next permission only after the previous passenger has sat ( up(ent_wait_mtx) )

	// down(queue_mtx);
	if (pthread_mutex_lock(&queue_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}
	// up(ent_wait_mtx);
	if (pthread_mutex_unlock(&ent_wait_mtx)) {
		perror("pthread_mutex_unlock");
		exit(1);
	}



	// down(cs_mtx);
	if (pthread_mutex_lock(&cs_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	// once sat, update seat counter, if train is full notify it to begin
	printf("A passenger got inside the train\n");
	count++;
	if (count == n) {
		printf("\nPassengers: We are %d, let's notify train!\n", count);
		// up(aboard_mtx)
		// notify train
		if (pthread_mutex_unlock(&aboard_mtx)) {
			perror("pthread_mutex_unlock");
			exit(1);
		}
	}

	// up(cs_mtx);
	if (pthread_mutex_unlock(&cs_mtx)) {
		perror("pthread_mutex_unlock");
		exit(1);
	}

	// passengers wait until trip ends. then one-by-one, they get notified and get off the train
	// Because of unload_mtx, train will wait until the passenger gets off, before letting another one leave
	if (pthread_mutex_lock(&trip_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}
	if (pthread_mutex_unlock(&unload_mtx)) {
		perror("pthread_mutex_unlock");
		exit(1);
	}

	return(NULL);
}



int main(int argc, char *argv[]) {
	int n;				// total seats
	int i;
	pthread_t train;

	int passengers;		// used only to allocate proper memory for threads
	pthread_t *passenger;

	printf("Enter number of train seats: ");
	scanf("%d", &n);

	printf("(Note that train leaves ONLY if it has no empty seats left)\n");
	printf("Enter total passengers randomly arriving: ");
	scanf("%d", &passengers);
	passenger = (pthread_t *)malloc(passengers*sizeof(pthread_t));

	mtx_init();		// initialize mutexes

	srand(time(NULL));
	// create train thread
	if (pthread_create(&train, NULL, train_func, &n)) {
		perror("pthread_create");
		exit(1);
	}
	// create passengers threads
	for(i=0; i<passengers; i++) {
		if (pthread_create(&passenger[i], NULL, passenger_func, &n)) {
			perror("pthread_create");
			exit(1);
		}
		sleep(rand()%2+1);
	}


	// main waits for some seconds...to let us watch the simulation
	sleep(60);
	return(0);
}
