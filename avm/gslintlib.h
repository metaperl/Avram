#ifndef AVM_GSLINTLIB_H
#define AVM_GSLINTLIB_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_gslint_call (list function_name, list argument, int *fault);
  extern list avm_have_gslint_call (list function_name, int *fault);
  extern void avm_initialize_gslint ();
  extern void avm_count_gslint ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_GSLINTLIB_H */
