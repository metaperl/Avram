#ifndef AVM_FNAMES_H
#define AVM_FNAMES_H 1

#include <avm/lists.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define avm_path_separator_character '/'
#define avm_path_separator_string "/"
#define avm_root_directory_prefix "/"
#define avm_current_directory_prefix "./"
#define avm_parent_directory_prefix "../"

  extern list avm_path_representation (char *path);
  extern list avm_date_representation (char *path);
  extern char *avm_path_name (list path);
  extern void avm_initialize_fnames ();
  extern void avm_count_fnames ();


#ifdef __cplusplus
}
#endif


#endif				/* !AVM_FNAMES_H */
