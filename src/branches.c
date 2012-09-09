
/* operations on pointers to lists and queues thereof

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

/* This constant determines how many branch queue nodes will be
   statically stored in order to save the time of allocating them. */
#define branch_cache_size 0xff

/* local cache of branch queue nodes */
static branch_queue available_branch = NULL;

/* number of branch queue nodes in the cache */
static int available_branches = 0;

/* total number of allocated branch queue nodes */
static counter extant_branches = 0;

/* Non-zero means static variables have been initialized. */
static int initialized = 0;



void
avm_recoverable_anticipate (front, back, operand, fault)
     branch_queue *front;
     branch_queue *back;
     branch operand;
     int *fault;

     /* This appends an operand to the branch queue addressed by front
        and back. Both of these should be initialized to NULL before
        the first call, and then passed to this function again for
        each subsequent call. Thanks to Norm Pleszkoch for spotting a
        bug in an early version of this routine. */
{
  if (*fault)
    {
      avm_dispose_branch_queue (*front);
      *front = *back = NULL;
      return;
    }
  if (!*front)
    {
      if (*front = *back = available_branch)
	{
	  available_branch = available_branch->following;
	  available_branches--;
	}
      else
	*fault = ! (*front = *back = (branch_queue) (malloc (sizeof (**back))));
    }
  else if (!*back)
    avm_internal_error (34);
  else if (available_branch)
    {
      *back = (*back)->following = available_branch;
      available_branch = available_branch->following;
      available_branches--;
    }
  else
    (*fault = ! (*back = (*back)->following = (branch_queue) (malloc (sizeof (**back)))));
  if (*fault)
    {
      avm_dispose_branch_queue (*front);
      *front = *back = NULL;
      return;
    }
  extant_branches++;
  (*back)->following = NULL;
  (*back)->above = operand;
}








void
avm_anticipate (front, back, operand)
     branch_queue *front;
     branch_queue *back;
     branch operand;

     /* This is similar to the recoverable version but incorporates
        error handling; this is the entry point used by avram. */

{
  int fault;

  fault = 0;
  avm_recoverable_anticipate (front, back, operand, &fault);
  if (fault)
    avm_error ("memory overflow (code 2)");
}









void
avm_enqueue_branch (front, back, received_bit)
     branch_queue *front;
     branch_queue *back;
     int received_bit;

     /* This gets used in several places when building a tree from an
        input string. Every 0 bit corresponds to an empty subtree, and
        every 1 bit corresponds to a non-empty subtree. Hence,
        whenever a 1 bit is read, a new cell is created and pointers
        to its head and tail are enqueued for later reading. */

{
  branch_queue old;

  if (!*front)
    return;
  if (*((*front)->above) = (received_bit ? avm_join (NULL, NULL) : NULL))
    {
      avm_anticipate (front, back, &((*((*front)->above))->head));
      avm_anticipate (front, back, &((*((*front)->above))->tail));
    }
  *front = (old = *front)->following;
  avm_dispose_branch (old);
}









void
avm_recoverable_enqueue_branch (front, back, received_bit, fault)
     branch_queue *front;
     branch_queue *back;
     int received_bit;
     int *fault;

     /* This is similar to the above, but allows clients to do their
        own error handling. */

{
  branch_queue old;
  list new_cell;

  if (*fault = (*fault ? 1 : !(*front)))
    return;
  new_cell = (received_bit ? avm_recoverable_join (NULL, NULL) : NULL);
  if (*fault = (received_bit ? !new_cell : 0))
    return;
  if ((*((*front)->above) = new_cell))
    {
      avm_recoverable_anticipate (front, back, &((*((*front)->above))->head), fault);
      if (!*fault)
	avm_recoverable_anticipate (front, back, &((*((*front)->above))->tail), fault);
    }
  if (!*fault)
    {
      *front = (old = *front)->following;
      avm_dispose_branch (old);
    }
}












void
avm_dispose_branch (old)
     branch_queue old;

     /* This frees a branch with caching. */

{

  if (old)
    {
      extant_branches--;
      if (available_branches > branch_cache_size)
	free (old);
      else
	{
	  available_branches++;
	  old->following = available_branch;
	  available_branch = old;
	}
    }
}








void
avm_dispose_branch_queue (front)
     branch_queue front;

     /* This frees a whole branch queue with caching; the lists that
        the branches point to are not touched. */

{
  branch_queue old;

  while (front)
    {
      front = (old = front)->following;
      avm_dispose_branch (old);
    }
}








void
avm_initialize_branches ()

     /* This is to be called before anything else; presently it
        doesn't do much. */

{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
}








void
avm_count_branches ()

     /* This detects and reports memory leaks. */

{
  if (initialized)
    {
      initialized = 0;
      if (extant_branches)
	avm_reclamation_failure ("branches", extant_branches);
    }
}
