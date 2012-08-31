
/* management of backward compatibility modes

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
#include <avm/vman.h>

/* increment this when creating a new version */
#define number_of_versions 3

/* an array of strings representing past and present version numbers */
static char *known_versions[number_of_versions];

/* an index into the array of known versions indicating which version is being emulated */
static int selection;

/* non-zero means a version has been selected */
static int selected = 0;

/* non-zero means static variables are initialized */
static int initialized = 0;




static void
initialize_vman ()

     /* This is used locally to initialize the table of known version numbers. */
{
  if (initialized)
    return;
  initialized = 1;
  selection = number_of_versions - 1;
  known_versions[0] = "0.0.0";
  known_versions[1] = "0.1.0";

  /* put the present version number above when creating a new one */

  known_versions[number_of_versions - 1] = VERSION;
}








void
avm_set_version (number)
     char *number;

     /* This is used for emulating earlier versions; the version set
        by this function can be interrogated by the one below. If this
        function is called, it must be called before any other
        function in this file.  */

{
  int found;

  if (selected)
    avm_error ("multiple version specifications");
  if (initialized)
    avm_internal_error (36);
  initialize_vman ();
  if (!number)
    avm_internal_error (37);
  selected = 1;
  found = selection = 0;
  while (found ? 0 : (selection < number_of_versions))
    if (!(found = !strcmp (known_versions[selection], number)))
      selection++;
  if (!found)
    {
      fprintf (stderr, "avram: can't emulate version %s\n", number);
      exit (EXIT_FAILURE);
    }
}








char *
avm_version ()

     /* This returns the version previously set by avm_set_version, or
        returns the current version by default if none has been
        set. */
{
  if (!initialized)
    initialize_vman ();
  return (known_versions[selection]);
}










int
avm_prior_to_version (number)
     char *number;

     /* This takes a version number in a string, which must be one of
        the known versions, and returns true if the version currently
        being emulated is earlier than the given one. */

{
  int matching_index;

  if (!initialized)
    initialize_vman ();
  if (!number)
    avm_internal_error (50);
  matching_index = 0;
  while ((matching_index < number_of_versions) ? strcmp (number,known_versions[matching_index]) : 0)
    matching_index++;
  if (matching_index >= number_of_versions)
    avm_internal_error (35);
  return (selection < matching_index);
}
