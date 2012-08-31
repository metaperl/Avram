#ifndef AVM_LIBBES_H
#define AVM_LIBBES_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_bes_call (list function_name, list argument, int *fault);
  extern list avm_have_bes_call (list function_name, int *fault);
  extern void avm_initialize_bes ();
  extern void avm_count_bes ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_LIBBES_H */
