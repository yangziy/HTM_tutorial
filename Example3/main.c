#define _GNU_SOURCE
#include <pthread.h> // needed to launch threads
#include <sched.h>   // bind thread to core
#include <errno.h>   // error detection
#include <string.h>  // strerror

#include <stdlib.h>
#include <stdio.h>

#include "HTM_impl.h"
#include "tot_utils.h"

#define CACHE_LINE_SIZE   64

// -------------------------------------------------------------------
typedef struct thread_params_ {
  int thread_id;
} thread_params_t;

/* + -------------------------------- +
   | Global variables                 |
   + -------------------------------- + */
static const int NB_THREADS  = 2;
static const int MAX_RETRIES = 10; // retries the HTM transaction 10 times
static __thread int aborted;
static __thread int nb_retries;
static __attribute__((aligned (HTM_CLSIZE))) int target_mem;
static pthread_barrier_t barrier;
pthread_mutex_t SGL; // globally accessed
__thread HTM_CL_ALIGN HTM_local_vars_s HTM_local_vars;
#if TM_STATISTICS == 1
__thread int htm_errors[HTM_NB_ERRORS]; // globally accessed
#endif /* TM_STATISTICS */

/* + -------------------------------- +
   | Thread callback (declaration)    |
   + -------------------------------- + */
static void* thread_fn(void*);

// -------------------------------------------------------------------

int main (int argc, char **argv)
{
    pthread_t thrs[NB_THREADS];
    thread_params_t params[] = {
      {.thread_id = 1},
      {.thread_id = 2},
    };

    // TODO: test the return value
    pthread_barrier_init(&barrier, NULL, 2);
    pthread_mutex_init(&SGL, NULL);

    // launches 2 threads
    thread_create_or_die(&thrs[0], thread_fn, &params[0]);
    thread_create_or_die(&thrs[1], thread_fn, &params[1]);

    // joins
    thread_join_or_die(thrs[0]);
    thread_join_or_die(thrs[1]);

    pthread_barrier_destroy(&barrier);
    pthread_mutex_destroy(&SGL);
    return EXIT_SUCCESS;
}

/* + -------------------------------- +
   | Thread callback (implementation) |
   + -------------------------------- + */
static void* thread_fn(void *args)
{
  thread_params_t *params = (thread_params_t*)args;
  int TSX_status, i;

  printf("Enter thread %i\n", params->thread_id);
  bindThread(params->thread_id-1); // binds to target CPU core

  // waits both threads
  pthread_barrier_wait(&barrier);

  HTM_BEGIN();

    for (i = 0; i < 10000; ++i); // spins a bit to cause conflicts
    target_mem++;
    for (i = 0; i < 10000; ++i); // spins a bit to cause conflicts

  HTM_COMMIT();

#if TM_STATISTICS == 1 // prints only if statistics is ON
  printf("Thread %i aborted %i times!\n", params->thread_id, htm_errors[HTM_ABORT]);
#endif /* TM_STATISTICS */
}
