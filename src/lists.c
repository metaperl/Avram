
/* this file contains some basic operations on lists

   Copyright (C) 2006-2010 Dennis Furey

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
#include <avm/lists.h>

/* a cache of reusable list nodes */
static list available_list = NULL;

/* the number of nodes in the cache */
static int available_lists = 0;

/* the maximum number allowed in the cache */
#define node_cache_size 0xff

/* the number of allocated lists excluding the cache */
static counter extant_lists = 0;

/* represents (nil,nil) */
static list shared_cell = NULL;

/* non-zero means static variables are initialized */
static int initialized = 0;


void
avm_dispose (front)
     list front;

     /* This performs space efficient storage reclamation; it avoids
        recursion and additional allocation by using the disposed list
        as its own queue. */

{
  list old_front, back;

  back = front;
  while (front)
    {
      if (front->sharers)
	{
	  (front->sharers)--;
	  front = NULL;
	}
      else
	{
	  while (front->head ? 1 : front->interpretation ? 1 : back->tail ? 1 : 0)
	    {
	      if (!(back->tail))
		{
		  if (front->head)
		    {
		      back->tail = front->head;
		      front->head = NULL;
		    }
		  else
		    {
		      back->tail = front->interpretation;
		      front->interpretation = NULL;
		    }
		}
	      else if (!(back->tail->sharers))
		back = back->tail;
	      else
		{
		  (back->tail->sharers)--;
		  back->tail = NULL;
		}
	    }
	  front = (old_front = front)->tail;
	  if (old_front->value)
	    {
	      free (old_front->value);
	      old_front->value = NULL;
	    }
	  if (old_front->facilitator)
	    {
	      old_front->facilitator->impetus = NULL;
	      old_front->facilitator = NULL;
	    }
	  extant_lists--;
	  if (available_lists > node_cache_size)
	    free (old_front);
	  else
	    {
	      old_front->tail = available_list;
	      available_list = old_front;
	      available_lists++;
	    }
	}
    }
}






list
avm_recoverable_join (left, right)
     list left;
     list right;

     /* This function implements the cons operation; this is where
        list nodes get allocated. A cache of nodes is maintained in
        available_lists so they can be quickly recycled without going
        through malloc(). Performance can be tuned by varying the size
        of the cache, which is in the constant node_cache_size,
        defined above. The number of allocated nodes outside of the
        cache is maintained in extant_lists, which is forced not to
        exceed a number that can be stored in a counter, thereby
        ensuring that the reference count field in a list node can
        never overflow. It could never happen anyway unless the size
        of a counter is smaller than that of a pointer. 

        A later addition to this function was to compute the optional
        known_weight field unconditionally so as to improve spacial
        locality. It isn't needed unless the weight of a list is
        computed, at which time it's assigned as a side effect if not
        already present. Compilation of large module with compression
        has a tendency to thrash if there are non-resident pages and
        weights aren't computed in advance. */

{
  list result;
  int lw,rw,w;

  if (!++extant_lists)		/* prevents reference count overflow */
    {
      extant_lists--;
      avm_dispose (left);
      avm_dispose (right);
      return NULL;
    }
  if (result = available_list)
    {
      available_list = available_list->tail;
      available_lists--;
    }
  else if (!(result = (list) (malloc (sizeof (*result)))))
    {
      avm_dispose (left);
      avm_dispose (right);
      return NULL;
    }
  memset (result, 0, sizeof (*result));
  result->head = left;
  result->tail = right;
  if ((left ? (lw = left->known_weight) : !(lw = 0)) ? (right ? (rw = right->known_weight) : !(rw = 0)) : 0)
    result->known_weight = ((lw ? 1 : rw) ? (((w = lw + rw + 1) > lw) ? ((w > rw) ? w : 0) : 0) : 0);
  return result;
}







list
avm_join (left, right)
     list left;
     list right;

     /* This creates a new list lst node with the given descendents or
	aborts if there isn't enough space. */

{
  list result;

  if (!(result = avm_recoverable_join (left, right)))
    avm_error ("memory overflow (code 11)");
  return (result);
}






inline list
avm_copied (operand)
     list operand;

     /* This returns a shared copy; reference count overflows are
        impossible because the number of extant list nodes is small
        enough to be enumerated by a counter. */
{
  if (operand)
    (operand->sharers)++;
  return operand;
}









void
avm_enqueue (front, back, operand)
     list *front;
     list *back;
     list operand;

     /* This can be used only to build a new unshared list; front and
        back should be initialized to NULL by the caller and not
        modified or copied until after the last item is enqueued. */

{
  if (*back ? (*back = (*back)->tail = avm_join (NULL, NULL)) : ((*front) = (*back) = avm_join (NULL, NULL)))
    (*back)->head = operand;
}






void
avm_recoverable_enqueue (front, back, operand, fault)
     list *front;
     list *back;
     list operand;
     int *fault;

     /* This is like enqueue, but blows away the operand instead of
        terminating in the event of a memory error. It's the
        responsibility of the caller to check after each call that the
        queue still exists, not just after the last call. */

{
  list new_item;

  if (*fault = (*fault ? 1 : !(new_item = avm_recoverable_join (NULL, NULL))))
    {
      avm_dispose (operand);
      avm_dispose (*front);
      *front = *back = NULL;
      return;
    }
  *back = (*back ? ((*back)->tail = new_item) : ((*front) = (*back) = new_item));
  (*back)->head = operand;
}





counter
avm_recoverable_length (operand)
     list operand;

     /* This returns the length of a list but returns zero if a
	counter overflows. */

{
  counter result;

  result = 0;
  while (operand)
    {
      if (!++result)
	return 0;
      operand = operand->tail;
    }
  return result;
}






counter
avm_length (operand)
     list operand;

     /* This returns the length of a list, but causes an error if it
        overflows. */

{
  counter result;

  result = avm_recoverable_length (operand);
  if (operand ? !result : result)
    avm_error ("counter overflow (code 1)");
  return result;
}








counter
avm_area (operand)
     list operand;

     /* This returns the sum of the lengths of a list of lists but
	aborts if a counter overflows. */

{
  counter result, new_result;

  result = 0;
  while (operand)
    {
      if ((new_result = result + avm_length (operand->head)) < result)
	avm_error ("counter overflow (code 2)");
      result = new_result;
      operand = operand->tail;
    }
  return result;
}






counter
avm_recoverable_area (operand, fault)
     list operand;
     int *fault;

     /* This returns sum of the lengths of a list of lists but sets
	the fault flag to 1 if a counter overflows. */

{
  counter result, new_result, head_length;

  result = *fault = 0;
  while (*fault ? 0 : operand)
    {
      if (! (*fault = (new_result = result + (head_length = avm_recoverable_length (operand->head))) < result))
	*fault = (operand->head ? !head_length : 0);
      result = new_result;
      operand = operand->tail;
    }
  return (*fault ? 0 : result);
}







list
avm_natural (number)
     counter number;

     /* This converts a counter to a list representation with list
        significant bit first. NULL represents a zero and shared_cell
        represents a 1 bit. It aborts if there isn't enough memory. */

{
  list front_bit, back_bit;

  front_bit = back_bit = NULL;
  while (number)
    {
      avm_enqueue (&front_bit, &back_bit,(number & 1) ? avm_copied (shared_cell) : NULL);
      number >>= 1;
    }
  return front_bit;
}






list
avm_recoverable_natural (number)
     counter number;

     /* This returns a list representation of an integer similar to
        avm_natural but returns NULL if there isn't enough memory. */

{
  list front_bit, back_bit;
  int fault;

  fault = 0;
  front_bit = back_bit = NULL;
  while (fault ? 0 : number)
    {
      avm_recoverable_enqueue (&front_bit, &back_bit, (number & 1) ? avm_copied (shared_cell) : NULL, &fault);
      number >>= 1;
    }
  return front_bit;
}





counter
avm_counter (number)
     list number;

     /* inverse of avm_natural; ignores overflow */
{
  counter result;
  list reversal,temporary;

  reversal = NULL;
  while (number)
    {
      temporary = number->tail;
      number->tail = reversal;
      reversal = number;
      number = temporary;
    }
  result = 0;
  temporary = reversal;
  while (reversal)
    {
      result <<= 1;
      if (reversal->head)
	result++;
      reversal = reversal->tail;
    }
  reversal = temporary;
  while (reversal)
    {
      temporary = reversal->tail;
      reversal->tail = number;
      number = reversal;
      reversal = temporary;
    }
  return result;
}








static void
plist (l)
     list l;

     /* This prints a list to standard output as a string of nested
	parentheses by recursively calling itself. */
{

  printf ("(");
  if (l)
    {
      plist (l->head);
      printf (",");
      plist (l->tail);
    }
  printf (")");
}







void
avm_print_list (operand)
     list operand;

     /* This is like plist but finishes with a newline. It's useful
        only for debugging; prints a list to standard output as a
        string of nested parentheses; there's no check for stack
        overflow */
{
  plist (operand);
  printf ("\n");
}





void
avm_initialize_lists ()

     /* This must be called before anything else in this file to
	initialize static data structures. */
{
  if (initialized)
    return;
  initialized = 1;
  if (!(shared_cell = (list) (malloc (sizeof (*shared_cell)))))
    avm_error ("memory overflow (code 12)");
  extant_lists++;
  if (!extant_lists)
    avm_internal_error (32);
  memset (shared_cell, 0, sizeof (*shared_cell));
}






void
avm_count_lists ()

     /* This frees static data structures and reports memory leaks. */
{
  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (shared_cell);
  shared_cell = NULL;
  if (extant_lists)
    avm_reclamation_failure ("lists", extant_lists);
}
