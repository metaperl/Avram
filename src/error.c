
/* functions for reporting errors and maybe exiting

   Copyright (C) 2006 Dennis Furey

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

extern char *xstrerror ();

/* the name of the program that is to appear at the beginning of error
   messages */
static char *program_name = NULL;


#define default_program_name (program_name?program_name:"avram")


void
avm_set_program_name (path)
     char *path;

     /* This sets the program name to be used in error messages of the
	form program-name: message; if none is set, avram is used by
	default. */

{
  if (program_name)
    free (program_name);
  if (!(program_name = (char *) malloc (strlen (path) + 1)))
    avm_error ("memory overflow (code 3)");
  strcpy (program_name, path);
}




char *
avm_program_name ()

     /* This returns the address of the program name used in error
	messages. */

{
  return default_program_name;
}





void
avm_internal_error (code)
     int code;

     /* This reports an internal error with the given code and
	exits. */

{
  fprintf (stderr, "%s: virtual machine internal error (code %d)\n",default_program_name, code);
  exit (EXIT_FAILURE);
}




void
avm_warning (message)
     char *message;

     /* This prints the messae but doesn't exit. */

{
  fprintf (stderr, "%s: %s\n", default_program_name, message);
}




void
avm_error (message)
     char *message;

     /* This prints the message and exits. */

{
  avm_warning (message);
  exit (EXIT_FAILURE);
}





void
avm_reclamation_failure (entity, count)
     char *entity;
     counter count;

     /* This non-fatally reports unreclaimed storage; entity is the
	type of storage and count is the number of units. */

{
  fprintf (stderr, "%s: %d unreclaimed %s\n", default_program_name, (int) count, entity);
}




void
avm_non_fatal_io_error (message, filename, reason)
     char *message;
     char *filename;
     int reason;

     /* This reports an i/o error associated with the file name for
	the given reason, but doesn't exit. */

{
  if (reason)
    {
#if HAVE_STRERROR
      fprintf (stderr, "%s: %s %s; %s\n", default_program_name, message,filename, xstrerror (reason));
#else
      fprintf (stderr, "%s: %s %s\n", default_program_name, message,filename);
#endif
    }
  else
    fprintf (stderr, "%s: %s %s\n", default_program_name, message,filename);
}




void
avm_fatal_io_error (message, filename, reason)
     char *message;
     char *filename;
     int reason;

     /* This reports an i/o error and exits. */
{
  avm_non_fatal_io_error (message, filename, reason);
  exit (EXIT_FAILURE);
}
