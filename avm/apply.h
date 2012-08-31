#ifndef AVM_APPLY_H
#define AVM_APPLY_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

  extern flag _avm_reset;

  extern list avm_apply (list operator, list operand);
  extern list avm_recoverable_apply (list operator, list operand, int *fault);
  extern void avm_initialize_apply ();
  extern void avm_count_apply ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_APPLY_H */
