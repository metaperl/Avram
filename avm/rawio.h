#ifndef AVM_RAWIO_H
#define AVM_RAWIO_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

  extern void avm_send_list (FILE * repository, list operand, char *filename);
  extern void avm_recoverable_send_list (int sockfd, list operand, char **crc, int *timeout, int *closed, int *fault);
  extern list avm_recoverable_received_list (int sockfd, char **crc, int *timeout, int *closed, int *fault);
  extern list avm_received_list (FILE *object, char *filename);
  extern void avm_initialize_rawio ();
  extern void avm_count_rawio ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_RAWIO_H */
