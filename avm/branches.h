#ifndef AVM_BRANCHES_H
#define AVM_BRANCHES_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif


  typedef list *branch;

  struct branch_node
  {
    branch above;
    struct branch_node *following;
  };

  typedef struct branch_node *branch_queue;

  extern void avm_anticipate (branch_queue * front, branch_queue * back, branch operand);
  extern void avm_recoverable_anticipate (branch_queue * front, branch_queue * back, branch operand, int *fault);
  extern void avm_enqueue_branch (branch_queue * front, branch_queue * back, int received_bit);
  extern void avm_recoverable_enqueue_branch (branch_queue * front, branch_queue * back, int received_bit, int *fault);
  extern void avm_dispose_branch_queue  (branch_queue front);
  extern void avm_dispose_branch  (branch_queue old);
  extern void avm_initialize_branches ();
  extern void avm_count_branches ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_BRANCHES_H */
