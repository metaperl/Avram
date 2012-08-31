#ifndef AVM_PORTS_H
#define AVM_PORTS_H 1

#include <avm/common.h>
#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

  extern port avm_newport (counter errors, port parent, int predicating);
  extern void avm_sever (port appendage);
  extern void avm_initialize_ports ();
  extern void avm_count_ports ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_PORTS_H */
