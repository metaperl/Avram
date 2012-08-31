#ifndef AVM_GSLEVU_H
#define AVM_GSLEVU_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_gslevu_call (list function_name, list argument, int *fault);
  extern list avm_have_gslevu_call (list function_name, int *fault);
  extern void avm_initialize_gslevu ();
  extern void avm_count_gslevu ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_GSLEVU_H */
