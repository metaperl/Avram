
/* memory management for ports and packets

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
#include <avm/ports.h>


/* a pointer to a fast local cache of reusable packets */
static port available_port = NULL;

/* the number of packets currently in the cache */
static counter available_ports = 0;

/* the total number of allocated packets excluding those in the cache */
static counter extant_ports = 0;

/* a non-zero value means static variables have been initialized */
static int initialized = 0;

/* the maximum number of packets allowed in the cache simultaneously */
#define port_cache_size 0xff




port
avm_newport (errors, parent, predicating)
     counter errors;
     port parent;
     int predicating;

     /* This attempts to create storage for a new port, initializing
        the given fields as shown if successful, and returning
        @code{NULL} otherwise. It interacts with the cache for better
        performance. All uninitialized fields are filled with
        zeros. */
{
  port result;

  if (result = available_port)
    {
      available_port = available_port->parent;
      available_ports--;
    }
  else
    result = (port) (malloc (sizeof (*result)));
  if (result)
    {
      extant_ports++;
      memset (result, 0, sizeof (*result));
      result->predicating = predicating;
      result->errors = errors;
      result->parent = parent;
    }
  return result;
}





void
avm_sever (appendage)
     port appendage;

     /* This frees the memory associated with a given port, or stores
	it in the cache if possible. */

{

  extant_ports--;
  if (!appendage)
    avm_internal_error (28);
  if (available_ports > port_cache_size)
    free (appendage);
  else
    {
      appendage->parent = available_port;
      available_port = appendage;
      available_ports++;
    }
}





void
avm_initialize_ports ()

     /* This initializes some local static data structures in the
	lists module if they haven't been done already. */

{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
}




void
avm_count_ports ()

     /* This detects and reports memory leaks by unreclaimed ports. */

{
  if (!initialized)
    return;
  initialized = 0;
  if (extant_ports)
    avm_reclamation_failure ("ports", extant_ports);
}
