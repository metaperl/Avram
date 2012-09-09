#ifndef AVM_CHRCODES_H
#define AVM_CHRCODES_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif


#define avm_character_representation(character)\
  (((_avm_temporary_character=_avm_representations[character&0xff])->sharers)++?\
   _avm_temporary_character:_avm_temporary_character)

#define avm_standard_character_representation(character)\
  (((_avm_temporary_character=_avm_standard_representations[character&0xff])->sharers)++?\
   _avm_temporary_character:_avm_temporary_character)

  extern list _avm_temporary_character;
  extern list _avm_representations[256];
  extern list _avm_standard_representations[256];

  extern int avm_character_code (list operand);
  extern int avm_standard_character_code  (list operand);
  extern list avm_scanned_list (char *string);
  extern char *avm_prompt (list prompt_strings);
  extern char *avm_recoverable_prompt (list prompt_strings, list *message, int *fault);
  extern list avm_multiscanned (char **strings);
  extern list avm_strung (char *string);
  extern list avm_standard_strung (char *string);
  extern char *avm_unstrung (list string, list *message, int *fault);
  extern char *avm_standard_unstrung (list string, list *message, int *fault);
  extern list avm_recoverable_strung (char *string, int *fault);
  extern list avm_recoverable_standard_strung (char *string, int *fault);
  extern void avm_initialize_chrcodes ();
  extern void avm_count_chrcodes ();


#ifdef __cplusplus
}
#endif


#endif				/* !AVM_CHRCODES_H */
