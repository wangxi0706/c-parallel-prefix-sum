// PPLS Exercise 2 Starter File
//
// See the exercise sheet for details
//
// Note that NITEMS, NTHREADS and SHOWDATA should
// be defined at compile time with -D options to gcc.
// They are the array length to use, number of threads to use
// and whether or not to printout array contents (which is
// useful for debugging, but not a good idea for large arrays).

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

// Print a helpful message followed by the contents of an array
// Controlled by the value of SHOWDATA, which should be defined
// at compile time. Useful for debugging.
void showdata (char *message,  int *data,  int n) {
  int i; 

if (SHOWDATA) {
    printf (message);
    for (i=0; i<n; i++ ){
     printf (" %d", data[i]);
    }
    printf("\n");
  }
}

// Check that the contents of two integer arrays of the same length are equal
// and return a C-style boolean
int checkresult (int* correctresult,  int *data,  int n) {
  int i; 

  for (i=0; i<n; i++ ){
    if (data[i] != correctresult[i]) return 0;
  }
  return 1;
}

// Compute the prefix sum of an array **in place** sequentially
void sequentialprefixsum (int *data, int n) {
  int i;
  for (i=1; i<n; i++ ) {
    data[i] = data[i] + data[i-1];
  }
}


// ------------------------------------------------------------------------------
// START OF NEW CODE
// ------------------------------------------------------------------------------

// These three global variables are used in the implementation of a simple counter barrier
pthread_cond_t end_of_phase;   // condition variable - waiting for all threads to reach barrier
pthread_mutex_t phase_barrier; // mutex lock - to ensure atomicity of barrier function
int number_complete_phase;     // count variable - the number of threads currently at the barrier

// To allow the passing of multiple arguments to a new thread, they are packed into a single struct
typedef struct worker_argument{
    int id;         // The number of the created thread - used for debug
    int *data;      // The allocated data for that thread (points to first address)
    int n;          // The length of its allocated data
    int n_items;    // The total number of items in the data (Phase 2)
    int *prev_max;  // The address of the final element of the preceding chunk. Not for thread 0.
    int chunk_size; // The length of each regular chunk. Needed for Phase 2.
} worker_argument;

// The barrier check needs to be atomic so a mutex is acquired
void barrier(){
    // Acquire the lock and increment the global number_complete variable
    pthread_mutex_lock(&phase_barrier);  
    number_complete_phase++;      
    // If I am the last thread then reset the global variable and release the other waiting threads
    if (number_complete_phase == NTHREADS) {
        number_complete_phase = 0;
        pthread_cond_broadcast(&end_of_phase);
    }
    // Otherwise wait until the final thread gets here
    // Automatically release the lock
    else {
        pthread_cond_wait(&end_of_phase, &phase_barrier);
    }
    // Explicitly release the lock
    pthread_mutex_unlock(&phase_barrier);
}

// Thread worker function
// Implements the three-phase parallel prefix sum algorithm
void *work_function (void *work_packet) {
    int id;         // My thread id - used to identify thread 0
    int *data;      // A pointer to the first address of my chunk of data
    int n;          // The length of my assigned chunk of data
    int n_items;    // The total number of items in the data [Used in Phase 2]
    int chunk_size; // The size of each chunk (except the final one) [Used in Phase 2]
    int *prev_max;  // A pointer to the final element of the previous chunk if it exists [Used in Phase 3]

    int i;          // Used for iterating through all threads

    // Retrieve the arguments from the work_packet
    id = ((worker_argument *)work_packet)->id;
    data = ((worker_argument *)work_packet)->data;
    n = ((worker_argument *)work_packet)->n;
    prev_max = ((worker_argument *)work_packet)->prev_max;
    chunk_size = ((worker_argument *)work_packet)->chunk_size;
    n_items = ((worker_argument *)work_packet)->n_items;

    // -- THE ALGORITHM -- //
    // PHASE 1
    // For all threads - run the sequential prefix sum on my chunk
    sequentialprefixsum(data, n);

    // Synchronisation barrier
    // Before thread 0 can start Phase 2, all threads must have completed Phase 1
    // for their chunk
    barrier();
    
    // All threads have reached this point at the same time

    // PHASE 2
    // For only thread 0 - run a prefix sum with the final elements of each chunk
    if (id == 0) {
        // For each final item pair except the final item, perform prefix sum
        int my_last_element;
        int the_preceding_last_element;

        for (i=1; i<NTHREADS-1; i++){
            my_last_element = ((i+1)*chunk_size) - 1;        // the index of the final element of chunk i 
            the_preceding_last_element = (i*chunk_size) - 1; // the index of the final element of chunk i-1

            data[my_last_element] = data[my_last_element] + data[the_preceding_last_element];
        }

        // Now do the final pair
        int the_final_element;

        the_final_element = n_items-1;                           // index of the final element of the last chunk
        the_preceding_last_element = (NTHREADS-1)*chunk_size -1; // index of the final element of the second last chunk

        data[the_final_element] = data[the_final_element] + data[the_preceding_last_element];
    }
    // else { do nothing } all other threads are inactive in Phase 2

    // Synchronisation barrier
    // Before the threads (except 0) can start Phase 3, all threads must have ended Phase 2
    // for their chunk
    barrier();
    
    // All threads have reached this point at the same time

    // PHASE 3 
    // For all threads except thread zero - Add the previous chunk's final element to all my elements except final
    if (id != 0){
        // Add the final value from the preceding chunk into every value in its own chunk, except the last position
        for (i=0; i<n-1; i++){
            data[i] = data[i] + *prev_max;
        }
    }
    // else { do nothing } thread 0 is inactive in Phase 3

    return NULL;
}

void parallelprefixsum (int *data, int n) {
    int i;                          // Iteration variable in for loops                
    int chunk_size;                 // The number of elements in each chunk
    int leftover_items;             // The leftover elements after dividing into chunks
    int final_chunk_size;           // chunk_size + leftover_items
    pthread_t *workers;             // Pointer to array of the worker pthreads
    worker_argument *work_packets;  // A collection of the worker data for each thread

    // Initialize the barrier mutex and condition variable 
    pthread_mutex_init(&phase_barrier, NULL);
    pthread_cond_init(&end_of_phase, NULL);
    number_complete_phase = 0;

    // Allocate memory for the worker threads and their data
    workers     = (pthread_t *) malloc (NTHREADS * sizeof(pthread_t)); 
    work_packets  = (worker_argument *)  malloc (NTHREADS * sizeof(worker_argument));

    // Each chunk should be of size at least floor{NITEMS/NTHREADS}
    // Simple method puts the excess items in the last chunk
    chunk_size = n / NTHREADS;
    leftover_items = n - (NTHREADS * chunk_size);
    final_chunk_size = chunk_size + leftover_items;

    // Initialise the work packets for each thread
    for (i=0; i<NTHREADS; i++) {
        work_packets[i].id = i;
        work_packets[i].data = &data[i*chunk_size];
        work_packets[i].n_items = n; // Needed by thread 0 in Phase 2
        work_packets[i].chunk_size = chunk_size; // Needed by thread 0 in Phase 2
        if(i<NTHREADS-1){
            work_packets[i].n = chunk_size;
            work_packets[i+1].prev_max = &data[(i+1)*chunk_size - 1]; // Will be used in Phase 3
        }
        else {
            work_packets[i].n = final_chunk_size;
        }
    }

    // Create and launch the threads and then wait for them all to complete
    for (i=0; i<NTHREADS; i++) {
        pthread_create (&workers[i], NULL, work_function, (void *) &work_packets[i]);
    }
    for (i=0; i<NTHREADS; i++) {
        pthread_join(workers[i], NULL);
    }

    free(workers);
    free(work_packets);
}

// ------------------------------------------------------------------------------
// END OF NEW CODE
// ------------------------------------------------------------------------------

int main (int argc, char* argv[]) {

  int *arr1, *arr2, i;

  // Check that the compile time constants are sensible for this exercise
  if ((NITEMS>10000000) || (NTHREADS>32)) {
    printf ("So much data or so many threads may not be a good idea! .... exiting\n");
    exit(EXIT_FAILURE);
  }

  // Create two copies of some random data
  arr1 = (int *) malloc(NITEMS*sizeof(int));
  arr2 = (int *) malloc(NITEMS*sizeof(int));
  srand((int)time(NULL));
  for (i=0; i<NITEMS; i++) {
     arr1[i] = arr2[i] = rand()%5;
  }
  showdata ("initial data          : ", arr1, NITEMS);

  // Calculate prefix sum sequentially, to check against later
  sequentialprefixsum (arr1, NITEMS);
  showdata ("sequential prefix sum : ", arr1, NITEMS);

  // Calculate prefix sum in parallel on the other copy of the original data
  parallelprefixsum (arr2, NITEMS);
  showdata ("parallel prefix sum   : ", arr2, NITEMS);

  // Check that the sequential and parallel results match
  if (checkresult(arr1, arr2, NITEMS))  {
    printf("Well done, the sequential and parallel prefix sum arrays match.\n");
  } else {
    printf("Error: The sequential and parallel prefix sum arrays don't match.\n");
  }

  return 0;
}


/*
DISCUSSION:
* How I created threads
I created the worker threads from within the main thread using the pthread_create() method.
Each is passed a worker packet which contains a pointer to the start of their allocated
chunk of data, the length of the their chunk and their id number. For the special work of
Thread 0 in Phase 2, the total length NITEMS and the regular chunk size 'chunk_size' are
also included. To facilitate Phase 3, a pointer to the final element of the previous chunk
is also included (not for Thread 0).

* How, where and why you synchronized them
The threads are synchronised after PHASE 1 using the custom barrier() function. This function
implements a basic counter barrier. It acquires the global mutex lock 'phase_barrier' for the
duration of the barrier() function. The global int 'number_complete_phase' is then incremented
which represents the number of threads that have reached the barrier. If this number is not
equal to the total number of threads then it implies that some threads are still working so
the the thread waits for the condition variable 'end_of_phase' with 'pthread_cond_wait'. This 
automatically frees the lock. When the final thread reaches the barrier(), it resets 
'number_complete_phase' and signals to all other waiting threads with 'pthread_cond_broadcast'.

The barrier() function is then used in the thread work function and guarantees that all threads
synchronize to that point before progressing.

This barrier() is used after Phase 1 so that each chunk has been completed so that Thread 0
can begin Phase 2 - which requires updated values in the final element of each chunk.

It is used again after Phase 2 to ensure that the other threads do not begin their Phase 3 work
before Thread 0 has finished Phase 2, as per the algorithm specification.

Finally, the main thread does not exit until all threads are complete. This is done by 
calling 'pthread_join' for all threads which blocks until that thread has completed.
Otherwise, the program could exit while work was still underway.

* Any other interesting features
As the number of items may not divide evenly between the number of threads, a basic solution 
is implemented. Simply assign floor{NITEMS/NTHREADS} items to all threads except the final
one which also gets the remaining items.
*/