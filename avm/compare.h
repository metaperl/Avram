#ifndef AVM_COMPARE_H
#define AVM_COMPARE_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_comparison (list operand, int *fault);
  extern list avm_binary_comparison (list left_operand, list right_operand, int *fault);
  extern void avm_initialize_compare ();
  extern void avm_count_compare ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_COMPARE_H */
