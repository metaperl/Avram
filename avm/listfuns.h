#ifndef AVM_LISTFUNS_H
#define AVM_LISTFUNS_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_reversal (list operand, int *fault);
  extern list avm_distribution (list operand, int *fault);
  extern list avm_concatenation (list operand, int *fault);
  extern list avm_transposition (list operand, int *fault);
  extern list avm_membership (list operand, int *fault);
  extern list avm_flattened (list operand, int *fault);
  extern list avm_binary_membership (list operand, list members, int *fault);
  extern list avm_measurement (list operand, int *fault);
  extern list avm_position (list key, list table, int *fault);
  extern void *avm_value_of_list (list operand, list *message, int *fault);
  extern list avm_list_of_value (void *contents, size_t size, int *fault);
  extern void avm_initialize_listfuns ();
  extern void avm_count_listfuns ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_LISTFUNS_H */
