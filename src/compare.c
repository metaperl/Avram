
/* this file contains routines for something like lazy comparison

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
#include <avm/compare.h>

struct decision_node
{
  branch left_operand, right_operand;
  struct decision_node *consequence;
  struct decision_node *successor;
};

typedef struct decision_node *decision;

#define decision_cache_size 0xff

/* represents (nil,nil) */
static list shared_cell;

/* error messages represented as lists of lists of character representations */
static list memory_overflow;
static list invalid_comparison;

/* non-zero means static variables have been initialized */
static int initialized = 0;

/* a cache of decision nodes */
static decision available_decision = NULL;

/* the number of decision nodes in the cache */
static int available_decisions = 0;

/* the total number of allocated decisions (excluding the cache) */
static counter extant_decisions = 0;




static int
considered (left_operand, right_operand, consequence, consideration)
     branch left_operand;
     branch right_operand;
     decision consequence;
     decision *consideration;

     /* This creates a new decision node and pushes it onto a stack
        addressed by consideration. */

{
  int success;
  decision result;

  extant_decisions++;
  if (success = !!(result = available_decision))
    {
      available_decision = available_decision->successor;
      available_decisions--;
    }
  else
    success = !!(result = (decision) malloc (sizeof (*result)));
  if (success)
    {
      result->left_operand = left_operand;
      result->right_operand = right_operand;
      result->consequence = consequence;
      result->successor = *consideration;
      *consideration = result;
    }
  return (success);
}





static void
conclude (old_comparison)
     decision old_comparison;

     /* This frees up a decision node, possibly caching it for later
	use. */

{

  extant_decisions--;
  if (available_decisions > decision_cache_size)
    free (old_comparison);
  else
    {
      old_comparison->successor = available_decision;
      available_decision = old_comparison;
      available_decisions++;
    }
}








list
avm_comparison (operand, fault)
     list operand;
     int *fault;

     /* This algorithm is meant to perform comparison of lists as
        quickly, efficiently, and reliably as possible. It works by
        making a decision node. A decision node points to a couple of
        lists whose equality needs to be determined. If the equality
        can not be immediately inferred by pointer comparison or
        comparison of the characterization fields, then two more
        decision nodes are created, one for the pair of heads and one
        for the pair of tails. These decision nodes are pushed into a
        stack of decision nodes addressed by the successor field of
        the one in question. The pair of tails is pushed first, and
        then the heads. The one with the pair of tails has a
        consequence field pointing back to the decision node in
        question, but the one with the pair of heads has a NULL
        consequence field. If the pair of tails is found subsequently
        to be equal, that will imply that the whole lists in the
        decision node in question are equal, because the heads will
        have been already compared by the time the tails are compared
        and the algorithm would have terminated if a difference were
        detected. Therefore, whenever equality is detected (either by
        pointer equality or equality of characterization fields), the
        trail of consequence fields is followed until the a NULL
        pointer is reached. For each decision node visited by way of
        the consequence field when equality is detected, the less
        shared list of the pair is disposed of, and made to point to
        the more shared one. This operation not only saves memory but
        will make the comparison faster if these same operands are
        compared again. */

{
  int inequality_detected;
  decision consideration, comparisand, irrelevance, implicant;


#define current_tails &((*(consideration->left_operand))->tail),&((*(consideration->right_operand))->tail)
#define current_heads &((*(consideration->left_operand))->head),&((*(consideration->right_operand))->head)
#define unequal_characters(left,right) (left)->characterization!=(right)->characterization
#define more_shared(left,right) (left)->sharers>(right)->sharers?1:(left)->sharers<(right)->sharers?0:left<right
  /*
#define preferable(left,right) left?((left)->characteristic?(\
(right)->characteristic?(more_shared(left,right)):1):\
(right)->characteristic?0:more_shared(left,right)):1
  */
#define preferable(left,right) left?(((left)->characteristic ? 1 : !!((left)->value))?( \
((right)->characteristic ? 1 : !!((right)->value))?(more_shared(left,right)):1): \
((right)->characteristic ? 1 : !!((right)->value))?0:more_shared(left,right)):1


#define infer_equalities(predicate)					\
  {									\
    consideration = (comparisand = consideration)->successor;		\
    while ((implicant = comparisand)->consequence)			\
      {									\
	comparisand = implicant->consequence;				\
	if (predicate(*(comparisand->left_operand), *(comparisand->right_operand))) \
	  {								\
	    if (!((*(comparisand->right_operand))->discontiguous))	\
	      {								\
		avm_dispose (*(comparisand->right_operand));		\
		*(comparisand->right_operand) = avm_copied (*(comparisand->left_operand)); \
	      }								\
	  }								\
	else if (!((*(comparisand->left_operand))->discontiguous))	\
	  {								\
	    avm_dispose (*(comparisand->left_operand));			\
	    *(comparisand->left_operand) = avm_copied (*(comparisand->right_operand)); \
	  }								\
	conclude (implicant);						\
      }									\
    conclude (comparisand);						\
  }


  if (!initialized)
    avm_initialize_compare ();
  consideration = NULL;
  inequality_detected = 0;
  if (*fault = !operand)
    return (avm_copied (invalid_comparison));
  if (*fault = !considered (&(operand->head), &(operand->tail), NULL,&consideration))
    return (avm_copied (memory_overflow));
  do
    {
      if (*(consideration->left_operand) == *(consideration->right_operand))
	infer_equalities(preferable)
      else if (!(inequality_detected = !(*(consideration->left_operand) ? *(consideration->right_operand) : 0)))
	{
	  if (((*(consideration->left_operand))->characteristic ? (*(consideration->right_operand))->characteristic : 0))
	    {
	      if(!(inequality_detected=unequal_characters(*(consideration->left_operand),*(consideration->right_operand))))
		infer_equalities(more_shared)
	    }
	  else if (!(*fault = !considered (current_tails, consideration,&(consideration->successor))))
	    {
	      *fault = !considered (current_heads, NULL,&(consideration->successor));
	      consideration = consideration->successor;
	    }
	}
    }
  while (inequality_detected ? 0 : *fault ? 0 : !!consideration);
  while ((irrelevance = consideration))
    {
      consideration = irrelevance->successor;
      while ((implicant = irrelevance))
	{
	  irrelevance = implicant->consequence;
	  conclude (implicant);
	}
    }
  return (inequality_detected ? NULL : *fault ? avm_copied (memory_overflow) : avm_copied (shared_cell));
}








list
avm_binary_comparison (left_operand, right_operand, fault)
     list left_operand;
     list right_operand;
     int *fault;
{
  list operand;
  list result;

  if (*fault)
    return NULL;
  if (*fault = !(operand = avm_recoverable_join(avm_copied(left_operand),avm_copied(right_operand))))
    return avm_copied (memory_overflow);
  result = avm_comparison (operand, fault);
  avm_dispose (operand);
  return result;
}












void
avm_initialize_compare ()

     /* This initializes some local data structures. */

{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_branches ();
  avm_initialize_chrcodes ();
  shared_cell = avm_join (NULL, NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  invalid_comparison = avm_join (avm_strung ("invalid comparison"), NULL);
}






void
avm_count_compare ()

     /* This frees some local data structures and reports memory leaks. */

{
  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (shared_cell);
  avm_dispose (memory_overflow);
  avm_dispose (invalid_comparison);
  shared_cell = NULL;
  memory_overflow = NULL;
  invalid_comparison = NULL;
  if (extant_decisions)
    avm_reclamation_failure ("decisions", extant_decisions);
}



