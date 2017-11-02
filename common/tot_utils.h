#ifndef TOT_UTILS_H_
#define TOT_UTILS_H_

#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>

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

#endif /* TOT_UTILS_H_ */
