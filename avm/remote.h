#ifndef AVM_REMOTE_H
#define AVM_REMOTE_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

  extern int avm_remotely_reduced (list operator, 
				   list vacuous_case, 
				   list operand, 
				   list *result, 
				   counter granularity,
				   flag balanceable,
				   int *fault);

  extern int avm_remotely_mapped (list operator, list operand, list *result, counter granularity, int *fault);

  extern int avm_remotely_sorted (list operator, list operand, list *result, counter granularity, int *fault);

  extern int avm_remotely_constructed (list left_side, list right_side, list operand, list *result, int *fault);

  extern void avm_register_server (char *host, int port_number, int *fault);

  extern void avm_initialize_remote ();

  extern void avm_count_remote ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_REMOTE_H */







