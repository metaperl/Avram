#ifndef AVM_MPFR_H
#define AVM_MPFR_H 1

#ifdef __cplusplus
extern "C"
{
#endif

#if HAVE_MPFR
#include <mpfr.h>
#if HAVE_GMP
#include <gmp.h>
#include <mpf2mpfr.h>
#endif
#endif

#if HAVE_MPFR
  typedef mpfr_t *avm_mpfr_ptr;
#else
  typedef void *avm_mpfr_ptr;
#endif

  extern list avm_mpfr_call (list function_name, list argument, int *fault);
  extern list avm_have_mpfr_call (list function_name, int *fault);
  extern avm_mpfr_ptr avm_mpfr_of_list (list operand, list *message, int *fault);
  extern list avm_list_of_mpfr (avm_mpfr_ptr x,int *fault);
  extern void avm_initialize_mpfr ();
  extern void avm_count_mpfr ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_MPFR_H */
