#ifndef AVM_FFTW_H
#define AVM_FFTW_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_fftw_call (list function_name, list argument, int *fault);
  extern list avm_have_fftw_call (list function_name, int *fault);
  extern void avm_initialize_fftw ();
  extern void avm_count_fftw ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_FFTW_H */
