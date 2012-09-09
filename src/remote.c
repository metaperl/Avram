
/* pertaining to execution of virtual machine code on remote servers

   Copyright (C) 2010 Dennis Furey

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

#include <netdb.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <avm/chrcodes.h>
#include <avm/common.h>
#include <avm/error.h>
#include <avm/apply.h>
#include <avm/branches.h>
#include <avm/rawio.h>
#include <avm/compare.h>
#include <avm/listfuns.h>
#include <avm/servlist.h>
#include <avm/farms.h>
#include <avm/jobs.h>
#include <avm/vglue.h>
#include <avm/remote.h>

/* Non-zero means static variables have been initialized. */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list memory_overflow = NULL;

/* virtual code for the function that turns a two item list into a pair */
static list listopair = NULL;

/* virtual code for the function that takes the head of a list */
static list listhead = NULL;

/* used for constructing virtual code */
static list shared_cell = NULL;










int
avm_remotely_constructed (left_side, right_side, operand, result, fault)
     list left_side;
     list right_side;
     list operand;
     list *result;
     int *fault;

     /* this implements concurrent evaluation of functions of the form couple(f,g) */
{
  job front,back,top;

  if (!initialized)
    avm_initialize_remote ();
  *result = NULL;
  if (*fault ? 1 : avm_offline ())
    return 0;
  front = back = top = NULL;
  avm_new_job (&front, &back, avm_compose (avm_copied (left_side), avm_copied (listhead), fault), NULL, NULL, 1, fault);
  if (! (*fault = (*fault ? 1 : !back)))
    avm_new_job (&(back->prerequisites), NULL, avm_copied (operand), back, NULL, 0, fault);
  avm_new_job (&front, &back, avm_compose (avm_copied (right_side), avm_copied (listhead), fault), NULL, NULL, 1, fault);
  if (! (*fault = (*fault ? 1 : !back)))
    avm_new_job (&(back->prerequisites), NULL, avm_copied (operand), back, NULL, 0, fault);
  avm_new_job (&top,  NULL,  avm_copied (listopair), NULL, front, 2, fault);
  *result = (*fault ? avm_copied (memory_overflow) : avm_evaluation (top, 0, fault));
  return 1;
}









static counter
balanced (operand_length, granularity)
     counter operand_length;
     counter granularity;

     /* finds the maximum size for nearly equal blocks not exceeding
	the granularity */
{
  counter block_size, blocks;

  if (!granularity)
    return 0;
  if ((blocks = operand_length / granularity) * granularity < operand_length)
    blocks++;
  block_size = operand_length / blocks;
  while (block_size * blocks < operand_length)
    block_size++;
  return block_size;
}










int
avm_remotely_mapped (operator, operand, result, granularity, fault)
     list operator;
     list operand;
     list *result;
     counter granularity;
     int *fault;

     /* This gets called by the main universal function, apply, when
        it determines that distributed evaluation is indicated for a
        function of the form map(operator) (in Ursala notation). If a
        distributed evaluation can't be performed due to being off
        line, this function returns zero and the caller performs the
        evaluation locally.  Otherwise, the result of the evaluation
        is placed in the result list and the function returns a
        non-zero value.

        The granularity is interpreted as the maximum size of a
        sublist to be evaluated on a single host. The job tree has a
        constant depth regardless of the granularity, and its root is
        a flattening function indicated by shared_cell. */

{
  int dependence;
  job front,back,top,bottom;
  counter operand_length, block_size, block_member;

  if (!initialized)
    avm_initialize_remote ();
  *result = NULL;
  if (*fault ? 1 : avm_offline () ? 1 : (operand_length = avm_recoverable_length (operand)) <= granularity)
    return 0;
  dependence = 0;
  front = back = NULL;
  if (!(block_size = balanced (operand_length, granularity)))
    block_size = 1;
  if (block_size == 1)
    operator = avm_compose (avm_copied (operator), avm_copied (listhead), fault);
  else
    operator = avm_map (avm_copied (operator), fault);
  while (*fault ? NULL : operand)
    {
      bottom = NULL;
      block_member = block_size;
      avm_new_job (&front, &back, avm_copied (operator), NULL, NULL, 0, fault);
      while (*fault ? 0 : operand ? block_member-- : 0)
	{
	  avm_new_job (&(back->prerequisites), &bottom, avm_copied (operand->head), back, NULL, 0, fault);
	  operand = operand->tail;
	  (back->dependence)++;
	}
      dependence++;
    }
  avm_new_job (&top, NULL, (block_size == 1 ? NULL : avm_copied (shared_cell)), NULL, front, dependence, fault);
  avm_dispose (operator);
  *result = (*fault ? avm_copied (memory_overflow) : avm_evaluation (top, 0, fault));
  return 1;
}












int
avm_remotely_reduced (operator, vacuous_case, operand, result, granularity, balanceable, fault)
     list operator;
     list vacuous_case;
     list operand;
     list *result;
     counter granularity;
     flag balanceable;
     int *fault;

     /* This function is similar to avm_remotely_mapped, but pertains
        to functions of the form reduce(f,k), where the operator
        corresponds to f and the vacuous case to k.

        The job tree has logarithmic depth in the length of the
        operand, and each non-terminal node has a linearly many
        prerequisites in the granularity. */
{
  job front,back,top,bottom;
  counter operand_length, block_size, block_member;

  if (!initialized)
    avm_initialize_remote ();
  *result = NULL;
  if (!(operand ? operand->tail : NULL))
    {
      *result = avm_copied (operand ? operand->head : vacuous_case);
      return 1;
    }
  if (*fault ? 1 : avm_offline() ? 1 : (operand_length = avm_recoverable_length (operand)) <= granularity)
    return 0;
  if ((block_size = balanced (operand_length, granularity)) < 2)
    block_size = 2;
  if (block_size == 2)
    operator = avm_compose (avm_copied (operator), avm_copied (listopair), fault);
  else
    operator = avm_reduce (avm_copied (operator), fault);
  front = back = top = NULL;
  operand_length = 0;
  while (*fault ? NULL : operand ? operand->tail : NULL)
    {
      bottom = NULL;
      operand_length++;
      block_member = block_size;
      avm_new_job (&front, &back, avm_copied (operator), NULL, NULL, 0, fault);
      while (*fault ? 0 : operand ? block_member-- : 0)
	{
	  avm_new_job (&(back->prerequisites), &bottom, avm_copied (operand->head), back, NULL, 0, fault);
	  operand = operand->tail;
	  (back->dependence)++;
	}
    }
  if (*fault ? NULL : operand)
    avm_new_job (&front, &back, avm_copied (operand->head), NULL, NULL, 0, fault);
  top = front;
  while (*fault ? NULL : top->corequisites)
    {
      if ((block_size = balanced (operand_length, granularity)) < 2)
	block_size = 2;
      operand_length = 0;
      front = back = NULL;
      while (*fault ? NULL : top ? top->corequisites : NULL)
	{
	  bottom = NULL;
	  operand_length++;
	  block_member = block_size;
	  avm_new_job (&front, &back, avm_copied (operator), NULL, NULL, 0, fault);
	  while (*fault ? 0 : top ? block_member-- : 0)
	    avm_queue_job (&(back->prerequisites), &bottom, &top, back);
	}
      if (*fault ? NULL : top)
	{
	  operand_length++;
	  avm_queue_job (&front, &back, &top, NULL);
	}
      top = front;
    }
  avm_dispose (operator);
  *result = (*fault ? avm_copied (memory_overflow) : avm_evaluation (top, balanceable, fault));
  return 1;
}











int
avm_remotely_sorted (operator, operand, result, granularity, fault)
     list operator;
     list operand;
     list *result;
     counter granularity;
     int *fault;

     /* This implements a distributed sort given the relational
	operator and the granularity.

        The job tree has logarithmic depth in the length of the
        operand, and the lowest level non-terminal nodes have linearly
        many descendents in the granularity, but above that they're
        all binary. */
{
  job front,back,top,middle,bottom;
  counter operand_length, block_size, block_member;
  list sorter,merger;

  if (!initialized)
    avm_initialize_remote ();
  *result = NULL;
  if (!(operand ? operand->tail : NULL))
    {
      *result = avm_copied (operand);
      return 1;
    }
  if (*fault ? 1 : avm_offline() ? 1 : (operand_length = avm_recoverable_length (operand)) <= granularity)
    return 0;
  if ((block_size = balanced (operand_length, granularity)) < 2)
    block_size = 2;
  front = back = top = NULL;
  sorter = avm_sort (avm_copied (operator), fault);
  merger = avm_compose (avm_merge (avm_copied (operator), fault), avm_copied (listopair), fault);
  while (*fault ? NULL : (operand ? operand->tail : NULL))
    {
      middle = NULL;
      block_member = block_size;
      avm_new_job (&front, &back, avm_copied ((block_size == 2) ? merger : sorter), NULL, NULL, 0, fault);
      while ((*fault = (*fault ? 1 : !back)) ? 0 : operand ? block_member-- : 0)
	{
	  if (block_size != 2)
	    avm_new_job (&(back->prerequisites), &middle, avm_copied (operand->head), back, NULL, 0, fault);
	  else
	    {
	      avm_new_job (&(back->prerequisites), &middle, NULL, back, NULL, 1, fault);
	      if (! (*fault = (*fault ? 1 : !middle)))
		avm_new_job (&(middle->prerequisites), NULL, avm_copied (operand->head), middle, NULL, 0, fault);
	    }
	  operand = operand->tail;
	  (back->dependence)++;
	}
    }
  if (*fault ? NULL : operand)
    {
      avm_new_job (&front, &back, NULL, NULL, NULL, 1, fault);
      if (! (*fault = (*fault ? 1 : !back)))
	avm_new_job (&(back->prerequisites), NULL, avm_copied (operand->head), back, NULL, 0, fault);
    }
  top = front;
  while (*fault ? NULL : top->corequisites)
    {
      front = back = NULL;
      while (*fault ? NULL : (top ? top->corequisites : NULL))
	{
	  avm_new_job (&front, &back, avm_copied (merger), NULL, NULL, 0, fault);
	  bottom = NULL;
	  if (! (*fault = (*fault ? 1 : !back)))
	    {
	      avm_queue_job (&(back->prerequisites), &bottom, &top, back);
	      avm_queue_job (&(back->prerequisites), &bottom, &top, back);
	    }
	}
      if (*fault ? NULL : top)
	avm_queue_job (&front, &back, &top, NULL);
      top = front;
    }
  avm_dispose (merger);
  avm_dispose (sorter);
  *result = (*fault ? avm_copied (memory_overflow) : avm_evaluation (top, 1, fault));
  return 1;
}









void
avm_initialize_remote ()

     /* This initializes static data structures. */
{

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_branches ();
  avm_initialize_apply ();
  avm_initialize_compare ();
  avm_initialize_jobs ();
  avm_initialize_farms ();
  avm_initialize_vglue ();
  listhead = avm_scanned_list("h<");
  listopair = avm_scanned_list("kE<");
  shared_cell = avm_join (NULL, NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
}






void
avm_count_remote ()

{
  server_list old,servers;

  if (!initialized)
    return;
  avm_dispose (memory_overflow);
  avm_dispose (listopair);
  avm_dispose (listhead);
  avm_dispose (shared_cell);
  shared_cell = NULL;
  memory_overflow = NULL;
  listopair = NULL;
  listhead = NULL;
  initialized = 0;
}
