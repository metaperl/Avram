#ifndef AVM_FORMOUT_H
#define AVM_FORMOUT_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

  extern void avm_output (FILE * repository, char *filename, list preamble, list contents, int trace_mode);
  extern void avm_output_as_directed (list data, int ask_to_overwrite_mode, int verbose_mode);
  extern void avm_put_bytes (list bytes);
  extern void avm_initialize_formout ();
  extern void avm_count_formout ();


#ifdef __cplusplus
}
#endif


#endif				/* !AVM_FORMOUT_H */
