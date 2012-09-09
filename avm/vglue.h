#ifndef AVM_VGLUE_H
#define AVM_VGLUE_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_compose (list operator, list preprocessor, int *fault);
  extern list avm_map (list operator, int *fault);
  extern list avm_reduce (list operator, int *fault);
  extern list avm_sort (list operator, int *fault);
  extern list avm_merge (list predicate, int *fault);
  extern void avm_initialize_vglue ();
  extern void avm_count_vglue ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_VGLUE_H */



