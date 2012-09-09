
/* functions for command line parsing and environment variables

   Copyright (C) 2006,2010 Dennis Furey

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


#include <avm/common.h>
#include <avm/error.h>
#include <avm/lists.h>
#include <avm/branches.h>
#include <avm/chrcodes.h>
#include <avm/rawio.h>
#include <avm/formin.h>
#include <avm/formout.h>
#include <avm/fnames.h>
#include <avm/cmdline.h>
#include <time.h>
#if HAVE_ARGZ_H
#include <argz.h>
#endif

#ifndef HAVE_MEMMOVE
extern void 
*memmove(char *dest, const char *source, unsigned length)
#endif

/* Non-zero means static variables have been initialized. */
static int initialized = 0;

/* Non-zero means standard input has been read and stored already. */
static int stdin_cached = 0;

/* represents (nil,nil) */
static list shared_cell = NULL;

/* stores a list representation of the standard input file */
static list stdin_cache = NULL;

/* Non-zero means the user has already been warned that search paths
   aren't supported. */
static int warned = 0;




static void
match_file (argv, source, filename, extension, search_paths, search_paths_length)
     char *argv;
     FILE **source;
     char **filename;
     char *extension;
     char *search_paths;
     size_t search_paths_length;

     /* This finds the full file name using search paths and default
        extensions, and opens it as source. */
{
  char *search_path;
  char *entry;
  int absolute, relative, extensible;
  size_t count;

  absolute = 0;
  relative = 0;
  *source = NULL;
  *filename = NULL;
  entry = argv;
  while (entry ? strstr (entry, avm_path_separator_string) : 0)
    entry++;
  while (entry ? ((*entry) == '.') : 0)
    entry++;
  extensible = (entry ? (!strstr (entry, ".")) : 0);
  entry = NULL;
#if HAVE_ARGZ_H
  while ((*filename)?0:(absolute?0:(entry=(search_paths?argz_next(search_paths,search_paths_length,entry):(entry?NULL:".")))))
    {
#else /* not HAVE_ARGZ_H */
  while ((*filename) ? 0 : (absolute ? 0 : (entry = (entry ? NULL : "."))))
    {
#endif /* HAVE_ARGZ_H */
      if (!(search_path = (char *) malloc (strlen (argv) + strlen (entry) + strlen (avm_path_separator_string) + 1)))
	avm_error ("memory overflow (code 14)");
      else if (relative)
	strcat (strcat (strcpy (search_path, entry), avm_path_separator_string), argv);
      else
	{
	  absolute = strstr (argv, avm_current_directory_prefix) == argv;
	  absolute = (absolute ? 1 : (strstr (argv, avm_parent_directory_prefix) == argv));
	  if (absolute = (absolute ? 1 : (strstr (argv, avm_root_directory_prefix) == argv)))
	    strcpy (search_path, argv);
	  else
	    {
	      relative = 1;
	      strcat (strcat(strcpy (search_path, entry), avm_path_separator_string), argv);
	    }
	}
      if (*source = fopen (search_path, "rb"))
	{
	  if (!(*filename = (char *) malloc (strlen (search_path) + 5)))
	    avm_error ("memory overflow (code 5)");
	  else
	    strcpy (*filename, search_path);
	}
      else if (extensible ? (extension ? !((*filename) = (char *) malloc(strlen(search_path)+1+strlen(extension))) : 0) : 0)
	avm_error ("memory overflow (code 6)");
      else if (extensible ? (extension ? !(*source = fopen(strcat(strcpy(*filename,search_path),extension),"rb")) : 1) : 0)
	{
	  if (*filename)
	    {
	      free (*filename);
	      *filename = NULL;
	    }
	  if (!(*filename = (char *) malloc (strlen (search_path) + 5)))
	    avm_error ("memory overflow (code 7)");
	  if (!(*source = fopen (strcat (strcpy (*filename, search_path), ".avm"),"rb")))
	    {
	      free (*filename);
	      if (!(*filename = (char *) malloc (strlen (search_path) + 5)))
		avm_error ("memory overflow (code 8)");
	      if (!(*source = fopen (strcat (strcpy (*filename, search_path), ".fun"),"rb")))
		{
		  free (*filename);
		  *filename = NULL;
		}
	    }
	}
      free (search_path);
    }
  if (!*source)
      avm_fatal_io_error ("can't read", argv, errno);
  if (strstr (*filename, avm_current_directory_prefix) == *filename)
    {
      count = strlen (*filename) - strlen (avm_current_directory_prefix) + 1;
      memmove (*filename, (*filename) + strlen (avm_current_directory_prefix), count);
    }
}







static list
cached_stdin ()

  /* This returns a copy of standard input represented as a list. The
     first time it's called, it reads from standard input, but
     subsequently it returns a cached copy. */
{
  char buffer[26];
  time_t the_time;
  char *now;
  list front_character, back_character;

  if (!stdin_cached)
    {
      the_time = time (NULL);
#if HAVE_CTIME_R
      ctime_r (&the_time, buffer);
      now = buffer;
#else /* not HAVE_CTIME_R */
      now = ctime (&the_time);
#endif /* HAVE_CTIME_R */
      front_character = back_character = NULL;
      while (*now ? (*now != '\n') : 0)
	avm_enqueue (&front_character, &back_character, avm_character_representation (*(now++)));
      stdin_cache = avm_join (avm_join (front_character, NULL), avm_preamble_and_contents (stdin, NULL));
      stdin_cached = 1;
    }
  return avm_copied (stdin_cache);
}






list
avm_default_command_line (argc, argv, index, extension, paths, default_to_stdin_mode, force_text_input_mode, file_ordinal)
     int argc;
     char *argv[];
     int index;
     char *extension;
     char *paths;		/* a colon separated list of directory names */
     int default_to_stdin_mode;
     int force_text_input_mode;
     int *file_ordinal;

     /* This function returns a list of the files and options given in
        the command line starting from the argument indicated by the
        index. By default, command lines are interpreted subject to
        the following conventions.

        1) An argument is treated as a keyword iff it meets these three conditions.
          a) It starts with a dash.
          b) It doesn't contain an equals sign.
          c) It doesn't consist solely of a dash.

        2) An argument is treated as a parameter list iff it meets these four conditions.
          a) It doesn't begin with a dash.
          b) It either begins with an equals sign or doesn't contain one.
          c) It immediately follows an argument beginning with a dash and not
             containing an equals sign.
          d) At least one of the following is true.
            1) It doesn't contain a period, asterisk, tilde, or slash.
            2) It contains a comma.
            3) It can be interpreted as a C style floating point number.

        3) An argument is treated as an input file name iff it meets these four conditions.
          a) It doesn't begin with a dash.
          b) It doesn't contain an equals sign.
          c) It doesn't contain a comma.
          d) At least one of the following is true.
            1) It contains a period, asterisk, tilde, or slash.
            2) It doesn't immediately follow an argument beginning with a
               dash and not containing an equals sign.

        4) If an argument contains an equals sign but doesn't begin with one,
           the part on the left of the equals sign is treated as a keyword and
           the part on the right is treated as a parameter list.

        5) An argument consisting solely of a dash is taken to represent the
           standard input file.

        6) An argument not fitting any of the above classifications is an
           error.

        Options are represented as ((position,longform),(keyword,parameters)),
        and files are represented as ((date,path),(preamble,contents)).

        If the file_ordinal parameter is non-null, then all file parameters in
        the command line are ignored except for that of the given ordinal and
        standard input. If there is no such file, then a NULL value is
        returned for the whole command line.  Otherwise, the ordinal is
        incremented. */

#define fileish(s) ((sscanf(s,"%e",&temporary_double)==1)?0:\
  (strchr(s,'.')?1:strchr(s,avm_path_separator_character)?\
  1:strchr(s,'*')?1:*(s)=='~'?strcmp(argv[index],"~"):0))

{
  double temporary_double;
  char *search_paths;		/* a null separated list of directory names */
  FILE *source;
  char *filename;
  char *keyword;
  char *parameter;
  counter position;
  int file_counter;
  int parameters_expected;
  list longform;
  size_t search_paths_length;
  list front_file, back_file, front_option, back_option, front_parameter,
    back_parameter, front_character, back_character, temporary, file_buffer;

  if (!initialized)
    avm_initialize_cmdline ();
  search_paths = NULL;
  search_paths_length = 0;
#if HAVE_ARGZ_H
  if (paths ? argz_add_sep (&search_paths, &search_paths_length, paths, ':') : 0)
    avm_error ("memory overflow (code 13)");
#else /* not HAVE_ARGZ_H */
  if (paths ? !warned : 0)
    {
      warned = 1;
      avm_warning ("warning: search paths not supported");
    }
#endif /* HAVE_ARGZ_H */
  position = parameters_expected = file_counter = 0;
  front_file = back_file = front_option = back_option = NULL;
  while (index < argc)
    {
      /*printf("argv[%d] = %s\n",index,argv[index]);*/
      if (!(argv[index]))
	avm_internal_error (17);
      else if (!strcmp (argv[index], "-"))
	{
	  parameters_expected = 0;
	  avm_enqueue (&front_file, &back_file, cached_stdin ());
	  position++;
	}
      else if (strchr (argv[index], '=') ? (*(argv[index]) != '=') : 0)
	{
	  parameters_expected = 0;
	  keyword = (((*(argv[index])) == '-') ? (1 + argv[index]) : argv[index]);
	  front_character = back_character = longform = NULL;
	  if ((*keyword) == '-')
	    {
	      keyword++;
	      longform = avm_copied (shared_cell);
	    }
	  while ((*keyword) != '=')
	    avm_enqueue (&front_character, &back_character,avm_character_representation (*(keyword++)));
	  temporary = front_character;
	  parameter = ++keyword;
	  front_parameter = back_parameter = front_character = back_character = NULL;
	  while (*parameter)
	    {
	      if ((*parameter) != ',')
		avm_enqueue (&front_character, &back_character,avm_character_representation (*(parameter++)));
	      else
		{
		  avm_enqueue (&front_parameter, &back_parameter,front_character);
		  front_character = back_character = NULL;
		  parameter++;
		}
	    }
	  if (front_character)
	    avm_enqueue (&front_parameter, &back_parameter,front_character);
	  temporary = avm_join (avm_join (avm_natural (position++), longform),avm_join (temporary, front_parameter));
	  avm_enqueue (&front_option, &back_option, temporary);
	}
      else if (*(argv[index]) == '-')
	{
	  parameters_expected = 1;
	  keyword = argv[index] + 1;
	  front_character = back_character = longform = NULL;
	  if ((*keyword) == '-')
	    {
	      keyword++;
	      longform = avm_copied (shared_cell);
	    }
	  while (*keyword)
	    avm_enqueue (&front_character, &back_character,avm_character_representation (*(keyword++)));
	  temporary = avm_join (avm_join (avm_natural (position++), longform),avm_join (front_character, NULL));
	  avm_enqueue (&front_option, &back_option, temporary);
	}
      else if (parameters_expected)
	{
	  parameters_expected = 0;
	  if (*(argv[index]) == '=' ? 1 : (strchr (argv[index], ',') ? 1 : !fileish (argv[index]) ))
	    {
	      parameter = (((*(argv[index])) == '=') ? (1 + argv[index]) : argv[index]);
	      front_parameter = back_parameter = front_character = back_character = NULL;
	      while (*parameter)
		if ((*parameter) != ',')
		  avm_enqueue (&front_character, &back_character,avm_character_representation (*(parameter++)));
		else
		  {
		    avm_enqueue (&front_parameter, &back_parameter, front_character);
		    front_character = back_character = NULL;
		    parameter++;
		  }
	      avm_enqueue (&front_parameter, &back_parameter, front_character);
	      back_option->head->tail->tail = front_parameter;
	    }
	  else 
	    {
	      if (file_ordinal ? (file_counter++ == *file_ordinal) : 1)
		{
		  match_file (argv[index], &source, &filename, extension,search_paths, search_paths_length);
		  temporary = avm_join (avm_date_representation (filename),avm_path_representation (filename));
		  if (force_text_input_mode)
		    file_buffer = avm_join (NULL,avm_load (source, filename,0));
		  else
		    file_buffer = avm_preamble_and_contents (source,filename);
		  avm_enqueue (&front_file, &back_file,avm_join (temporary, file_buffer));
		  free (filename);
		  position++;
		}
	    }
	}
      else if (file_ordinal ? (file_counter++ == *file_ordinal) : 1)
	{
	  parameters_expected = 0;
	  match_file (argv[index], &source, &filename, extension,search_paths, search_paths_length);
	  temporary = avm_join (avm_date_representation (filename),avm_path_representation (filename));
	  if (force_text_input_mode)
	    file_buffer = avm_join (NULL,avm_load (source, filename,0));
	  else
	    file_buffer = avm_preamble_and_contents (source,filename);
	  avm_enqueue (&front_file, &back_file,avm_join (temporary, file_buffer));
	  free (filename);
	  position++;
	}
      index++;
    }
  if (front_file ? 0 : (default_to_stdin_mode ? !file_ordinal : 0))
    {
      avm_enqueue (&front_file, &back_file, cached_stdin ());
      stdin_cached = 0;
      avm_dispose (stdin_cache);
      stdin_cache = NULL;
    }
#if HAVE_ARGZ_H
  if (search_paths)
    free (search_paths);
#endif /* HAVE_ARGZ_H */
  if (file_ordinal)
    {
      if (front_file ? (stdin_cached ? (!!(front_file->tail)) : 1) : 0)
	{
	  (*file_ordinal)++;
	  return (avm_join (front_file, front_option));
	}
      else
	{
	  avm_dispose (front_option);
	  avm_dispose (front_file);
	  return (NULL);
	}
    }
  else
    {
      if (stdin_cached)
	{
	  stdin_cached = 0;
	  avm_dispose (stdin_cache);
	  stdin_cache = NULL;
	}
      if (front_file)
	return avm_join (front_file, front_option);
      return (front_option ? avm_join (front_file, front_option) : NULL);
    }
}









list
avm_environment (env)
     char *env[];

     /* This returns a list of pairs (identifier,setting) given a
        pointer to a null terminated array of pointers to null
        terminated strings. */
{
  int index;
  char *variable;
  list temporary, front_character, back_character, front_variable, back_variable;

  if (!initialized)
    avm_internal_error (18);
  index = 0;
  front_variable = back_variable = NULL;
  while (env[index])
    {
      front_character = back_character = NULL;
      variable = env[index++];
      while ((*variable) ? ((*variable) != '=') : 0)
	avm_enqueue (&front_character, &back_character,avm_character_representation (*(variable++)));
      temporary = front_character;
      front_character = back_character = NULL;
      if (*variable)
	variable++;
      while (*variable)
	avm_enqueue (&front_character, &back_character,avm_character_representation (*(variable++)));
      avm_enqueue (&front_variable, &back_variable,avm_join (temporary, front_character));
    }
  return front_variable;
}






void
avm_initialize_cmdline ()
     /* This initializes some local data structures. */
{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_branches ();
  avm_initialize_chrcodes ();
  avm_initialize_fnames ();
  avm_initialize_formout ();
  avm_initialize_formin ();
  stdin_cache = NULL;
  stdin_cached = 0;
  shared_cell = avm_join (NULL, NULL);
}





void
avm_count_cmdline ()
     /* This frees up some local data structures, including the cached
        copy of standard input represented as a list. */
{
  if (!initialized)
    return;
  initialized = 0;
  warned = 0;
  avm_dispose (shared_cell);
  shared_cell = NULL;
  if (stdin_cached)
    {
      stdin_cached = 0;
      avm_dispose (stdin_cache);
      stdin_cache = NULL;
    }
}
