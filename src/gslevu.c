
/* this file incorporates series acceleration routines from gsl

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
#include <avm/lists.h>
#include <avm/listfuns.h>
#include <avm/compare.h>
#include <avm/chrcodes.h>
#include <avm/gslevu.h>
#include <math.h>
#include <avm/apply.h>
#include <avm/matcon.h>

#if HAVE_GSL

#include <gsl/gsl_sum.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_errno.h>

#endif

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list empty_sequence = NULL;
static list memory_overflow = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list wild = NULL;
static list funs = NULL;

#if HAVE_GSL




list
levinu(trunc, series, fault)
     int trunc;
     list series;
     int *fault;

     /* series should be a list of doubles; the result is a pair
	of doubles (sum,error) */
{
  double *terms;
  double sum,error;
  list result,sum_list,error_list;
  size_t number_of_terms;
  gsl_sum_levin_u_workspace *w;
  gsl_sum_levin_utrunc_workspace *tw;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(number_of_terms = avm_length(series)))
    return avm_copied(empty_sequence);
  terms = avm_vector_of_list (series, sizeof(double), &result, fault);
  w = (trunc ? NULL : gsl_sum_levin_u_alloc (number_of_terms));
  tw = (trunc ? gsl_sum_levin_utrunc_alloc (number_of_terms) : NULL);
  if (trunc ? 0 : !(*fault = (*fault ? 1 : !(w ? terms : NULL))))
    gsl_sum_levin_u_accel (terms, number_of_terms, w, &sum, &error);
  if (trunc ? !(*fault = (*fault ? 1 : !(tw ? terms : NULL))) : 0)
    gsl_sum_levin_utrunc_accel (terms, number_of_terms, tw, &sum, &error);
  if (w)
    gsl_sum_levin_u_free (w);
  if (tw)
    gsl_sum_levin_utrunc_free (tw);
  if (terms)
    free (terms);
  sum_list = (*fault ? NULL : avm_list_of_value((void *) &sum, sizeof(double),fault));
  error_list = (*fault ? NULL : avm_list_of_value((void *) &error, sizeof(double), fault));
  if (*fault = (*fault ? 1 : !!result))
    {
      if (sum_list)
	avm_dispose(sum_list);
      if (error_list)
	avm_dispose(error_list);
    }
  else
    *fault = !(result = avm_recoverable_join(sum_list,error_list));
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}





#endif



list
avm_have_gslevu_call (list function_name, int *fault)

/* this reports the availability of a function */

{
#if HAVE_GSL
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_gslevu ();
  if (*fault)
    return NULL;
  comparison = avm_binary_comparison (function_name, wild, fault);
  if (*fault)
    return comparison;
  if (comparison)
    {
      avm_dispose(comparison);
      return avm_copied(funs);
    }
  if (!(membership = avm_binary_membership (function_name, funs, fault)) ? 1 : *fault)
    return membership;
  avm_dispose(membership);
  return ((*fault = !(result = avm_recoverable_join(avm_copied(function_name),NULL))) ? avm_copied(memory_overflow) : result);
#endif
  return NULL;
}










list
avm_gslevu_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;

     /* This figures out what function to call and calls it. */
{
#if HAVE_GSL
  list message;
  int function_number;

  if (*fault)
    return NULL;
  if (! initialized)
    avm_initialize_gsldif ();
  if (!(function_number = 0xff & (function_name ? function_name->characterization : 0)))
    {
      message = avm_position (function_name, funs, fault);
      if (*fault)
	return message;
      if (*fault = !message)
	return avm_copied (unrecognized_function_name);
      function_number = message->characterization;
      function_name->characterization = function_number;
      avm_dispose (message);
    }
  switch (function_number)
    {
    case 1: return levinu (0, argument, fault);
    case 2: return levinu (1, argument, fault);
    }
#endif /* HAVE_GSL */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}









void
avm_initialize_gslevu ()

     /* This initializes some static data structures. */

{
  char *funames[] = {
    "accel",
    "utrunc",
    NULL};            /* add more function names here up to a total of 255 */
  list back;
#if HAVE_GSL
  gsl_error_handler_t *old_handler;
#endif
  int string_number;

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_apply ();
  avm_initialize_listfuns ();
  avm_initialize_matcon ();
  avm_initialize_chrcodes ();
#if HAVE_GSL
  old_handler = gsl_set_error_handler_off();
#endif
  wild = avm_strung("*");
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  empty_sequence = avm_join (avm_strung ("empty gslevu sequence"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized gslevu function name"), NULL);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
    avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
}




void
avm_count_gslevu ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
    {
      return;
    }
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (memory_overflow);
  avm_dispose (empty_sequence);
  avm_dispose (unrecognized_function_name);
  funs = NULL;
  wild = NULL;
  memory_overflow = NULL;
  empty_sequence = NULL;
  unrecognized_function_name = NULL;
}
