#ifndef AVM_MTWIST_H
#define AVM_MTWIST_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_mtwist_call (list function_name, list argument, int *fault);
  extern list avm_have_mtwist_call (list function_name, int *fault);
  extern void avm_initialize_mtwist ();
  extern void avm_count_mtwist ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_MTWIST_H */
