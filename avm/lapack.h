#ifndef AVM_LAPACK_H
#define AVM_LAPACK_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_lapack_call (list function_name, list argument, int *fault);
  extern list avm_have_lapack_call (list function_name, int *fault);
  extern void avm_initialize_lapack ();
  extern void avm_count_lapack ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_LAPACK_H */
