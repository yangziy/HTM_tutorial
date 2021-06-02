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
static const int NB_THREADS = 2;
static __thread int aborted;
static __attribute__((aligned (CACHE_LINE_SIZE))) int target_mem;
static pthread_barrier_t barrier;

/* + -------------------------------- +
   | Thread callback (declaration)    |
   + -------------------------------- + */
static void* thread_fn(void*);
static void* thread_fn2(void*);


// -------------------------------------------------------------------

int main (int argc, char **argv)
{
    printf("BEGIN - target_mem: %d\n", target_mem);
    pthread_t thrs[NB_THREADS];
    thread_params_t params[] = {
      {.thread_id = 1},
      {.thread_id = 2},
      {.thread_id = 3},
    };

    pthread_barrier_init(&barrier, NULL, 3);

    // launches 2 threads
    thread_create_or_die(&thrs[0], thread_fn, &params[0]);
    thread_create_or_die(&thrs[1], thread_fn, &params[1]);
    thread_create_or_die(&thrs[2], thread_fn2, &params[2]);

    // joins
    thread_join_or_die(thrs[0]);
    thread_join_or_die(thrs[1]);
    thread_join_or_die(thrs[2]);

    pthread_barrier_destroy(&barrier);
    printf("END - target_mem: %d\n", target_mem);
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
  while (1) {
     // return point to aborted transactions
    if ((TSX_status = _xbegin()) != _XBEGIN_STARTED) {
      // hardware transaction aborted
      switch (TSX_status) {
        case _XABORT_CAPACITY:
          printf("Thread %i aborted due to a buffer overflow!\n", params->thread_id);
          break;
        case _XABORT_CONFLICT|_XABORT_RETRY:
          printf("Thread %i aborted due to a conflict!\n", params->thread_id);
          break;
        default:
          printf("Thread %i aborted (status=%i)!\n", params->thread_id, TSX_status);
          break;
      }
      aborted++; // aborted is thread local, no sync needed!
      continue;  // try again
    }

    for (i = 0; i < 10000; ++i); // spins a bit to cause conflicts
    target_mem++;
    for (i = 0; i < 10000; ++i); // spins a bit to cause conflicts
    _xend();
    break; // exit
  }

  printf("Thread %i aborted %i times!\n", params->thread_id, aborted);
  return NULL;
}

static void* thread_fn2(void *args)
{
  thread_params_t *params = (thread_params_t*)args;
  int TSX_status, i;

  printf("Enter thread %i\n", params->thread_id);
  bindThread(params->thread_id-1); // binds to target CPU core

  // waits both threads
  pthread_barrier_wait(&barrier);
  while (1) {
     // return point to aborted transactions
    if ((TSX_status = _xbegin()) != _XBEGIN_STARTED) {
      aborted++; // aborted is thread local, no sync needed!
      // hardware transaction aborted
      switch (TSX_status) {
        case _XABORT_CAPACITY:
          printf("Thread %i aborted due to a buffer overflow!\n", params->thread_id);
          break;
        case _XABORT_CONFLICT|_XABORT_RETRY:
          printf("Thread %i aborted due to a conflict!\n", params->thread_id);
          break;
        default:
          if (_XABORT_CODE(TSX_status) == 0xff) {
            printf("Thread %i aborted due to _xabort(0xff)!\n", params->thread_id);
            goto outside;
          }
          printf("Thread %i aborted (status=%u)!\n", params->thread_id, TSX_status);
          break;
      }
      continue;  // try again
    }

    for (i = 0; i < 10000; ++i); // spins a bit to cause conflicts
    target_mem++;
    for (i = 0; i < 10000; ++i); // spins a bit to cause conflicts
    _xabort(0xff);
    break; // exit
  }

outside:
  printf("Thread %i aborted %i times!\n", params->thread_id, aborted);

  return NULL;
}
