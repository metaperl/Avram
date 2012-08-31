#ifndef AVM_MINPACK_H
#define AVM_MINPACK_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_minpack_call (list function_name, list argument, int *fault);
  extern list avm_have_minpack_call (list function_name, int *fault);
  extern void avm_initialize_minpack ();
  extern void avm_count_minpack ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_MINPACK_H */
