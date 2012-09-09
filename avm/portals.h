#ifndef AVM_PORTALS_H
#define AVM_PORTALS_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

  extern portal avm_new_portal (portal alters);
  extern void avm_seal (portal fate);
  extern void avm_initialize_portals ();
  extern void avm_count_portals ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_PORTALS_H */
