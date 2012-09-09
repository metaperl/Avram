#ifndef AVM_EXMODES_H
#define AVM_EXMODES_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

  extern void avm_interact (list interactor, int step_mode, int ask_to_overwrite_mode, int quiet_mode);
  extern list avm_recoverable_interact (list interactor, int *fault);
  extern void avm_byte_transduce (list operator);
  extern void avm_line_map (list operator);
  extern void avm_disable_interaction ();
  extern void avm_trace_interaction ();
  extern void avm_initialize_exmodes ();
  extern void avm_count_exmodes ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_EXMODES_H */
