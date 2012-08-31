#ifndef AVM_PROFILE_H
#define AVM_PROFILE_H 1

#include <avm/common.h>
#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* Anyone who can figure out how to change the unsigned long fields
     to counter types without causing profile information to be
     incorrectly recorded or reported should do it. */

  struct score_node
  {
    list team;
    unsigned long calls;
    unsigned long reductions;
    struct score_node *league;
  };

  typedef struct score_node *score;

  extern score avm_entries (list team, list * message, int *fault);
  extern void avm_tally (char *filename);
  extern void avm_initialize_profile ();
  extern void avm_count_profile ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_PROFILE_H */
