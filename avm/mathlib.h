#ifndef AVM_LIBMATH_H
#define AVM_LIBMATH_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_math_call (list function_name, list argument, int *fault);
  extern list avm_have_math_call (list function_name, int *fault);
  extern void avm_initialize_math ();
  extern void avm_count_math ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_LIBMATH_H */
