#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <stdlib.h>

volatile int count;				// total seats taken
pthread_mutex_t queue_mtx;		// queue of passengers waiting to get on the train
pthread_mutex_t ent_wait_mtx;	// train should wait for a passenger to sit before letting anyone else enter
pthread_mutex_t cs_mtx;			// used for cs between passengers
pthread_mutex_t aboard_mtx;		// used to notify the train if its full
pthread_mutex_t trip_mtx;		// used to make passengers wait during trip
pthread_mutex_t unload_mtx;		// passengers should get off the train 1 by 1 when the trip has ended


void mtx_init() {

	// queue_mtx = 0
	pthread_mutex_init(&queue_mtx, NULL);
	pthread_mutex_lock(&queue_mtx);

	// ent_wait_mtx = 0
	pthread_mutex_init(&ent_wait_mtx, NULL);
	pthread_mutex_lock(&ent_wait_mtx);

	// cs_mtx = 1
	pthread_mutex_init(&cs_mtx, NULL);

	// aboard_mtx = 0
	pthread_mutex_init(&aboard_mtx, NULL);
	pthread_mutex_lock(&aboard_mtx);

	// trip_mtx = 0
	pthread_mutex_init(&trip_mtx, NULL);
	pthread_mutex_lock(&trip_mtx);

	// unload_mtx = 0
	pthread_mutex_init(&unload_mtx, NULL);
	pthread_mutex_lock(&unload_mtx);

}



void *train_func(void *arg) {
	int n;
	int i;

	n = *((int *)arg);

	while(1) {

		printf("\n------------------------------------------------\n");
		printf("Train: Train has arrived...Waiting for passengers\n");
		// let n passengers enter (1 by 1). Waits until passenger i is on before letting another one
		for (i=0; i<n; i++) {
			pthread_mutex_unlock(&queue_mtx);
			pthread_mutex_lock(&ent_wait_mtx);
		}

		// wait until no free seats are left.
		pthread_mutex_lock(&aboard_mtx);

		// travel
		printf("\nTrain: Got signal to begin trip\n");
		printf("Train: Traveling...\n");
		srand(time(NULL));
		/*sleep(rand()%6);*/

		count = 0;
		printf("\nTrain: End of trip\n");

		// let passengers get of the train (1 by 1 <-> wait for current to get off before allowing next)
		for (i=0; i<n; i++) {
			pthread_mutex_unlock(&trip_mtx);
			pthread_mutex_lock(&unload_mtx);
		}
		printf("\nTrain: All passengers down\n");
		printf("Train: Returning...");
		printf("\n------------------------------------------------\n");
		/*sleep(rand()%6);*/
	}
	return(NULL);
}



void *passenger_func(void *arg) {
	int n;
	n = *((int *)arg);

	// passengers wait until train arives and gives them permission to enter
	// train will give next permission only after the previous passenger has sat ( up(ent_wait_mtx) )
	pthread_mutex_lock(&queue_mtx);
	pthread_mutex_unlock(&ent_wait_mtx);

	// once sat, update seat counter, if train is full notify it to begin
	pthread_mutex_lock(&cs_mtx);
	count++;
	if (count == n) {
		printf("\nPassengers: We are %d, let's notify train!\n", count);
		pthread_mutex_unlock(&aboard_mtx);
	}
	pthread_mutex_unlock(&cs_mtx);

	// passengers wait until trip ends. then one-by-one, they get notified and get off the train
	// because of unload_mtx, train will wait until the passenger gets off, before letting another one leave
	pthread_mutex_lock(&trip_mtx);
	pthread_mutex_unlock(&unload_mtx);

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

	mtx_init();

	srand(time(NULL));
	pthread_create(&train, NULL, train_func, &n);
	for(i=0; i<passengers; i++) {
		pthread_create(&passenger[i], NULL, passenger_func, &n);
		/*sleep(rand()%6);*/
	}


	// main waits for some seconds
	sleep(60);
	return(0);
}
