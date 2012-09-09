
/* this file contains functions supporting list deconstruction

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
#include <avm/decons.h>


/* A stack of these is used to avoid recursion. Each item on the stack
   has the pointer, the operand, and the address where the result will
   be put after it is computed. The address will be in another item
   somwhere below it in the stack. */

  struct point_node
  {
    list pointer;
    list operand;
    branch target;
    struct point_node *dependents;
  };

typedef struct point_node *point;


static list memory_overflow;	/* error messages */
static list invalid_deconstruction;

/* non-zero means static variables have been initialized */
static int initialized = 0;

/* a cache of unused point nodes */
static point available_point = NULL;

/* the number of point_nodes in the cache */
static int available_points = 0;

/* the maximum number of point nodes allowed in the cache */
# define point_cache_size 0xf

/* the total number of allocated point nodes excluding the cache */
static counter extant_points = 0;




static int
given (pointer, operand, target, points)
     list pointer;
     list operand;
     branch target;
     point *points;

     /* This pushes a point onto the stack. */
{
  int success;
  point result;

  if (success = !!(result = available_point))
    {
      available_point = available_point->dependents;
      available_points--;
    }
  else
    success = !!(result = (point) (malloc (sizeof (*result))));
  if (success)
    {
      extant_points++;
      result->pointer = pointer;
      result->operand = operand;
      result->target = target;
      result->dependents = *points;
      *points = result;
    }
  return success;
}




static int
taken (pointer, operand, target, points)
     list *pointer;
     list *operand;
     branch *target;
     point *points;

     /* This pops a point from the stack. */
{
  point old_point;

  if (!*points)
    return 0;
  extant_points--;
  *pointer = (*points)->pointer;
  *operand = (*points)->operand;
  *target = (*points)->target;
  *points = (old_point = *points)->dependents;
  if (available_points > point_cache_size)
    free (old_point);
  else
    {
      old_point->dependents = available_point;
      available_point = old_point;
      available_points++;
    }
  return 1;
}





list
avm_deconstruction (pointer, operand, fault)
     list pointer;
     list operand;
     int *fault;

     /* The pointer and operand are first pushed onto the stack.
        While the stack is non-empty, the top item is examined and the
        following operations performed. If the top pointer is a single
        cell, the operand is copied to the target. If the top pointer
        has an empty head, the tails of the pointer and the operand
        are pushed. If the top pointer has an empty tail, both heads
        are pushed. If the pointer has both a non-empty head and an
        non-empty tail, each of them is pushed with a copy of the
        operand. In the first two cases, the same target can be used
        for the newly pushed item, but in the last, a new cell has to
        be created and the addresses of its head and tail used. */

{
  int done;
  list result;
  int overflow;
  branch target;
  point points;

  points = NULL;
  result = NULL;
  target = &result;
  *fault = done = overflow = 0;
  if (!pointer)
    avm_internal_error (19);
  do
    {
      while ((!(pointer->head) ^ !(pointer->tail)) ? !(*fault = !operand) : 0)
	{
	  if (pointer->head)
	    {
	      operand = operand->head;
	      pointer = pointer->head;
	    }
	  else
	    {
	      operand = operand->tail;
	      pointer = pointer->tail;
	    }
	}
      if (*fault ? 0 : (pointer->head ? 0 : !(pointer->tail)))
	{
	  *target = avm_copied (operand);
	  done = !taken (&pointer, &operand, &target, &points);
	}
      else if (!*fault)
	{
	  *target = avm_recoverable_join (NULL,NULL);
	  if (!(overflow = !(*target ? given (pointer->head,operand,&((*target)->head),&points) : 0)))
	    {
	      target = &((*target)->tail);
	      pointer = pointer->tail;
	    }
	}
    }
  while (done ? 0 : overflow ? 0 : !*fault);
  while (taken (&pointer, &operand, &target, &points));
  if (*fault)
    {
      avm_dispose (result);
      return avm_copied (invalid_deconstruction);
    }
  if (*fault = overflow)
    {
      avm_dispose (result);
      return avm_copied (memory_overflow);
    }
  return result;
}





void
avm_initialize_decons ()
     /* This initializes some static data. */
{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_branches ();
  avm_initialize_chrcodes ();
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  invalid_deconstruction =
    avm_join (avm_strung ("invalid deconstruction"), NULL);
}






void
avm_count_decons ()
     /* This frees some static data and reports memory leaks. */
{
  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (memory_overflow);
  avm_dispose (invalid_deconstruction);
  memory_overflow = NULL;
  invalid_deconstruction = NULL;
  if (extant_points)
    avm_reclamation_failure ("points", extant_points);
}
