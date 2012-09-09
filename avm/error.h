#ifndef AVM_ERROR_H
#define AVM_ERROR_H 1

#include <avm/common.h>

#ifdef __cplusplus
extern "C"
{
#endif

  extern char *avm_program_name ();
  extern void avm_set_program_name (char *argv0);
  extern void avm_warning (char *message);
  extern void avm_error (char *message);
  extern void avm_fatal_io_error (char *message, char *filename, int reason);
  extern void avm_non_fatal_io_error (char *message, char *filename, int reason);
  extern void avm_internal_error (int code);
  extern void avm_reclamation_failure (char *entity, counter count);


#ifdef __cplusplus
}
#endif


#endif				/* !AVM_ERROR_H */
