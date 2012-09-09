#ifndef AVM_LPSOLVE_H
#define AVM_LPSOLVE_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_lpsolve_call (list function_name, list argument, int *fault);
  extern list avm_have_lpsolve_call (list function_name, int *fault);
  extern void avm_initialize_lpsolve ();
  extern void avm_count_lpsolve ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_LPSOLVE_H */
