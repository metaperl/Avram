#ifndef AVM_LIBFUNS_H
#define AVM_LIBFUNS_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_library_call (list library_name, list function_name, list argument, int *fault);
  extern list avm_have_library_call (list library_name, list function_name, int *fault);
  extern void avm_initialize_libfuns ();
  extern void avm_count_libfuns ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_LIBFUNS_H */
