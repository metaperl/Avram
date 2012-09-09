/* 
   avram - Applicative ViRtuAl Machine code interpreter

   Copyright (C) 2006-2010 Dennis Furey

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
*/

#define _GNU_SOURCE

#include <stdio.h>
#include <avm/common.h>
#include <avm/apply.h>
#include <avm/branches.h>
#include <avm/chrcodes.h>
#include <avm/cmdline.h>
#include <avm/compare.h>
#include <avm/decons.h>
#include <avm/error.h>
#include <avm/vman.h>
#include <avm/exmodes.h>
#include <avm/fnames.h>
#include <avm/formin.h>
#include <avm/formout.h>
#include <avm/instruct.h>
#include <avm/listfuns.h>
#include <avm/matcon.h>
#include <avm/libfuns.h>
#include <avm/lists.h>
#include <avm/portals.h>
#include <avm/ports.h>
#include <avm/profile.h>
#include <avm/rawio.h>
#include <avm/mwrap.h>
#include <avm/remote.h>
#include <avm/jobs.h>
#include <avm/farms.h>
#include <avm/vglue.h>
#include <avm/servlist.h>



/*------------------------------------------- memory management --------------------------------------------------- */



static void
initialize_everything ()
     /* This initializes some static variables used by the library. */
{
  avm_initialize_cmdline ();
  avm_initialize_exmodes ();
  avm_initialize_remote ();
  avm_initialize_apply ();
  avm_initialize_formin ();
  avm_initialize_formout ();
  avm_initialize_mwrap ();
  avm_initialize_rawio ();	/* the rest are initialized indirectly */
}



static void
reclaim_everything ()

     /* This reclaims static variables used by the library and checks
        for memory leaks. */
{
  avm_count_apply ();
  avm_count_branches ();
  avm_count_chrcodes ();
  avm_count_cmdline ();
  avm_count_compare ();
  avm_count_decons ();
  avm_count_exmodes ();
  avm_count_remote ();
  avm_count_fnames ();
  avm_count_formin ();
  avm_count_formout ();
  avm_count_instruct ();
  avm_count_listfuns ();
  avm_count_matcon ();
  avm_count_libfuns ();
  avm_count_portals ();
  avm_count_ports ();
  avm_count_profile ();
  avm_count_rawio ();
  avm_count_libfuns ();
  avm_count_mwrap ();
  avm_count_jobs ();
  avm_count_farms ();
  avm_count_vglue ();
  avm_count_servlist ();
  avm_count_lists ();    /* this one should be last */

}





/*------------------------------------------- information display ------------------------------------------------- */







static void
show_version ()

     /* prints a copyright notice and exits */
{
  char *notice = "\
Copyright (C) 2001,2006-2010 Dennis Furey. avram comes with NO WARRANTY,\n\
to the extent permitted by law. You may redistribute copies of avram\n\
under the terms of the GNU General Public License. For more information\n\
about these matters, see the files named COPYING.\n";

  printf ("avram version %s\n", avm_version ());
  printf (notice);
}










static void
show_libs ()

     /* prints a listing of configured libraries and exits */
{
#include "exf.c"
  list cell, result;

  cell = avm_join (NULL, NULL);
  avm_output (stdout, "standard output", NULL, result = avm_apply(avm_scanned_list (exf_code), cell), 0);
  avm_dispose (result);
}







static void
show_usage (arg)
     char *arg;

     /* displays information about command line options on standard
        error */
{
  char *usage = "\n\
avram %s, Applicative ViRtuAl Machine\n\n\
Usage:\n\
  avram [filter mode options] codefile[.avm] < inputfile\n\
  avram [parameter mode options] codefile[.avm] [command line parameters]\n\
  avram [general options] [codefile[.avm] command line parameters] \n\
\n\
general options:\n\
  -V,-v, --version            display the version of avram and exit\n\
  -h,    --help               print this help\n\
         --emulation=VERSION  be backward compatible with an old version\n\
         --open=HOST:PORT...  run concurrently on comma separated servers\n\
  -e,    --external-libraries show available external library functions\n\
  -f,    --force-text-input   don't infer .avm data format in input files\n\
  -j,    --jail               disable the interaction combinator\n\
\n\
filter mode options:\n\
  -r,    --raw-output         write stdout in .avm raw data file format\n\
  -c,    --choice-of-output   let the code specify raw or text output\n\
  -l,    --line-map           interpret code as a line oriented map\n\
  -b,    --byte-transducer    interpret code as a byte oriented transducer\n\
  -u,    --unparameterized    assume filter mode despite parameters\n\
\n\
parameter mode options:\n\
  -q,    --quiet              don't inform the user when writing files\n\
  -a,    --ask-to-overwrite   ask permission to overwrite existing files\n\
         -.EXT                assume a suffix .EXT on input file names\n\
  -d,    --default-to-stdin   read stdin if no file parameters are given\n\
  -m,    --map-to-each-file   invoke separately for each file parameter\n\
  -i,    --interactive        let the interpreted program run shell commands\n\
         --trace              echo dialogs of the interact combinator\n\
  -s,    --step               like --interactive but pausing for each command\n\
  -p,    --parameterized      assume parameter mode even with no parameters\n\
\n\
All filter mode options except -u are mutually exclusive. Parameter mode\n\
options -d and -m are mutually exclusive, -s implies -i, and -a overrides\n\
 -q. -j conflicts with -i, -t, and -s. Please send bug reports and\n\
suggestions to avram-support@basis.uklinux.net\n\n";

  if (arg ? ! strstr ("--help", arg) : 0)
    fprintf (stderr, "avram: unrecognized option: %s\n", arg);
  fprintf (stderr, usage, VERSION);
}





/*------------------------------------------- option parsing effects ---------------------------------------------- */





static int
emulating (option)
     char *option;

     /* this sets the main virtual machine emulation version to the
	given option string; clients and servers should use compatible
	character sets, which there's no easy way of checking */
{
  char *emulation;
  char *message;

  emulation = "-emulation";
  message = option;
  if (option && (*option == '-') && ((*(1 + option)) == '-'))
    option++;
  while (option && emulation && (*option == *emulation))
    {
      option++;
      emulation++;
    }
  if (!(option && (*option == '=')))
    return 0;
  avm_set_version (1 + option);
  return 1;
}










static void
connect_to_servers (argc, argv, verbose)
     unsigned int argc;
     char *argv[];
     int verbose;

     /* This scans for --open= options anywhere in the command line
	and registers the servers given by the parameters which,
	should be a comma separated list of colon separated hostnames
	or addresses and port numbers. */
{
  int input_index;
  char *option;
  int port_number;
  char *parameter;
  char *cursor;
  char *host;

  input_index = 0;
  while ((input_index < argc) ? (option = argv[input_index]) : NULL)
    {
      if ((! strcmp (option, "-open")) ? 1 : ! strcmp (option, "--open"))
	option = ((++input_index < argc) ? argv[input_index] : NULL);
      else if ((strstr(option,"--open=") == option) ? 0 : (strstr(option,"-open=") != option))
	option = NULL;
      else
	{
	  while (*option != '=')
	    option++;
	  option++;
	  option = (*option ? option : (++input_index < argc) ? argv[input_index] : NULL);
	}
      if (!option ? 0 : !*option ? 0 : (*option != '-'))
	{
	  if (!(cursor = parameter = strdup (option)))
	    avm_error ("memory overflow (code 15)");
	  while (*cursor)
	    {
	      host = cursor;
	      while (*cursor ? (*cursor != ':') : 0)
		cursor++;
	      if (!*cursor)
		avm_error ("bad --open specification (code 0)");
	      *cursor = 0;
	      cursor++;
	      port_number = 0;
	      while (isdigit (*cursor))
		port_number = (port_number * 10) + (*(cursor++) - '0');
	      if (!*cursor ? 0 : (*cursor != ','))
		avm_error ("bad --open specification (code 1)");
	      if (avm_registered_server (host, port_number) ? 0 : verbose)
		fprintf (stderr, "%s: unable to open server %s on port %d\n", avm_program_name (), host, port_number);
	      if (*cursor)
		{
		  cursor++;
		  if (!*cursor)
		    avm_error ("bad --open specification (code 2)");
		}
	    }
	  free (parameter);
	}
      input_index++;
    }
}












/*------------------------------------------- option parsing ------------------------------------------------------ */




#define VERSION_MODE 0x1
#define UNPARAMETERIZED 0x2
#define RAW_OUTPUT 0x4
#define FORCE_TEXT_INPUT 0x8
#define CHOICE_OF_OUTPUT 0x10
#define LINE_MAP 0x20
#define BYTE_TRANSDUCER 0x40
#define QUIET 0x80
#define MAP_TO_EACH_FILE 0x100
#define PARAMETERIZED 0x200
#define INTERACTIVE 0x400
#define TRACE 0x800
#define STEP 0x1000
#define DEFAULT_TO_STDIN 0x2000
#define ASK_TO_OVERWRITE 0x4000
#define EXTERNAL_LIBRARIES 0x8000
#define JAIL 0x10000
#define OPEN 0x20000






int
tracing (argc, argv)
     unsigned int argc;
     char *argv[];

     /* returns true if the --trace option appears anywhere on the
        command line, even beyond the range that is normally parsed
        for flags */
{
  int input_index;

  input_index = 0;
  while (input_index < argc)
    if (! strcmp(argv[input_index++],"--trace"))
      return 1;
  return 0;
}

















static int
bad_usage (input_index, argc, flags, extension)
     unsigned int input_index;
     unsigned int argc;
     int flags;
     char *extension;

     /* checks whether command line options are selected consistently
	and returns true if they aren't */
{
  int result;

  if ((input_index) > argc)
    return 1;
  if ((input_index) == argc)
    return !(flags & (VERSION_MODE | EXTERNAL_LIBRARIES));
  if (flags & (RAW_OUTPUT | CHOICE_OF_OUTPUT | LINE_MAP |  BYTE_TRANSDUCER | UNPARAMETERIZED))
    {
      result = (flags & UNPARAMETERIZED) ? 0 : ((input_index) < (argc - 1));
      result = result | (flags & (ASK_TO_OVERWRITE | PARAMETERIZED | INTERACTIVE | VERSION_MODE));
      result = result | (flags & (STEP | QUIET | MAP_TO_EACH_FILE | DEFAULT_TO_STDIN));
      result = result | ((flags & CHOICE_OF_OUTPUT) ? (flags & (RAW_OUTPUT | LINE_MAP | BYTE_TRANSDUCER)) :  0);
      result = result | ((flags & RAW_OUTPUT) ? (flags & (LINE_MAP | BYTE_TRANSDUCER)) : 0);
      result = result | ((flags & LINE_MAP) ? (flags & BYTE_TRANSDUCER) : 0);
      result = result | !!extension;
    }
  else
    {
      result = flags & (RAW_OUTPUT | VERSION_MODE | LINE_MAP | BYTE_TRANSDUCER | CHOICE_OF_OUTPUT);
      result = result | ((flags & DEFAULT_TO_STDIN) ? (flags & MAP_TO_EACH_FILE) : 0);
    }
  if (result ? 0 : (flags & JAIL))
    result = (flags & (INTERACTIVE | TRACE | STEP));
  return result;
}



















static unsigned int
the_flags_of (argc, argv, extension, inin)
     unsigned int argc;
     char *argv[];
     char **extension;
     unsigned int *inin;

     /* This fills in the flags with the command line options, points
        the extension to the -.EXT option, if any, and leaves
        inin pointing to the position of the first non-flag on
        the command line, which is probably the name of the virtual
        code file. */
{
#define recognized_long ((*(1 + argv[*inin])=='-') ? ((*(2 + argv[*inin])) ? strstr (keywords, argv[*inin]) : 0) : 0)
#define recognized_short ((strlen (argv[*inin]) == 2) ? strstr (keyletters, argv[*inin]) : 0)
#define recognized_option (offset = (recognized_long ? 2 : (recognized_short ? 1 : 0)))
#define set_if(character,flag) flags = flags | ((*(offset + argv[*inin]) == character) ? flag : 0)

  char *option;
  char *emulation;
  unsigned int flags, offset;
  char *keyletters = "-r-f-q-u-a-V-v-p-s-i-l-b-m-c-d-n-e-j";   /* must be mutually distinct, unused are gknwxyz */
  char *keywords = "\
--raw-output--force-text-input--quiet--unparameterized\
--ask-to-overwrite--step--trace--map-to-each-file\
--default-to-stdin--external-libraries--open-connections\
--version--parameterized--interactive--jail\
--line-map--byte-transducer--choice-of-output";

  *inin = 0;
  flags = 0;
  *extension = NULL;
  while (++(*inin) >= argc ? 0 : argv[*inin] ? *(argv[*inin]) == '-' : 0)
    {
      if (emulating (argv[*inin]) ? 0 : recognized_option)
	{
	  set_if ('r', RAW_OUTPUT);
	  set_if ('i', INTERACTIVE);
	  set_if ('l', LINE_MAP);
	  set_if ('v', VERSION_MODE);
	  set_if ('V', VERSION_MODE);
	  set_if ('b', BYTE_TRANSDUCER);
	  set_if ('c', CHOICE_OF_OUTPUT);
	  set_if ('m', MAP_TO_EACH_FILE);
	  set_if ('a', ASK_TO_OVERWRITE);
	  set_if ('p', PARAMETERIZED);
	  set_if ('s', STEP);
	  set_if ('d', DEFAULT_TO_STDIN);
	  set_if ('t', TRACE);
	  set_if ('f', FORCE_TEXT_INPUT);
	  set_if ('u', UNPARAMETERIZED);
	  set_if ('e', EXTERNAL_LIBRARIES);
	  set_if ('o', OPEN);
	  set_if ('j', JAIL);
	  set_if ('q', QUIET);
	}
      else if (*(1 + argv[*inin]) == '.')
	*extension = 1 + argv[*inin];
      else
	{
	  show_usage (argv[*inin]);
	  exit (strstr ("--help", argv[*inin]) ? EXIT_SUCCESS : EXIT_FAILURE);
	}
    }
  if (!(bad_usage (*inin, argc, flags, *extension)))
    return flags;
  show_usage (NULL);
  exit (EXIT_FAILURE);
}








/*------------------------------------------- parameter extraction ------------------------------------------------ */










static char
*avminputs (env)
     char *env[];

     /* This returns a pointer to the AVMINPUTS environment variable
        if found, which is a colon separated list of directory
        names. */
{
  int index;
  char *result;
  char *variable;
  char *default_paths = ".:/usr/local/lib/avm:/usr/lib/avm:/lib/avm:/opt/avm:/opt/lib/avm\
:/usr/local/share/avm:/usr/share/avm:/share/avm:/opt/avm:/opt/share/avm";

  index = 0;
  result = NULL;
  while (result ? 0 : env[index])
    {
      variable = env[index++];
      if (strstr (variable, "AVMINPUTS=") == variable)
	{
	  while ((*variable) ? ((*variable) != '=') : 0)
	    variable++;
	  if (*variable)
	    variable++;
	  result = variable;
	}
    }
  return ((result ? *result : 0) ? result : default_paths);
}











static FILE
*code_file (arg)
     char *arg;

     /* tries to open a file by the given name, and also with the
	default .avm suffix if there is no suffix */
{
  char *filename;
  FILE *code;

  if (code = fopen (arg, "rb"))
    return code;
  if(!strstr(".",arg))
    avm_fatal_io_error ("can't read", arg, errno);
  if (!(filename = (char *) malloc (strlen (arg) + 5)))
    avm_error ("memory overflow (code 1)");
  if (!(code = fopen (strcat (strcpy (filename, arg), ".avm"),"rb")))
    avm_fatal_io_error ("can't read", filename, errno);
  free (filename);
  return code;
}












static list
parametric (flags, argv, argc, input_index, extension, paths, environs)
     int flags;
     char *argv[];
     unsigned int argc;
     char *extension;
     char *paths;
     list environs;

     /* constructs the main program operand for execution modes other
	than filtering */
{
  list operand;
  int dts, fti;

  operand = NULL;
  dts = flags & DEFAULT_TO_STDIN;
  fti = flags & FORCE_TEXT_INPUT;
  if (!(flags & UNPARAMETERIZED))
    operand = avm_default_command_line (argc, argv, input_index, extension, paths, dts, fti, NULL);
  if (operand)
    return avm_join (operand, environs);
  if (extension)
    return avm_join (avm_join (NULL, NULL), environs);
  if (flags & (ASK_TO_OVERWRITE | QUIET | PARAMETERIZED | INTERACTIVE | TRACE | STEP | DEFAULT_TO_STDIN))
    return avm_join (avm_join (NULL, NULL), environs);
  avm_dispose (environs);
  return NULL;
}






/*------------------------------------------- execution modes ----------------------------------------------------- */





static void
mapper_mode (argc, argv, input_index, paths, flags, extension, operator, environs)
     int argc;
     char *argv[];
     int input_index;
     int flags;
     char *paths;
     char *extension;
     list operator;
     list environs;

     /* loops through the input files on the command line instead of
	loading them all at once */
{
  int dts, fti, file_ordinal;
  list result, operand;

  file_ordinal = 0;
  operand = result = NULL;
  dts = flags & DEFAULT_TO_STDIN;
  fti = flags & FORCE_TEXT_INPUT;
  while ((operand = avm_default_command_line (argc, argv, input_index, extension, paths, dts, fti, &file_ordinal)))
    {
      operand = avm_join (operand ? operand : avm_join (NULL, NULL), avm_copied (environs));
      result = avm_apply (avm_copied (operator), operand);
      if (flags & (INTERACTIVE | STEP))
	avm_interact (result, flags & STEP, flags & ASK_TO_OVERWRITE, flags & QUIET);
      else
	{
	  avm_output_as_directed (result, flags & ASK_TO_OVERWRITE, !(flags & QUIET));
	  avm_dispose (result);
	  result = NULL;
	}
    }
  avm_dispose (operator);
  avm_dispose (environs);
}











static void
parameter_mode (flags, operand, code, arg)
     int flags;
     list operand;
     FILE *code;
     char *arg;

     /* executes the main program according to either parameterized or
	interactive operation */
{
  list result;

  result = avm_apply (avm_received_list (code, arg), operand);
  if (flags & (INTERACTIVE | STEP))
    avm_interact (result, flags & STEP, flags & ASK_TO_OVERWRITE, flags & QUIET);
  else
    avm_output_as_directed (result, flags & ASK_TO_OVERWRITE, ! (flags & QUIET));
  avm_dispose (result);
}









static void
text_mode (flags, code, arg)
     int flags;
     FILE *code;
     char *arg;

     /* executes as a filter with the input file treated as text
	only */
{
  list result, cell;

  cell = NULL;
  if (flags & CHOICE_OF_OUTPUT)
    result = avm_apply (avm_received_list (code, arg), avm_join (NULL, avm_load (stdin, NULL, 0)));
  else
    result = avm_apply (avm_received_list (code, arg), avm_load (stdin, NULL, 0));
  if ((flags & CHOICE_OF_OUTPUT) ? result : NULL)
    avm_output (stdout, "standard output", result->head, result->tail, 0);
  else
    avm_output (stdout, "standard output", (flags & RAW_OUTPUT) ? (cell = avm_join (NULL, NULL)) : NULL, result, 0);
  avm_dispose (result);
  avm_dispose (cell);
}










static void
binary_mode (flags, code, arg)
     int flags;
     FILE *code;
     char *arg;

     /* executes as a filter with the input file disambiguated between
        text and binary format */
{
  list result, cell, operand;

  cell = NULL;
  operand = avm_preamble_and_contents (stdin, "standard input");
  if (flags & CHOICE_OF_OUTPUT)
    result = avm_apply (avm_received_list (code, arg), avm_copied (operand));
  else  /* ignore preamble */
    result = avm_apply (avm_received_list (code, arg), avm_copied (operand->tail));
  avm_dispose (operand);
  if ((flags & CHOICE_OF_OUTPUT) ? result : NULL)
    avm_output (stdout, "standard output", result->head, result->tail, 0);
  else /* don't expect preamble */
    avm_output (stdout, "standard output", (flags & RAW_OUTPUT) ? (cell = avm_join (NULL, NULL)) : NULL, result, 0);
  avm_dispose (result);
  avm_dispose (cell);
}






/*----------------------------------------------------------------------------------------------------------------- */





int
main (argc, argv, env)
     unsigned int argc;
     char *argv[];
     char *env[];
{
#define PRO_FILENAME "profile.txt"

  FILE *code;
  char *extension;
  list operand;
  unsigned int input_index = 0;
  unsigned int flags;

  initialize_everything ();
  avm_set_program_name (basename (argv[0]));
  if ((flags = the_flags_of (argc, argv, &extension, &input_index)) & (VERSION_MODE | EXTERNAL_LIBRARIES))
    {
      if (flags & EXTERNAL_LIBRARIES)
	show_libs ();
      if (flags & VERSION_MODE)
	show_version ();
      reclaim_everything ();
      exit (EXIT_SUCCESS);
    }
  avm_set_program_name (basename (argv[input_index]));
  connect_to_servers (argc, argv, ! flags & QUIET);
  if (flags & JAIL)
    avm_disable_interaction ();
  if (tracing (argc, argv))
    avm_trace_interaction ();
  code = code_file (argv[input_index]);
  if (flags & MAP_TO_EACH_FILE)
    {
      operand = avm_received_list (code, argv[input_index++]);
      mapper_mode (argc, argv, input_index, avminputs (env), flags, extension, operand, avm_environment (env));
    }
  else if (operand = parametric (flags, argv, argc, input_index + 1, extension, avminputs (env), avm_environment (env)))
    parameter_mode (flags, operand, code, argv[input_index]);
  else if (flags & LINE_MAP)
    avm_line_map (operand = avm_received_list (code, argv[input_index]));
  else if (flags & BYTE_TRANSDUCER)
    avm_byte_transduce (operand = avm_received_list (code, argv[input_index]));
  else if (flags & FORCE_TEXT_INPUT)
    text_mode (flags, code, argv[input_index]);
  else
    binary_mode (flags, code, argv[input_index]);
  if (fclose (code))
    avm_non_fatal_io_error ("can't close", argv[(flags & MAP_TO_EACH_FILE) ? input_index : (--input_index)], errno);
  avm_tally (PRO_FILENAME);
  reclaim_everything ();
  exit (EXIT_SUCCESS);
}
