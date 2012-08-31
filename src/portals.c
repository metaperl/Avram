
/* memory management for portal and port pair types

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
#include <avm/ports.h>
#include <avm/portals.h>

/* a pointer to an internal cache of port pair structures used for
   fast allocation */
static portal available_portal = NULL;

/* the total number of allocated portals at any given time, excluding
   those in the cache */
static counter extant_portals = 0;

/* the number of portals in the cache */
static counter available_portals = 0;

/* non-zero implies static variable are initialized */
static int initialized = 0;

/* the limit to the number of portals allowed in the cache; can be
   changed arbitrarily for performance tuning */
#define portal_cache_size 0xff



portal
avm_new_portal (alters)
     portal alters;

     /* This allocates a new port pair node and initializes the alters
        field with the parameter, effectively inserting the new one
        into a list of them. It returns a NULL pointer if
        unsuccessful. The local cache is used for improved
        performance. */

{
  portal result;

  if (result = available_portal)
    {
      available_portal = available_portal->alters;
      available_portals--;
    }
  else
    result = (portal) (malloc (sizeof (*result)));
  if (result)
    {
      extant_portals++;
      memset (result, 0, sizeof (*result));
      result->alters = alters;
    }
  return (result);
}




void
avm_seal (fate)
     portal fate;

     /* This frees the memory associated with a portal, or leaves it
        in the cache for future use. It must be non-null or an
        internal error results. */
{
  extant_portals--;
  if (!fate)
    avm_internal_error (27);
  if (available_portals > portal_cache_size)
    free (fate);
  else
    {
      fate->alters = available_portal;
      available_portal = fate;
      available_portals++;
    }
}




void
avm_initialize_portals ()

     /* This initializes some static data structures in modules used
	by this one. */

{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_ports ();
}




void
avm_count_portals ()

     /* This frees up the cache and reports a memory leak if appropriate. */

{
  if (!initialized)
    return;
  initialized = 0;
  if (extant_portals)
    avm_reclamation_failure ("portals", extant_portals);
}
