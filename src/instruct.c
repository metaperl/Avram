/* memory management and stack operations on instruction nodes

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
#include <avm/portals.h>
#include <avm/instruct.h>

/* non-zero implies that static variables have been initialized */
static int initialized = 0;

/* a local cache of instruction nodes stored for fast reuse */
static instruction available_instruction = NULL;

/* the number of instruction nodes in the cache */
static int available_instructions = 0;

/* the total number of allocated instruction nodes excluding the cache */
static counter extant_instructions = 0;

/* the maximum number of instruction nodes allowed in the cache */
#define instruction_cache_size 0xff



void
avm_retire (done)
     instruction *done;

     /* This pops an instruction node off a stack threaded through the
        dependents field. The instruction referenced by the argument
        will be reassigned to the dependents field of the instruction
        node that was popped. The node will be either freed or stored
        in a cache for later reuse. It is an internal error if the
        instruction is NULL. */

{
  instruction old;

  extant_instructions--;
  if ((!done) ? 1 : !*done)
      avm_internal_error (25);
  *done = (old = *done)->dependents;
  avm_dispose (old->datum.contents);
  avm_dispose (old->actor.contents);
  if (old->actor.impetus)
    {
      old->actor.impetus->facilitator = NULL;
      old->actor.impetus = NULL;
    }
  if (available_instructions > instruction_cache_size)
    free (old);
  else
    {
      old->dependents = available_instruction;
      available_instruction = old;
      available_instructions++;
    }
}





int
avm_scheduled (actor_contents, datum_errors, datum_contents, client, next, sheet, remote, balanceable, granularity)
     list actor_contents;
     counter datum_errors;
     list datum_contents;
     port client;
     instruction *next;
     score sheet;
     flag remote;
     flag balanceable;
     counter granularity;

     /* This attempts to create storage for a new instruction node or
        grabs one out of the cache, and initializes the fields with
        the given parameters. Other fields are filled with zeros. If
        allocation fails, a NULL pointer is returned. */
{
  instruction result;

  if (result = available_instruction)
    {
      available_instruction = available_instruction->dependents;
      available_instructions--;
    }
  else
    result = (instruction) (malloc (sizeof (*result)));
  if (result)
    {
      extant_instructions++;
      memset (result, 0, sizeof (*result));
      result->actor.contents = avm_copied (actor_contents);
      result->datum.errors = datum_errors;
      result->datum.contents = avm_copied (datum_contents);
      result->sheet = sheet;
      result->remotely_executable = remote;
      result->non_deterministic = balanceable;
      result->granularity = granularity;
      result->client = client;
      result->dependents = *next;
      *next = result;
    }
  return (!!result);
}




void
avm_reschedule (next)
     instruction *next;

     /* This interchanges the positions of two instructions on top of
        a stack of them, which must have at least two nodes in it or
        it's an internal error. */

{
  instruction temporary;

  if (!*next ? 1 : !((*next)->dependents))
    avm_internal_error (26);
  temporary = *next;
  *next = temporary->dependents;
  temporary->dependents = (*next)->dependents;
  (*next)->dependents = temporary;
}





void
avm_initialize_instruct ()

     /* This initializes some static variables. */
{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_ports ();
  avm_initialize_profile ();
  avm_initialize_portals ();
}






void
avm_count_instruct ()

     /* This frees some static variables and detects and reports
	memory leaks. */
{
  if (!initialized)
    return;
  initialized = 0;
  if (extant_instructions)
    avm_reclamation_failure ("instructions", extant_instructions);
}
