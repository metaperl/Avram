
/* conversion between path names as character strings and as lists

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
#include <avm/fnames.h>
#include <time.h>

/* File names aren't expected to be longer than 256 characters, so an
   error is reported if they are. If you need them longer, by all
   means change this. There's no memory cost in doing so. */
#define plausible_filename_length 0xff


/* a list representation of the string "unknown date", which is
   returned when avm_date_representation is at a loss */
static list unknown_date;

/* non-zero implies static variables are initialized */
static int initialized;



list
avm_path_representation (path)
     char *path;

     /* This maps a path stored in a character string to the
        equivalent representation in a list. Each item in the list is
        a single component of the path, represented as a list of
        character representations. The file name is first and the
        parent directory names come next. The root directory is
        represented by an empty component. Standard input and standard
        output are represented by empty lists. */

{
  list front, back, result;

  if(!path)
    return NULL;
  result = front = back = NULL;
  while (*path)
    {
      if (*path != avm_path_separator_character)
	avm_enqueue (&front, &back, avm_character_representation (*path));
      else if(*(path+1))
	{
	  result = avm_join (front, result);
	  front = back = NULL;
	}
      path++;
    }
  return avm_join (front, result);
}






list
avm_date_representation (path)
     char *path;

     /* This returns the time stamp as a list. */

{
  list front, back;
  char *temporary;
  struct stat buffer;
  char time_buffer[26];

  if (path ? stat (path, &buffer) : 1)
    return (avm_copied (unknown_date));
  else
    {
      front = back = NULL;
#if HAVE_CTIME_R
      ctime_r (&buffer.st_mtime, time_buffer);
      temporary = time_buffer;
#else /* not HAVE_CTIME_R */
      temporary = ctime (&buffer.st_mtime);
#endif /* HAVE_CTIME_R */
      while (temporary && (*temporary) && (*temporary != '\n'))
	{
	  avm_enqueue (&front, &back, avm_character_representation (*temporary));
	  temporary++;
	}
      return front;
    }
}






char *
avm_path_name (path)
     list path;

     /* This is the inverse of avm_path_representation. */

{
  list name;
  int erroneous;
  int datum;
  char *result;
  char *temporary;
  counter cumulative_length, total_length, name_length, new_length;

  total_length = (name_length = avm_area (path)) + avm_length (path);
  if (total_length < name_length)
    avm_error ("counter overflow (code 3)");
  if (total_length == 0)
    return (NULL);
  if (total_length == 1 ? 1 : total_length > plausible_filename_length)
    avm_error ("invalid file name (code 1)");
  if (!(temporary = (char *) malloc (total_length)) ? 1 : !(result = (char *) malloc (total_length)))
    avm_error ("memory overflow (code 10)");
  *result = 0;
  cumulative_length = 1;
  while (path)
    {
      name_length = 0;
      name = path->head;
      while (name)
	{
	  if ((datum = avm_character_code (name->head)) < 0)
	    avm_error ("invalid file name (code 2)");
	  else if ((datum<32) | (datum>126) | (datum=='/') | (datum=='\\'))
	    avm_error ("bad character in file name");
	  else if (name_length >= total_length - 1)
	    avm_internal_error (21);
	  else if (!(temporary[name_length++] = datum))
	    avm_error ("null character in file name");
	  name = name->tail;
	}
      temporary[name_length] = 0;
      if (*result)
	{
	  if ((++name_length) == total_length)
	    avm_internal_error (22);
	  strcat (temporary, "/");
	}
      erroneous = (new_length = cumulative_length + name_length) < cumulative_length;
      erroneous = erroneous ? 1 : ((cumulative_length = new_length) > total_length);
      if (erroneous)
	avm_internal_error (23);
      strcat (temporary, result);
      strcpy (result, temporary);
      path = path->tail;
    }
  free (temporary);
  if ((*result) == 0)
    avm_error ("invalid file name (code 3)");
  return result;
}





void
avm_initialize_fnames ()

     /* This initializes some static data structures. */

{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_branches ();
  avm_initialize_chrcodes ();
  unknown_date = avm_strung ("unknown date");
}







void
avm_count_fnames ()

     /* This frees some static data structures to help detect memory
	leaks. */
{
  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (unknown_date);
  unknown_date = NULL;
}
