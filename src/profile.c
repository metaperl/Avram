
/* profiling operations

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

#include <avm/error.h>
#include <avm/chrcodes.h>
#include <avm/compare.h>
#include <avm/profile.h>

/* a data base of profile statistics, with one item for each profiled code fragment */
static score board = NULL;

/* non-zero means local variables are initialized */
static int initialized = 0;

/* represents (nil,nil) */
static list shared_cell = NULL;

/* an error message represented as a list of lists of character representations */
static list memory_overflow = NULL;




score
avm_entries (team, message, fault)
     list team;
     list *message;
     int *fault;

     /* This looks up an entry in the score board whose team is given,
        or creates it if there isn't one, and returns the pointer to
        it; also increments the number of calls to the one looked
        up. */

{
  list temporary;
  score opened;
  int found;

  opened = board;
  found = *fault = 0;
  *message = NULL;
  while (!!opened & !*fault & !found)
    {
      if (found = ((*message = avm_binary_comparison (team, opened->team, fault)) ? !*fault : 0))
	{
	  avm_dispose (*message);
	  (opened->calls)++;
	  *message = NULL;
	}
      else if (!*fault)
	opened = opened->league;
    }
  if (!*fault & !found)
    {
      if (*fault = !(opened = (score) (malloc (sizeof (*opened)))))
	*message = avm_copied (memory_overflow);
      else
	{
	  memset (opened, 0, sizeof (*opened));
	  opened->team = avm_copied (team);
	  opened->league = board;
	  board = opened;
	}
    }
  return (*fault ? NULL : opened);
}






void
avm_tally (filename)
     char *filename;

     /* This outputs profile information to the file in a bare bones
	format suitable for major embellishments. */

{
  FILE *pro_file;
  int ioerror;
  list name;
  score old,reversed_board,temporary;
  double total,average,percentage;
  counter calls,reductions;
  int warned;
  int datum;

  if (!initialized)
    avm_initialize_profile ();
  total = 0.0;
  old = board;
  warned = 0;
  while (old)
    {
      total = total + (double) (old->reductions);
      old = old->league;
    }
  reversed_board = NULL;
  while (board)
    {
      temporary = board;
      board = board->league;
      temporary->league = reversed_board;
      reversed_board = temporary;
    }
  board = reversed_board; /* display in order of invocation */
  if (board ? board->league : 0) /* except for the unprofiled */
    {
      old = board;
      temporary = board;
      board = board->league;
      while (temporary ? temporary->league : 0)
	temporary = temporary->league;
      old->league = NULL;
      temporary->league = old;
    }
  if (board ? board->league : 0)
    {
      if (ioerror = !(pro_file = fopen (filename, "w")))
	avm_non_fatal_io_error ("can't write", filename, errno);
      else
	{
	  ioerror = fprintf(pro_file, "\n%12s  %12s  %12s  %12s\n\n","invocations","reductions", "average", "percentage");
	  if (ioerror = (ioerror == EOF))
	    avm_non_fatal_io_error ("can't write to", filename, errno);
	  while (!!board & !ioerror)
	    {
	      ioerror = (EOF == fprintf(pro_file, "%12u  ",calls = board->calls + 1));
	      ioerror = (ioerror ? 1 : (EOF == fprintf(pro_file, "%12u  ",reductions = board->reductions)));
	      average = ((double) reductions) / ((double) calls);
	      percentage = 100.0 * (((double) reductions) / total);
	      ioerror = (ioerror ? 1 : (EOF == fprintf(pro_file, "%12.1f  ",average)));
	      ioerror = (ioerror ? 1 : (EOF == fprintf(pro_file, "%12.3f  ",percentage)));
	      name = board->team;
	      while (!ioerror & !!name)
		{
		  if (ioerror = ((datum = avm_character_code (name->head)) < 0))
		    {
                      if(warned ? 0 : ++warned)
			avm_warning ("invalid profile identifier");
		    }
		  else if (ioerror = (putc (datum, pro_file) != datum))
		    avm_non_fatal_io_error ("can't write to", filename,errno);
		  name = name->tail;
		}
	      if (ioerror = (putc ('\n', pro_file) != '\n'))
		avm_non_fatal_io_error ("can't write to", filename, errno);
	      avm_dispose (board->team);
	      board = (old = board)->league;
	      free (old);
	    }
	  if (EOF == fprintf (pro_file, "\n%.0f reductions in total\n\n", total))
	    avm_non_fatal_io_error ("can't write to", filename, errno);
	  if (fclose (pro_file))
	    avm_non_fatal_io_error ("can't close", filename, errno);
	}
    }
}






void
avm_initialize_profile ()

     /* This initializes static data, which includes a global table of
	profile statistics. */
{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_chrcodes ();
  avm_initialize_compare ();
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
}







void
avm_count_profile ()

     /* This frees static data. */
{
  score old;

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (memory_overflow);
  memory_overflow = NULL;
  while (board)
    {
      avm_dispose (board->team);
      board = (old = board)->league;
      free (old);
    }
}
