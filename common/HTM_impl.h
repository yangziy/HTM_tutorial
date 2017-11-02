#ifndef HTM_IMPL_H_GUARD
#define HTM_IMPL_H_GUARD

#include "HTM_arch.h"

#ifndef HTM_INIT_BUDGET
#define HTM_INIT_BUDGET 10
#endif /* HTM_INIT_BUDGET */

// packs the needed meta-data in 1 cache line
typedef struct HTM_local_vars_ {
  HTM_STATUS_TYPE status;
  int budget;
} __attribute__((packed)) HTM_local_vars_s;

// application must implement it
extern __thread HTM_CL_ALIGN HTM_local_vars_s HTM_local_vars;
extern pthread_mutex_t SGL; // single global lock (must be implemented in the application)
#if TM_STATISTICS == 1
extern __thread int htm_errors[HTM_NB_ERRORS]; // must be implemented in the application
#endif /* TM_STATISTICS */

// easy to access state
#define HTM_status HTM_local_vars.status
#define HTM_budget HTM_local_vars.budget

#define HTM_START_TRANSACTION()   (HTM_begin(HTM_status) != HTM_CODE_SUCCESS)
#define HTM_BEFORE_TRANSACTION()  /* empty */
#define HTM_AFTER_TRANSACTION()   /* empty */

#define HTM_AFTER_BEGIN()   /* empty */
#define HTM_BEFORE_COMMIT() /* empty */
#define HTM_COMMIT_TRANSACTION() \
	HTM_commit(); /* Commits and updates some statistics after */ \
  HTM_INC(HTM_status)

#define HTM_AFTER_ABORT() ({ \
  HTM_budget--; \
})

#define HTM_BEFORE_BEGIN()  /* empty */
#define HTM_BEFORE_COMMIT() /* empty */
#define HTM_AFTER_COMMIT()  /* empty */

#define HTM_ENTER_COND()      HTM_budget > 0
#define HTM_IN_TRANSACTION()  HTM_test()

#if TM_STATISTICS == 1
  #define HTM_INC(status) ({ \
    HTM_errors_e code = HTM_ERROR_TO_INDEX(status); \
    htm_errors[code]++; \
  })
  #define HTM_INC_ABORT()    htm_errors[HTM_ABORT]++
  #define HTM_INC_SUCCESS()  htm_errors[HTM_SUCCESS]++
  #define HTM_INC_FALLBACK() htm_errors[HTM_FALLBACK]++
#else
  #define HTM_INC(status)          /* empty */
  #define HTM_INC_ABORT(status)    /* empty */
  #define HTM_INC_SUCCESS(status)  /* empty */
  #define HTM_INC_FALLBACK(status) /* empty */
#endif

#define HTM_BEGIN() \
{ \
  HTM_budget = HTM_INIT_BUDGET; \
  HTM_BEFORE_TRANSACTION(); \
  while (1) { \
    if (HTM_ENTER_COND()) { \
      HTM_BEFORE_BEGIN(); \
      if (HTM_START_TRANSACTION()) { \
        HTM_INC_ABORT(); \
        HTM_INC(HTM_status); \
        HTM_AFTER_ABORT(); \
        continue; \
      } \
    } else { \
      pthread_mutex_unlock(&SGL); \
    } \
    HTM_AFTER_BEGIN(); \
    break;\
  } \
}
//
#define HTM_COMMIT() \
{ \
  HTM_BEFORE_COMMIT(); \
  if (HTM_IN_TRANSACTION()) { \
    HTM_BEFORE_COMMIT(); \
    HTM_COMMIT_TRANSACTION(); \
    HTM_AFTER_COMMIT(); \
    HTM_INC_SUCCESS(); \
  } else { \
    pthread_mutex_unlock(&SGL); \
    HTM_INC_FALLBACK(); \
  } \
  HTM_AFTER_TRANSACTION(); \
}

#endif /* HTM_IMPL_H_GUARD */
