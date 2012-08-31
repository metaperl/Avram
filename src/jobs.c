
/* pertaining to concrete representations of dependence relations
   to regulate concurrent evaluation strategies

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

#include <avm/common.h>
#include <avm/lists.h>
#include <avm/chrcodes.h>
#include <avm/error.h>
#include <avm/jobs.h>
#include <avm/farms.h>

/* the maximum number of cached available jobs */
#define CACHE_SIZE 0xff

/* Non-zero means static variables have been initialized. */
static int initialized = 0;

/* the number of allocated and not reclaimed structs */
static counter extant_jobs = 0;

/* used to recycle job nodes more quickly than malloc and free */
static job available_job = NULL;

/* the number of job nodes in the cache */
static int available_jobs = 0;

/* an error message */
static list memory_overflow = NULL;



void
avm_free_job (front)
     job *front;

     /* this gets rid of a job and all the roots in it */
{
  job back,old;

  back = *front;
  while (*front)
    {
      while ((*front)->prerequisites ? 1 : back->corequisites ? 1 : 0)
	if (back->corequisites)
	  back = back->corequisites;
	else if ((*front)->prerequisites)
	  {
	    back->corequisites = (*front)->prerequisites;
	    (*front)->prerequisites = NULL;
	  }
      *front = (old = *front)->corequisites;
      avm_dispose (old->root);
      if (available_jobs >= CACHE_SIZE)
	{
	  extant_jobs--;
	  free (old);
	}
      else
	{
	  available_jobs++;
	  old->corequisites = available_job;
	  available_job = old;
	}
    }
}










void
avm_new_job(front, back, root, dependent, prerequisites, dependence, fault)
     job *front;
     job *back;
     list root;
     job dependent;
     job prerequisites;
     int dependence;
     int *fault;

     /* allocates a job, fills in the fields, and enqueues it if the back is non-null */
{
  job result,old;

  if (*fault)
    return;
  if (available_job)
    {
      result = available_job;
      available_job = available_job->corequisites;
      available_jobs--;
    }
  else 
    {
      if (*fault = !(result = (job) malloc (sizeof (*result))))
	{
	  avm_dispose (root);
	  avm_free_job (&prerequisites);
	  avm_free_job (front);
	  return;
	}
      extant_jobs++;
    }
  memset (result, 0, sizeof (*result));
  result->root = root;
  result->dependent = dependent;
  result->prerequisites = prerequisites;
  result->dependence = dependence;
  while (prerequisites)
    {
      prerequisites->dependent = result;
      prerequisites = prerequisites->corequisites;
    }
  if (!back)
    *front = result;
  else
    *back = (*back ? ((*back)->corequisites = result) : (*front = result));
}










void
avm_queue_job (front, back, top, dependent)
     job *front;
     job *back;
     job *top;
     job dependent;

     /* this enqueues an existing job into a queue of jobs,
	and removes it from the queue it was in */
{
  job old;

  *back = (*back ? ((*back)->corequisites = *top) : (*front = *top));
  (*top)->dependent = dependent;
  if (dependent)
    (dependent->dependence)++;
  *top = (old = *top)->corequisites;
  old->corequisites = NULL;
}











list
avm_evaluation (top, balanceable, fault)
     job top;
     flag balanceable;
     int *fault;

     /* This function plants the leaves of a job tree in a newly
        created farm. Then it calls the harvest function and waits for
        the result to propagate back to the root. 

        The balanceable parameter, which is passed through to the
        harvest function, allows for improved performance through out
        of order evaluation assuming the caller warrants that
        correctness is preserved. Currently this condition holds only
        for reduction with a commutative operator and sorting. */
{
  job next;
  list result;
  farm maggie;

  next = top;
  maggie = NULL;
  while (*fault ? NULL : next)
    if (next->prerequisites)
      next = next->prerequisites;
    else
      {
	avm_plant (&maggie, next, fault);
	while (next ? !(next->corequisites) : 0)
	  next = next->dependent;
	next = (next ? next->corequisites : NULL);
      }
  if (*fault)
    {
      avm_free_job (&top);
      return avm_copied (memory_overflow);
    }
  avm_harvest (maggie, balanceable, fault);
  result = (top ? avm_copied (top->root) : NULL);
  avm_free_job (&top);
  return result;
}











void
avm_initialize_jobs ()

     /* This initializes static data structures. */
{

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_chrcodes ();
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
}












void
avm_count_jobs ()

{
  server_list old,servers;

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (memory_overflow);
  memory_overflow = NULL;
  if (extant_jobs)
    avm_reclamation_failure ("jobs", extant_jobs);
}
