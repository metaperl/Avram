#ifndef AVM_VMAN_H
#define AVM_VMAN_H 1

#include <avm/common.h>

#ifdef __cplusplus
extern "C"
{
#endif

  extern void avm_set_version (char *number);
  extern int avm_prior_to_version (char *number);
  extern char *avm_version ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_VMAN_H */
