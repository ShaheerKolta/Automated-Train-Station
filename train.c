#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

struct station {
  int wait_passengers; // in station waiting passengers
  int in_passengers; // in train passengers 
  pthread_mutex_t lock;
  pthread_cond_t train_arrived_cond;
  pthread_cond_t passengers_seated_cond;
  pthread_cond_t train_is_full_cond;
};


void station_init(struct station *station)
{
  station->wait_passengers = 0;
  station->in_passengers = 0;
  pthread_mutex_init(&(station->lock), NULL);
  pthread_cond_init(&(station->train_arrived_cond), NULL);
  pthread_cond_init(&(station->passengers_seated_cond), NULL);
  pthread_cond_init(&(station->train_is_full_cond), NULL);
}

/**
Loads the train with passengers. When a passenger robot arrives in a station, it first invokes this function. 
The function must not return until the train is satisfactorily loaded.
Params:
  stattion: current station pointer
  count: indicates how many seats are available on the train
*/
void station_load_train(struct station *station, int count)
{
  // Enter critical region
  pthread_mutex_lock(&(station->lock));

  while ((station->wait_passengers > 0) && (count > 0)){
    pthread_cond_signal(&(station->train_arrived_cond));
  	count--;
  	pthread_cond_wait(&(station->passengers_seated_cond), &(station->lock));
  }

  if (station->in_passengers > 0)
  	pthread_cond_wait(&(station->train_is_full_cond), &(station->lock));

  // Leave critical region
  pthread_mutex_unlock(&(station->lock));
}

/**
This function must not return until a train is in the station and there are enough free seats on
the train for this passenger. Once this function returns, the passenger robot will move the
passenger on board the train and into a seat.
Once the passenger is seated, it will call the function: station_on_board
Params:
  stattion: current station pointe
*/
void station_wait_for_train(struct station *station)
{
  pthread_mutex_lock(&(station->lock));

  station->wait_passengers++;
  pthread_cond_wait(&(station->train_arrived_cond), &(station->lock));
  station->wait_passengers--;
  station->in_passengers++;

  pthread_mutex_unlock(&(station->lock));

  pthread_cond_signal(&(station->passengers_seated_cond));
}

/**
Use this function to let the train know that it’s on board.
Params:
  stattion: current station pointer
*/
void station_on_board(struct station *station)
{
  pthread_mutex_lock(&(station->lock));

  station->in_passengers--;

  pthread_mutex_unlock(&(station->lock));

  if (station->in_passengers == 0)
  	pthread_cond_broadcast(&(station->train_is_full_cond));
}


// Count of passenger threads that have completed (i.e. station_wait_for_train
// has returned) and are awaiting a station_on_board() invocation.
volatile int threads_completed = 0;

void* passenger_thread(void *arg)
{
	struct station *station = (struct station*)arg;
	station_wait_for_train(station);
	__sync_add_and_fetch(&threads_completed, 1);
	return NULL;
}

struct load_train_args {
	struct station *station;
	int free_seats;
};

int load_train_returned = 0;

void* load_train_thread(void *args)
{
	struct load_train_args *ltargs = (struct load_train_args*)args;
	station_load_train(ltargs->station, ltargs->free_seats);
	load_train_returned = 1;
	return NULL;
}


/*
 * This creates a bunch of threads to simulate arriving trains and passengers.
 */
int main()
{
	struct station station;
	station_init(&station);

	int a=0;
	scanf("%d",&a);

    int x=0;
    scanf("%d",&x);

	// Create a bunch of 'passengers', each in their own thread.
	int i;
	const int total_passengers = a;
	int passengers_left = total_passengers;
	for (i = 0; i < total_passengers; i++) {
		pthread_t tid;
		int ret = pthread_create(&tid, NULL, passenger_thread, &station);
		if (ret != 0) {
			// If this fails, perhaps we exceeded some system limit.
			// Try reducing 'total_passengers'.
			perror("pthread_create");
			exit(1);
		}
	}


	// Tons of random tests.
	int total_passengers_boarded = 0;
	const int max_free_seats_per_train = 50;
	int pass = 0;
	while (passengers_left > 0) {
		int free_seats = random() % max_free_seats_per_train;

		printf("Train entering station with %d free seats\n", free_seats);
		load_train_returned = 0;
		struct load_train_args args = { &station, free_seats };
		pthread_t t;
		int ret = pthread_create(&t, NULL, load_train_thread, &args);
		if (ret != 0) {
			perror("pthread_create");
			exit(1);
		}

		int threads_to_collect = 0;
		if(passengers_left<free_seats)
		    threads_to_collect = passengers_left;
		else
		    threads_to_collect=free_seats;
		int threads_collected = 0;
		while (threads_collected < threads_to_collect) {
			if (load_train_returned) {
				fprintf(stderr, "Error: station_load_train returned early!\n");
				exit(1);
			}
			if (threads_completed > 0) {
				threads_collected++;
				station_on_board(&station);
				__sync_sub_and_fetch(&threads_completed, 1);
			}
		}

		// Wait a little bit longer. Give station_load_train() a chance to return
		// and ensure that no additional passengers board the train.
		for (i = 0; i < 1000; i++) {
			if (i > 50 && load_train_returned)
				break;
			usleep(1000);
		}

		if (!load_train_returned) {
			fprintf(stderr, "Error: station_load_train failed to return\n");
			exit(1);
		}

		while (threads_completed > 0) {
			threads_collected++;
			__sync_sub_and_fetch(&threads_completed, 1);
		}

		passengers_left -= threads_collected;
		total_passengers_boarded += threads_collected;
		printf("Train departed station with %d new passenger(s) (expected %d)\n",threads_to_collect, threads_collected);

		if (threads_to_collect != threads_collected) {
			fprintf(stderr, "Error: Too many passengers on this train!\n");
			exit(1);
		}

		pass++;
		if(pass == x)
		    break;
	}

	if (total_passengers_boarded == total_passengers) {
		printf("Station cleared\n");
		return 0;
	} else {

		printf("Waiting passengers in station %d!\n",station.wait_passengers);
		return 0;
	}
}
