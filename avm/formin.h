#ifndef AVM_FORMIN_H
#define AVM_FORMIN_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_preamble_and_contents (FILE * source, char *filename);
  extern list avm_load (FILE * source, char *filename, int raw);
  extern void avm_initialize_formin ();
  extern void avm_count_formin ();


#ifdef __cplusplus
}
#endif


#endif				/* !AVM_FORMIN_H */
