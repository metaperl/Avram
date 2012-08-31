#ifndef AVM_GLPKLIB_H
#define AVM_GLPKLIB_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_glpk_call (list function_name, list argument, int *fault);
  extern list avm_have_glpk_call (list function_name, int *fault);
  extern void avm_initialize_glpk ();
  extern void avm_count_glpk ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_GLPKLIB_H */
