#ifndef AVM_DECONS_H
#define AVM_DECONS_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_deconstruction (list pointer, list operand, int *fault);
  extern void avm_initialize_decons ();
  extern void avm_count_decons ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_DECONS_H */
