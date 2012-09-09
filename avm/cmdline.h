#ifndef AVM_CMDLINE_H
#define AVM_CMDLINE_H 1

#ifdef __cplusplus
extern "C"
{
#endif

  extern list avm_default_command_line (int argc, 
					char *argv[],
					int index,
					char *extension,
					char *paths,
					int default_to_stdin_mode,
					int force_text_input_mode,
					int *file_ordinal);

  extern list avm_environment (char *env[]);
  extern void avm_initialize_cmdline ();
  extern void avm_count_cmdline ();

#ifdef __cplusplus
}
#endif


#endif				/* !AVM_CMDLINE_H */
