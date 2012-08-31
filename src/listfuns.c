
/* this file contains some more complicated operations on lists

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
#include <avm/chrcodes.h>
#include <avm/compare.h>
#include <avm/listfuns.h>


/* non-zero means static variables are initialized */
static int initialized = 0;

/* represents (nil,nil) */
static list shared_cell = NULL;

/* error messages as lists of lists of character representations */
static list empty_size = NULL;
static list invalid_value = NULL;
static list missing_value = NULL;
static list memory_overflow = NULL;
static list counter_overflow = NULL;
static list invalid_transpose = NULL;
static list invalid_membership = NULL;
static list invalid_distribution = NULL;
static list invalid_concatenation = NULL;




void
*avm_value_of_list (operand, message, fault)
     list operand;
     list *message;
     int *fault;

     /* This takes a list representing a value used by a library
        function and returns a pointer to the value. The value field
        in such a list will normally point to the block of memory
        holding the value, and the list itself will be a list of
        character representations whose binary encodings spell out the
        value. The redundancy is deliberate because it allows a list
        representing a value to be written out to a file in the usual
        avm format without any loss of information. */

{
  char *temporary;
  void *result;
  int datum;
  counter size;
  list root;

  if (*fault = (*fault ? 1 : !!(*message)))
    return NULL;
  if (*fault = !operand)
    {
      *message = avm_copied (missing_value);
      return NULL;
    }
  if (operand->value)
    return operand->value;
  if (*fault = ! (result = (void *) malloc (size = avm_length (operand))))
    {
      *message = avm_copied (memory_overflow);
      return NULL;
    }
  operand->value = result;
  temporary = (char *) result;
  root = operand;
  while (*fault ? NULL : operand)
    if (*fault = (datum = avm_standard_character_code (operand->head)) < 0)
      *message = avm_copied (invalid_value);
    else
      {
	if (!size--)
	  avm_internal_error(60);
	*temporary++ = datum;
	operand = operand->tail;
      }
  if (!*fault)
    return result;
  free (root->value);
  root->value = NULL;
  return NULL;
}







list
avm_list_of_value (contents, size, fault)
     void *contents;
     size_t size;
     int *fault;

     /* inverse of value_of_list, takes the address and the size of
	the value to a list, making a copy of the contents rather
        than relying on the original */
{
  list front,back,entry;
  char *temporary;
  void *result;

  if (*fault)
    return NULL;
  if(*fault = !size)
    return avm_copied (empty_size);
  if(*fault = !(result = (void *) malloc (size)))
    return avm_copied (memory_overflow);
  front = back = NULL;
  memcpy (result, contents, size);
  temporary = (char *) contents;
  while (*fault ? 0 : size)
    {
      entry = avm_standard_character_representation (*temporary++);
      avm_recoverable_enqueue (&front, &back, entry, fault);
      size--;
    }
  if (*fault)
    {
      avm_dispose (front);
      free (result);
      front = avm_copied (memory_overflow);
    }
  else
    front->value = result;
  return front;
}















list
avm_reversal (operand, fault)
     list operand;
     int *fault;

     /* This returns a copy of the reversal of a list. */

{
  list result;

  *fault = 0;
  if (!operand ? 1 : !(operand->tail))
    return avm_copied (operand);
  result = NULL;
  while (*fault ? 0 : operand)
    {
      *fault = !(result = avm_recoverable_join (avm_copied (operand->head), result));
      operand = operand->tail;
    }
  if (*fault)
    return avm_copied (memory_overflow);
  return result;
}







list
avm_distribution (operand, fault)
     list operand;
     int *fault;

     /* This creates a list in which every item is a pair with the
	head of the original operand on the left and the corresponding
	member of the tail of the original operand on the right. */

{
  list left, right, front, back;

  if (*fault = !operand)
    return (avm_copied (invalid_distribution));
  left = operand->head;
  right = operand->tail;
  front = back = (right ? avm_recoverable_join(NULL, NULL) : NULL);
  if (right ? !(*fault = !back) : 0)
    {
      front->known_weight = 0;
      *fault = !(back->head = avm_recoverable_join (avm_copied (left),avm_copied (right->head)));
      right = right->tail;
    }
  while (*fault ? 0 : right)
    {
      if (! (*fault = !(back = back->tail = avm_recoverable_join (NULL, NULL))))
	*fault = !(back->head = avm_recoverable_join (avm_copied (left),avm_copied (right->head)));
      right = right->tail;
    }
  if (*fault)
    {
      avm_dispose (front);
      return avm_copied (memory_overflow);
    }
  return front;
}








list
avm_concatenation (operand, fault)
     list operand;
     int *fault;

     /* This makes a list with the head of the operand concatenated to
	the tail. */

{
  list left, front, back;

  if (*fault = !operand)
    return avm_copied (invalid_concatenation);
  if (!(operand->tail))
    return avm_copied(operand->head);
  if (!(left = operand->head))
    return avm_copied(operand->tail);
  if (! (*fault = !(front = back = avm_recoverable_join (NULL, NULL))))
    {
      back->head = avm_copied (left->head);
      left = left->tail;
    }
  while (left ? !(*fault = !back) : 0)
    {
      if (back = back->tail = avm_recoverable_join (NULL, NULL))
	back->head = avm_copied (left->head);
      left = left->tail;
    }
  if (!(*fault = !back))
    back->tail = avm_copied (operand->tail);
  if (*fault)
    {
      avm_dispose (front);
      return avm_copied (memory_overflow);
    }
  return front;
}








list
avm_flattened (operand, fault)
     list operand;
     int *fault;

     /* equivalent to reduce(cat,nil) in Ursala notation */
{
  list front,back,item;

  front = back = NULL;
  while (*fault ? NULL : operand)
    {
      item = operand->head;
      while (*fault ? NULL : item)
	{
	  avm_recoverable_enqueue (&front, &back, avm_copied (item->head), fault);
	  item = item->tail;
	}
      operand = operand->tail;
    }
  return front;
}











list
avm_transposition (operand, fault)
     list operand;
     int *fault;

    /* This requires the operand to represent a list of equal length
       lists. It returns the list of lists in which the first item is
       the list of all first items in the operand, the second item is
       the list of all second items, and so on. The operand is
       disposed of. */

{
  list old, front_head, back_head, front_tail, back_tail, front, back;

#define queue(f,b,x)							                                  \
  if(!*fault)								                                  \
    {									                                  \
      if((*fault=!(b?(b=b->tail=avm_recoverable_join(NULL,NULL)):(f=b=avm_recoverable_join(NULL,NULL))))) \
	{								                                  \
	  avm_dispose(f);	                                         		        	  \
	  f = avm_copied(memory_overflow);				                                  \
	}								                                  \
      else								                                  \
	b->head = avm_copied(x);					                                  \
    }
  
  *fault = 0;
  front = back = NULL;
  while (operand ? (!!(operand->head) ? !*fault : 0) : 0)
    {
      front_head = back_head = front_tail = back_tail = NULL;
      while (*fault ? 0 : operand)
	{
	  queue (front_head, back_head, operand->head->head);
	  queue (front_tail, back_tail, operand->head->tail);
	  operand = avm_copied ((old = operand)->tail);
	  avm_dispose (old);
	  if (!operand ? 0 : *fault ? 0 : (*fault = !(operand->head)))
	    {
	      avm_dispose (front);
	      front = avm_copied (invalid_transpose);
	    }
	}
      queue (front, back, front_head);
      avm_dispose (front_head);
      operand = front_tail;
    }
  while (operand)
    {
      if (*fault ? 0 : (*fault = !!(operand->head)))
	{
	  avm_dispose (front);
	  front = avm_copied (invalid_transpose);
	}
      operand = avm_copied ((old = operand)->tail);
      avm_dispose (old);
    }
  return front;
}







list
avm_binary_membership (operand, members, fault)
     list operand;
     list members;
     int *fault;

     /* This computes the membership predicate; returns NULL if the
        operand isn't anywhere in the members, but returns
        shared_cell if it is. */
{
  list message;

  message = NULL;
  while (*fault ? 0 : (message ? 0 : !!members))
    {
      message = avm_binary_comparison (operand, members->head, fault);
      members = members->tail;
    }
  return message;
}








list
avm_membership (operand, fault)
     list operand;
     int *fault;

     /* This computes the membership predicate; returns NULL if the
        head isn't anywhere in the tail of the operand, but returns
        shared_cell if it is. The operand must be non-empty or an
        error message is returned. */
{
  if (*fault = !operand)
    return avm_copied (invalid_membership);
  return avm_binary_membership (operand->head, operand->tail, fault);
}







list
avm_position (key, table, fault)
     list key;
     list table;
     int *fault;

     /* This takes a key and list whose items are possible keys, and
        returns position the corresponding item as a character
        encoding if any; otherwise returns NULL. */

{
  int found;
  int position;
  list message;

  message = NULL;
  found = position = 0;
  while (*fault ? 0 : (found ? 0 : !!table))
    {
      found = (*fault ? 0 : !!(message = avm_binary_comparison (key, table->head, fault)));
      position++;
      table = table->tail;
    }
  if(found)
    {
      avm_dispose (message);
      message = avm_character_representation (position);
    }
  return message;
}








list
avm_measurement (operand, fault)

/* This returns the number of cells in a list as a binary number
   represented by a list of bits lsb first, with shared_cell for 1 and
   NULL for 0; also assigns the known_weight fields in all cells
   visited for future reference. The algorithm works without recursion
   by building a stack, starting out with just the operand on it. Then
   the following operations are perfomed until the stack has only a
   single item on it with a known weight, which is the answer. An
   unknown weight in the top item causes its head and tail to be
   pushed. A known weight on the top and an unknown weight on the one
   below causes the top and the one below to be interchanged. Known
   weights on both cause them to be added and popped, with the
   successor of the total assigned to the one below them. There could
   be an overflow if the weight is too big to fit in a counter type
   (probably 64 bits). Even though a list can't have more cells than
   that, it could appear to have more due to shared subtrees. In the
   event of overflow, an exception is thrown. */

     list operand;
     int *fault;
{
  counter count;
  list temporary, stack, result;

  if (*fault = !(stack = avm_recoverable_join (avm_copied (operand), NULL)))
    return avm_copied (memory_overflow);
  while (stack)
    {
      if (stack->head)
	{
	  if (stack->head->known_weight)
	    {
	      if (stack->tail)
		{
		  if (stack->tail->head)
		    {
		      if (count = stack->tail->head->known_weight)
			{
			  *fault = ((stack->tail->tail->head->known_weight = 1+count+stack->head->known_weight) <= count);
			  if (*fault)
			    {
			      stack->tail->tail->head->known_weight = 0;
			      avm_dispose (stack);
			      return (avm_copied (counter_overflow));
			    }
			  else
			    {
			      stack = avm_copied ((temporary = stack)->tail->tail);
			      avm_dispose (temporary);
			    }
			}
		      else
			{
			  temporary = stack->tail->head;
			  stack->tail->head = stack->head;
			  stack->head = temporary;
			}
		    }
		  else if (*fault = !(stack->tail->tail->head->known_weight = stack->head->known_weight + 1))
		    {
		      stack->tail->tail->head->known_weight = 0;
		      avm_dispose (stack);
		      return (avm_copied (counter_overflow));
		    }
		  else
		    {
		      stack = avm_copied ((temporary = stack)->tail->tail);
		      avm_dispose (temporary);
		    }
		}
	      else
		{
		  count = stack->head->known_weight;
		  avm_dispose (stack);
		  stack = NULL;
		}
	    }
	  else
	    {
	      temporary = avm_copied(stack->head->head);
	      stack = avm_recoverable_join(temporary, avm_recoverable_join (avm_copied(stack->head->tail),stack));
	      if (*fault = !stack)
		return (avm_copied (memory_overflow));
	    }
	}
      else if (stack->tail)
	{
	  if (stack->tail->head)
	    {
	      if (count = stack->tail->head->known_weight)
		{
		  if (*fault = ((stack->tail->tail->head->known_weight = 1 + count) <= count))
		    {
		      stack->tail->tail->head->known_weight = 0;
		      avm_dispose (stack);
		      return (avm_copied (counter_overflow));
		    }
		  else
		    {
		      stack = avm_copied ((temporary = stack)->tail->tail);
		      avm_dispose (temporary);
		    }
		}
	      else
		{
		  temporary = stack->tail->head;
		  stack->tail->head = stack->head;
		  stack->head = temporary;
		}
	    }
	  else
	    {
	      stack->tail->tail->head->known_weight = 1;
	      stack = avm_copied ((temporary = stack)->tail->tail);
	      avm_dispose (temporary);
	    }
	}
      else
	{
	  count = 0;
	  avm_dispose (stack);
	  stack = NULL;
	}
    }
  while (count)
    {
      if (*fault = !(stack = avm_recoverable_join ((count & 1) ? avm_copied (shared_cell) : NULL, stack)))
	return (avm_copied (memory_overflow));
      count >>= 1;
    }
  result = NULL;
  while (stack)
    {
      stack = (temporary = stack)->tail;
      temporary->tail = result;
      result = temporary;
    }
  return result;
}













void
avm_initialize_listfuns ()

     /* This initializes some static data structures. */

{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_chrcodes ();
  avm_initialize_compare ();
  shared_cell = avm_join (NULL, NULL);
  empty_size = avm_join (avm_strung ("empty size"), NULL);
  missing_value = avm_join (avm_strung ("missing value"), NULL);
  invalid_value = avm_join (avm_strung ("invalid value"), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  counter_overflow = avm_join (avm_strung ("counter overflow"), NULL);
  invalid_transpose = avm_join (avm_strung ("invalid transpose"), NULL);
  invalid_membership = avm_join (avm_strung ("invalid membership"), NULL);
  invalid_distribution = avm_join (avm_strung ("invalid distribution"), NULL);
  invalid_concatenation = avm_join (avm_strung ("invalid concatenation"), NULL);
}




void
avm_count_listfuns ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (empty_size);
  avm_dispose (shared_cell);
  avm_dispose (invalid_value);
  avm_dispose (missing_value);
  avm_dispose (memory_overflow);
  avm_dispose (counter_overflow);
  avm_dispose (invalid_transpose);
  avm_dispose (invalid_membership);
  avm_dispose (invalid_distribution);
  avm_dispose (invalid_concatenation);
  empty_size = NULL;
  shared_cell = NULL;
  missing_value = NULL;
  invalid_value = NULL;
  memory_overflow = NULL;
  counter_overflow = NULL;
  invalid_transpose = NULL;
  invalid_membership = NULL;
  invalid_distribution = NULL;
  invalid_concatenation = NULL;
}


