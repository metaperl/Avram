
/* functions writing lists out to text and data files

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
#include <avm/branches.h>
#include <avm/chrcodes.h>
#include <avm/rawio.h>
#include <avm/formin.h>
#include <avm/fnames.h>
#include <avm/formout.h>
#include <ctype.h>


/* non-zero means static variables are initialized */
static int initialized = 0;

/* the character code for a line break */
static int line_break = 10;




void
avm_output (repository, filename, preamble, contents, trace_mode)
     FILE *repository;
     char *filename;
     list preamble;
     list contents;
     int trace_mode;

     /* Iff the preamble is non-empty, the contents are output in raw
        data format. Iff the first character of the preamble is an
        exclamation point, the file is marked executable. If the
        preamble is empty, the contents are output as text. */

{
  int datum;
  list line;
  int ioerror;
  int incomment;
  int executable;

  if (!initialized)
    avm_initialize_formout ();
  executable = ioerror = incomment = 0;
  if (!repository)
    avm_internal_error (24);
  else if (preamble)
    {
      if (preamble->head ? 1 : !!(preamble->tail))
	{
	  incomment = 0;
	  while (preamble ? !ioerror : 0)
	    {
	      line = preamble->head;
	      if (!incomment)
		{
		  if (putc ('#', repository) != '#')
		    avm_non_fatal_io_error ("can't write to", filename, errno);
		}
	      incomment = 0;
	      while (line ? !ioerror : 0)
		{
		  if (line->head ? (line->head->characteristic ? 1 : 0) : 0)
		    {
		      incomment = (line->head->characterization == '\\');
		      if (!executable)
			executable = ((line->head == preamble->head->head) ? (line->head->characterization == '!') : 0);
		      if (putc (line->head->characterization, repository) != line->head->characterization)
			  avm_non_fatal_io_error ("can't write to", filename, errno);
		      else if (line->head->characterization == line_break)
			avm_error ("invalid output preamble format");
		    }
		  else
		    {
		      if ((datum = avm_character_code (line->head)) < 0)
			avm_error ("invalid output preamble format");
		      else if (datum == line_break)
			avm_error ("invalid output preamble format");
		      else if (putc (datum, repository) != datum)
			  avm_non_fatal_io_error ("can't write to", filename, errno);
		      else
			{
			  incomment = (datum == '\\');
			  executable = executable ? 1 : ((line->head == preamble->head->head) ? (datum == '!') : 0);
			}
		    }
		  line = line->tail;
		}
	      preamble = preamble->tail;
	      if (putc (line_break, repository) != line_break)
		avm_non_fatal_io_error ("can't write to", filename, errno);
	    }
	}
      if (executable ? (repository != stdout) : 0)
	chmod (filename, S_IXUSR | S_IXGRP | S_IXOTH | S_IRUSR | S_IRGRP | S_IROTH | S_IWUSR);
      avm_send_list (repository, contents, filename);
    }
  else
    {
      while (contents ? !ioerror : 0)
	{
	  line = contents->head;
	  while (line ? !ioerror : 0)
	    {
	      if (line->head ? (line->head->characteristic ? 1 : 0) : 0)
		{
		  if (trace_mode)
		    printf ("%c", line->head->characterization);
		  if (putc (line->head->characterization, repository) != line->head->characterization)
		    avm_non_fatal_io_error ("can't write to", filename, errno);
		}
	      else if ((datum = avm_character_code (line->head)) < 0)
		avm_error ("invalid text format (code 3)");
	      else if (putc (datum, repository) != datum)
		avm_non_fatal_io_error ("can't write to", filename, errno);
	      else if (trace_mode)
		printf ("%c", datum);
	      line = line->tail;
	    }
	  if (trace_mode ? (contents->tail) : 0)
	    printf ("\n");
	  if ((contents = contents->tail) ? putc (line_break, repository) != line_break : 0)
	    avm_non_fatal_io_error ("can't write to", filename, errno);
	}
    }
}






void
avm_output_as_directed (data, ask_to_overwrite_mode, verbose_mode)
     list data;
     int ask_to_overwrite_mode;
     int verbose_mode;

     /* This outputs a list of files specified in the form of a list
        of quadruples ((overwrite,path),(preamble,contents)). If the
        rewrite field is non-null, the file is opened for writing, but
        if it's null, the file is opened for appending. For security
        reasons, standard output is done last, so that malicious
        virtual code applications can't alter the query messages by
        using stdout to clobber the console.

        Thanks to Norm Pleszkoch for the part about buffered input.
     */

{
  int authorized;
  FILE *repository;
  char *filename;
  char *program_name;
  char key;
  list old, front_preamble, back_preamble, front_contents, back_contents;

#define current_file (data->head)
#define current_overwrite (current_file->head->head)
#define current_path (current_file->head->tail)
#define current_preamble (current_file->tail->head)
#define current_contents (current_file->tail->tail)

  if (!initialized)
    avm_initialize_formout ();
  program_name = avm_program_name ();
  front_preamble = back_preamble = front_contents = back_contents = NULL;
  while (data)
    {
      if (!(current_file) ? 1 : !(current_file->tail) ? 1 : !(current_file->head))
	avm_error ("invalid file specification");
      else if (!(filename = avm_path_name (current_path)))
	{
	  avm_enqueue (&front_preamble, &back_preamble, avm_copied (current_preamble));
	  avm_enqueue (&front_contents, &back_contents, avm_copied (current_contents));
	}
      else
	{
	  if (!(authorized = !(ask_to_overwrite_mode ? repository = fopen (filename, "rb") : 0)))
	    {
	      if (fclose (repository))
		avm_non_fatal_io_error ("can't close", filename, errno);
	      for (key = ' '; (authorized = (key != 'N')) && (key != 'Y');)
		{
		  printf ("%s: %s `%s'? (y/n) ", program_name, current_overwrite ? "overwrite" : "append to", filename);
		  fflush (stdout);	/* usually stdout is buffered. (sometimes stderr is also) */
		  key = toupper (getchar ());
		  if (key != '\n')
		    for (; '\n' != getchar (););
		}
	    }			/* flush out stdin up to (and including) the '\n' */
	  if (authorized)
	    {
	      if (!(repository = fopen (filename, current_overwrite ? "wb" : "ab")))
		avm_non_fatal_io_error ("can't write", filename, errno);
	      else
		{
		  if (verbose_mode)
		    {
		      if(current_overwrite)
			printf ("%s: writing `%s'\n", program_name, filename);
		      else
			printf ("%s: appending `%s'\n", program_name, filename);
		    }
		  avm_output (repository, filename, current_preamble, current_contents, 0);
		  if (fclose (repository))
		    avm_non_fatal_io_error ("can't close", filename, errno);
		}
	    }
	  else if (verbose_mode)
	    {
	      if(current_overwrite)
		printf ("%s: not writing `%s'\n", program_name, filename);
	      else
		printf ("%s: not appending `%s'\n", program_name, filename);
	    }
	  free (filename);
	}
      data = data->tail;
    }
  while (front_preamble)
    {
      avm_output (stdout, "standard output", front_preamble->head, front_contents->head, 0);
      front_preamble = avm_copied ((old = front_preamble)->tail);
      avm_dispose (old);
      front_contents = avm_copied ((old = front_contents)->tail);
      avm_dispose (old);
    }
}







void
avm_put_bytes (bytes)
     list bytes;

     /* This takes a list of character representations and sends it to
	standard output as characters. */

{
  int ioerror;
  int datum;

  ioerror = 0;
  if (!initialized)
    avm_initialize_formout ();
  while (bytes ? !ioerror : 0)
    {
      if ((datum = avm_character_code (bytes->head)) < 0)
	avm_error ("invalid text format (code 2)");
      else if (ioerror = (putc (datum, stdout) != datum))
	avm_non_fatal_io_error ("can't write to", "standard output", errno);
      bytes = bytes->tail;
    }
}







void
avm_initialize_formout ()

     /* This initializes some static data. */

{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_fnames ();
  avm_initialize_branches ();
  avm_initialize_chrcodes ();
  avm_initialize_formin ();
}






void
avm_count_formout ()

     /* This is a hook for future use. */

{
  if (!initialized)
    return;
  initialized = 0;
}
