#ifndef AVM_LIBHARMINV_H
#define AVM_LIBHARMINV_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_harminv_call (list function_name, list argument, int *fault);
  extern list avm_have_harminv_call (list function_name, int *fault);
  extern void avm_initialize_harminv ();
  extern void avm_count_harminv ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_LIBHARMINV_H */
