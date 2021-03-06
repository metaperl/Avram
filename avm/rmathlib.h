#ifndef AVM_RMATHLIB_H
#define AVM_RMATHLIB_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_rmath_call (list function_name, list argument, int *fault);
  extern list avm_have_rmath_call (list function_name, int *fault);
  extern void avm_initialize_rmath ();
  extern void avm_count_rmath ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_RMATHLIB_H */
