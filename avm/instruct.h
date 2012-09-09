#ifndef AVM_INSTRUCT_H
#define AVM_INSTRUCT_H 1

#include <avm/lists.h>
#include <avm/common.h>
#include <avm/profile.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct instruction_node *instruction;

  struct instruction_node
  {
    port client;
    score sheet;
    flag remotely_executable;
    flag non_deterministic;
    counter granularity;
    struct avm_packet actor;
    struct avm_packet datum;
    instruction dependents;
  };

  extern int avm_scheduled (list actor_contents, 
			    counter datum_errors,
			    list datum_contents,
			    port client,
			    instruction *next,
			    score sheet,
			    flag remote,
			    flag balanceable,
			    counter granularity);

  extern void avm_retire (instruction *done);

  extern void avm_reschedule (instruction *next);

  extern void avm_initialize_instruct ();

  extern void avm_count_instruct ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_INSTRUCT_H */
