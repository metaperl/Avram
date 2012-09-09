#ifndef AVM_LISTS_H
#define AVM_LISTS_H 1

#include <avm/common.h>

#ifdef __cplusplus
extern "C"
{
#endif

  typedef unsigned short flag;

  struct node;
  struct avm_packet;
  struct port_pair;

  typedef struct node *list;
  typedef struct avm_packet *port;
  typedef struct port_pair *portal;

  struct port_pair
  {
    port left;
    port right;
    portal alters;
  };

  struct avm_packet
  {
    port parent;
    counter errors;
    portal descendents;
    list impetus, contents;
    flag predicating;
  };

  struct node
  {
    list head, tail;
    counter sharers;		/* reference count */
    counter known_weight;	/* number of nodes */
    port facilitator;		/* impetus points back here */
    int characterization;	/* character it represents */
    void *value;                /* used by library functions */
    flag discontiguous;         /* -> not to be combined by comparison */
    flag internal;		/* -> part of the interpreter */
    flag characteristic;	/* -> represents a character */
    list interpretation;	/* virtual code equivalent */
  };

  extern void avm_dispose (list front);
  extern list avm_recoverable_join (list left, list right);
  extern list avm_join (list left, list right);
  extern list avm_copied (list operand);
  extern void avm_enqueue (list * front, list *back, list operand);
  extern void avm_recoverable_enqueue (list *front, list *back, list operand, int *fault);
  extern counter avm_length (list operand);
  extern counter avm_recoverable_length (list operand);
  extern counter avm_area (list operand);
  extern counter avm_recoverable_area (list operand, int *fault);
  extern list avm_natural (counter number);
  extern list avm_recoverable_natural (counter number);
  extern counter avm_counter (list number);
  extern void avm_print_list (list operand);
  extern void avm_initialize_lists ();
  extern void avm_count_lists ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_LISTS_H */
