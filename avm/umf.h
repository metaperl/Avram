#ifndef AVM_UMF_H
#define AVM_UMF_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_umf_call (list function_name, list argument, int *fault);
  extern list avm_have_umf_call (list function_name, int *fault);
  extern void avm_initialize_umf ();
  extern void avm_count_umf ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_UMF_H */
