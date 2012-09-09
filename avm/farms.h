#ifndef AVM_FARMS_H
#define AVM_FARMS_H 1

#include <avm/lists.h>
#include <avm/jobs.h>
#include <avm/servlist.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* number of seconds between status checks on running jobs */
#define WAIT (time_t) 60

/* number of seconds between reconnection attempts to unresponsive servers */
#define RETRY (time_t) 300

typedef struct farm_struct *farm;

/* A farm represents a queue of pending jobs whose dependences are
   resolved. We start by planting the independent (leaf) nodes in a
   job tree and launching them asynchronously via the harvest
   function. As each one finishes, its dependent is checked for any
   remaining dependences and launched if possible, until the whole job
   is completed in an order consistent with its dependence
   constraints. */

struct farm_struct
{
  job site;            /* the quantum of computation assigned to this farm */
  server_list runner;  /* the remote server working on this job, if any */
  flag cache_hit;      /* the server is running with a remotely cached copy of the function site->root */
  list operand;        /* the argument to the site->root function; this is freed when the farm is freed */
  farm prev;           /* doubly linked list pointers */
  farm next;
};


  extern void avm_plant (farm *maggie, job top, int *fault);
  extern void avm_harvest (farm maggie, flag balanceable, int *fault);
  extern void avm_abnormally_terminate (farm *maggie);
  extern void avm_initialize_farms ();
  extern void avm_count_farms ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_FARMS_H */

