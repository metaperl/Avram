#ifndef AVM_MWRAP_H
#define AVM_MWRAP_H 1

#ifdef __cplusplus
extern "C"
{
#endif

#if HAVE_SETJMP
#include <setjmp.h>
#else
  typedef int jmp_buf;
  extern int setjmp (jmp_buf);
#endif

  extern jmp_buf *_avm_mwrap_client;

#if HAVE_SETJMP
#define avm_setjmp() ((_avm_mwrap_client = avm_new_jmp_buf()) ? setjmp(*_avm_mwrap_client) : -1)
#else
#define avm_setjmp() 0
#endif

  extern void avm_clearjmp ();
  extern void avm_setnonjmp ();
  extern jmp_buf *avm_new_jmp_buf ();  /* should be called only by way of avm_setjmp() */
  extern void avm_debug_memory ();
  extern void avm_dont_debug_memory ();
  extern inline void avm_manage_memory ();
  extern inline void avm_dont_manage_memory ();
  extern void avm_free_managed_memory ();
  extern void avm_turn_off_stdout ();
  extern void avm_turn_off_stderr ();
  extern void avm_turn_on_stdout ();
  extern void avm_turn_on_stderr ();
  extern void avm_initialize_mwrap ();
  extern void avm_count_mwrap ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_MWRAP_H */
