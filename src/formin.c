
/* functions reading text and data files into lists

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

/* represents (nil,nil) */
static list shared_cell = NULL;

/* non-zero means static variables are initialized */
static int initialized = 0;

/* the character code for a line break */
static int line_break = 10;



static list
initial_comments (line)
     list line;

     /* This takes a list of character strings represented as lists
        and returns the comments, if any, which will be the maximum
        prefix of the list of strings in which each item either begins
        with a hash or follows one that ends with a backslash. 

        Some sneaky hacks are used here. Character comparisons are
        done by comparing pointers to members of the array
        avm_representations, which is declared in chrcodes.h. Direct
        use of this array is an undocument feature of the library, but
        it is used here to avoid the need for disposing of
        it. Furthermore, pointer equality is not generally a valid
        test for comparison, but in this case the list is assumed to
        be recently created using calls to
        avm_character_representation, so it should be all right. */

{
  list column, front_line, back_line, front_column, back_column;
  int continued;

  continued = 0;
  front_line = back_line = NULL;
  while (line)
    {
      column = line->head;
      line = line->tail;
      front_column = back_column = NULL;
      if (continued)
	{
	  continued = 0;
	  while (column)
	    {
	      continued = (column->head == _avm_representations['\\']);
	      avm_enqueue (&front_column, &back_column,avm_copied (column->head));
	      column = column->tail;
	    }
	  avm_enqueue (&front_line, &back_line, front_column);
	}
      else if (column ? (column->head == _avm_representations['#']) : 0)
	{
	  column = column->tail;
	  while (column)
	    {
	      continued = (column->head == _avm_representations['\\']);
	      avm_enqueue (&front_column, &back_column, avm_copied (column->head));
	      column = column->tail;
	    }
	  avm_enqueue (&front_line, &back_line, front_column);
	}
      else
	line = NULL;
    }
  return (front_line);
}






list
avm_preamble_and_contents (source, filename)
     FILE *source;
     char *filename;

     /* This function is used for file input when it has to guess
	whether the file is in raw or text format. */
{
  int spool, datum, last_character_on_previous_line, textual, spoke, previously_in_comment, in_comment;
  list raw_data, front_line, back_line, front_column, back_column, preamble, contents;
  branch_queue front_branch, back_branch;

  if (!initialized)
    avm_initialize_formin ();
  front_branch = back_branch = NULL;
  front_line = back_line = raw_data = NULL;
  avm_anticipate (&front_branch, &back_branch, &raw_data);
  textual = in_comment = last_character_on_previous_line = 0;
  if ((spool = datum = getc (source)) != EOF)
    {
      front_column = back_column = NULL;
      in_comment = (datum == '#');
      while (datum != line_break ? datum != EOF : 0)
	{
	  if (in_comment ? 0 : (textual ? 0 : !(textual=(front_branch ? (spool<60 ? 1 : (spool=spool-60) & 0xffc0) : 1))))
	    {
	      spoke = 6;
	      while (spoke)
		avm_enqueue_branch (&front_branch, &back_branch,(spool >> (--spoke)) & 1);
	    }
	  avm_enqueue (&front_column, &back_column,avm_character_representation (datum));
	  last_character_on_previous_line = datum;
	  spool = datum = getc (source);
	}
      avm_enqueue (&front_line, &back_line, front_column);
      while (datum == line_break)
	{
	  front_column = back_column = NULL;
	  in_comment = ((previously_in_comment = in_comment) ? (last_character_on_previous_line == '\\') : 0);
	  last_character_on_previous_line = 0;
	  while ((spool = datum = getc (source)) != line_break ? datum != EOF : 0)
	    {
	      in_comment = (in_comment ? 1 : (front_column ? 0 : (previously_in_comment ? (datum == '#') : 0)));
	      if (in_comment ? 0 : (textual ? 0 : !(textual=(front_branch ? (spool< 60 ? 1 : (spool=spool-60) & 0xffc0):1))))
		{
		  spoke = 6;
		  while (spoke)
		    avm_enqueue_branch (&front_branch, &back_branch, (spool >> (--spoke)) & 1);
		}
	      avm_enqueue (&front_column, &back_column, avm_character_representation (datum));
	      last_character_on_previous_line = datum;
	    }
	  avm_enqueue (&front_line, &back_line, front_column);
	}
    }
  if (front_branch ? 1 : textual)
    {
      preamble = NULL;
      avm_dispose (raw_data);
      contents = front_line;
    }
  else
    {
      if (!(preamble = initial_comments (front_line)))
	preamble = avm_copied (shared_cell);
      avm_dispose (front_line);
      contents = raw_data;
    }
  avm_dispose_branch_queue (front_branch);
  if (filename)
    {
      if (fclose (source))
	avm_non_fatal_io_error ("can't close", filename, errno);
    }
  return (avm_join (preamble, contents));
}





list
avm_load (source, filename, raw)
     FILE *source;
     char *filename;
     int raw;

     /* This is used for file input when the caller knows whether it
	should be raw or text. A fatal error results if something
	that's supposed to be data is text. */

{
  int datum;
  list front_line, back_line, front_column, back_column;

  if (!initialized)
    avm_initialize_formin ();
  front_line = (raw ? avm_received_list (source,(filename ? filename : "standard input")) : (back_line = NULL));
  if (!raw)
    {
      if ((datum = getc (source)) != EOF)
	{
	  front_column = back_column = NULL;
	  while (datum != line_break ? datum != EOF : 0)
	    {
	      avm_enqueue (&front_column, &back_column, avm_character_representation (datum));
	      datum = getc (source);
	    }
	  avm_enqueue (&front_line, &back_line, front_column);
	  while (datum == line_break)
	    {
	      front_column = back_column = NULL;
	      while ((datum = getc (source)) != line_break ? datum != EOF : 0)
		avm_enqueue (&front_column, &back_column,avm_character_representation (datum));
	      avm_enqueue (&front_line, &back_line, front_column);
	    }
	}
    }
  if (filename)
    if (fclose (source))
      avm_non_fatal_io_error ("can't close", filename, errno);
  return front_line;
}




void
avm_initialize_formin ()

     /* This creates some static variables. */

{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_branches ();
  avm_initialize_chrcodes ();
  shared_cell = avm_join (NULL, NULL);
}





void
avm_count_formin ()

     /* This frees some static variables. */

{
  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (shared_cell);
  shared_cell = NULL;
}
