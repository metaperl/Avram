#ifndef AVM_LIBCOMPLEX_H
#define AVM_LIBCOMPLEX_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_complex_call (list function_name, list argument, int *fault);
  extern list avm_have_complex_call (list function_name, int *fault);
  extern void avm_initialize_complex ();
  extern void avm_count_complex ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_LIBCOMPLEX_H */
