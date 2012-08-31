#ifndef AVM_JOBS_H
#define AVM_JOBS_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct job_struct *job;

/* A job represents an ensemble of computations and their dependence
   relation as a tree.  The value of a terminal job tree is its root,
   the value of a non-terminal tree with an empty root is the list of
   values of its prerequisites, the value of a non-terminal tree with
   a single celled root is the flattened list of its prerequisites,
   and the value of a non-terminal tree with an otherwise non-empty
   root is the root applied to the list of values of its
   prerequisites. */

struct job_struct
{
  list root;             /* this is to be freed when the job is freed */
  flag running;          /* set when there is a remote server working on it */
  int dependence;        /* the number of unevaluated prerequisites  */
  int deficit;           /* used temporarily by the rebalancing algorithm */
  job dependent;         /* the thing that can't get evaluated until this is */
  job prerequisites;     /* the things that have to be evaluted before this */
  job corequisites;      /* things that can be evaluated concurrently on which the dependent also depends */
};

  extern void avm_free_job (job *front);
  extern void avm_new_job (job *front, job *back, list root, job dependent, job prerequisites, int dependence, int *fault);
  extern void avm_queue_job (job *front, job *back, job *top, job dependent);
  extern list avm_evaluation (job top, flag balanceable, int *fault);
  extern void avm_initialize_jobs ();
  extern void avm_count_jobs ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_JOBS_H */
