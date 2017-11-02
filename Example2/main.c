#define _GNU_SOURCE
#include <pthread.h> // needed to launch threads
#include <sched.h>   // bind thread to core
#include <errno.h>   // error detection
#include <string.h>  // strerror

#include <stdlib.h>
#include <stdio.h>

// TSX headers
#include <immintrin.h>
#include <rtmintrin.h>

#define CACHE_LINE_SIZE   64

// binds the current thread to a core (POSIX interface)
#define bindThread(threadId) ({ \
  cpu_set_t my_set; \
  CPU_ZERO(&my_set); \
  int offset = threadId; /* attention mapping threadIds to cores ! */ \
  CPU_SET(offset, &my_set); \
  sched_setaffinity(0, sizeof(cpu_set_t), &my_set); \
})

#define thread_create_or_die(thr, callback, args) \
  if (pthread_create(thr, NULL, callback, (void*)(args))) { \
    fprintf(stderr, "Error creating thread in " __FILE__ ":%i\n  -> %s\n", \
      __LINE__, strerror(errno)); \
  } \
//

#define thread_join_or_die(thr) \
  if (pthread_join(thr, NULL)) { \
    fprintf(stderr, "Error joining thread in " __FILE__ ":%i\n  -> %s\n", \
      __LINE__, strerror(errno)); \
  } \
//

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
static __attribute__((aligned (CACHE_LINE_SIZE))) int target_mem;
static pthread_barrier_t barrier;
static pthread_mutex_t single_global_lock;

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
    pthread_mutex_init(&single_global_lock, NULL);

    // launches 2 threads
    thread_create_or_die(&thrs[0], thread_fn, &params[0]);
    thread_create_or_die(&thrs[1], thread_fn, &params[1]);

    // joins
    thread_join_or_die(thrs[0]);
    thread_join_or_die(thrs[1]);

    pthread_barrier_destroy(&barrier);
    pthread_mutex_destroy(&single_global_lock);
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

  nb_retries = MAX_RETRIES;
  while (1) {
    if ((TSX_status = _xbegin()) != _XBEGIN_STARTED) {
      // hardware transaction aborted
      nb_retries--;
      aborted++; // aborted is thread local, no sync needed!
      if (nb_retries < 0) {
        pthread_mutex_lock(&single_global_lock);
        // execute non-transactionally
      } else {
        continue; // try again
      }
    }

    for (i = 0; i < 10000; ++i); // spins a bit to cause conflicts
    target_mem++;
    for (i = 0; i < 10000; ++i); // spins a bit to cause conflicts

    if (_xtest())
      _xend(); // _xtext returns true if inside the HTM transaction
    else
      pthread_mutex_unlock(&single_global_lock);
    break; // exit
  }

  printf("Thread %i aborted %i times!\n", params->thread_id, aborted);
}
