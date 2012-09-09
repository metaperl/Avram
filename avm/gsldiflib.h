#ifndef AVM_GSLDIFLIB_H
#define AVM_GSLDIFLIB_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_gsldif_call (list function_name, list argument, int *fault);
  extern list avm_have_gsldif_call (list function_name, int *fault);
  extern void avm_initialize_gsldif ();
  extern void avm_count_gsldif ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_GSLDIFLIB_H */
