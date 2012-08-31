
/* this file interfaces to the harminv harmonic inversion library

   Copyright (C) 2008 Dennis Furey

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
   Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA

*/

#include <avm/common.h>
#include <avm/error.h>
#include <avm/lists.h>
#include <avm/listfuns.h>
#include <avm/compare.h>
#include <avm/harminv.h>
#include <avm/matcon.h>
#include <avm/mwrap.h>
#include <avm/chrcodes.h>
#include <math.h>
#include <string.h>

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list bad_harminv = NULL;
static list memory_overflow = NULL;
static list counter_overflow = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list funs = NULL;

static list shared_cell = NULL;
static list wild = NULL;

#if HAVE_COMPLEX
#if HAVE_HARMINV
#include <complex.h>
#include <harminv.h>

/*typedef void *harminv_data;*/

extern void harminv_solve(harminv_data d);
extern int harminv_get_num_freqs(harminv_data d);
extern void harminv_data_destroy(harminv_data d);
extern double harminv_get_Q(harminv_data d, int k);
extern double harminv_get_freq(harminv_data d, int k);
extern double harminv_get_decay(harminv_data d, int k);
extern complex harminv_get_omega(harminv_data d, int k);
extern complex harminv_get_amplitude(harminv_data d, int k);
extern double harminv_get_freq_error(harminv_data d, int k);
extern harminv_data harminv_data_create(int n, const complex *signal,double fmin, double fmax, int nf);



static list
extraction (d, fault)
     harminv_data d;
     int *fault;

     /* This takes an already solved harminv_data structure to a list
	of quintuples (type %jeeeeXXXXL in Ursala notation) of
	(amplitude,frequency,decay,quality,error). */
{
  list result,message;
  complex h;
  double e;
  int k;

  result = NULL;
  if (*fault)
    return NULL;
  k = harminv_get_num_freqs (d);
  while (*fault ? 0 : k)
    {
      k--;
      e = harminv_get_freq_error (d, k);
      message = avm_list_of_value ((void *) &e, sizeof(double), fault);
      e = harminv_get_Q (d, k);
      message = (*fault ? message : avm_recoverable_join (avm_list_of_value ((void *) &e, sizeof(double), fault), message));
      *fault = (*fault ? *fault : !(message));
      e = harminv_get_decay (d, k);
      message = (*fault ? message : avm_recoverable_join (avm_list_of_value ((void *) &e, sizeof(double), fault), message));
      *fault = (*fault ? *fault : !(message));
      e = harminv_get_freq (d, k);
      message = (*fault ? message : avm_recoverable_join (avm_list_of_value ((void *) &e, sizeof(double), fault), message));
      *fault = (*fault ? *fault : !(message));
      h = harminv_get_amplitude (d, k);
      message = (*fault ? message : avm_recoverable_join (avm_list_of_value ((void *) &h, sizeof(complex), fault), message));
      *fault = (*fault ? *fault : !(message));
      result = (*fault ? result : avm_recoverable_join (message, result));
      *fault = (*fault ? *fault : !(result));
    }
  if (!*fault)
    return result;
  avm_dispose (result);
  return (message ? message : avm_copied (memory_overflow));
}






static list
solution (operand, fault)
     list operand;
     int *fault;

     /* The operand should represent a tuple of the form
        (signal,(fmin,fmax),nf) where signal is a list of complex
        numbers, fmin and fmax are real numbers defining the band
        limits, and nf is the natural number of spectral basis
        functions. See README.gz from the harminv distribution for an
        explanaiton. If nf is zero, a default value of min(300,(fmax -
        fmin) * 1.1 * length(signal)) is used. The output is a list
        of quintuples (type %jeeeeXXXXL in Ursala notation) of
        (amplitude,frequency,decay,quality,error). */
{
  complex *signal;
  double *fmin;
  double *fmax;
  list message;
  int n, nf, N, k;
  harminv_data d;
  complex h;
  double e;

  message = NULL;
  if (*fault)
    return NULL;
  if (*fault = !(operand ? (operand->head ? (operand->tail ? operand->tail->head : NULL) : NULL) : NULL))
    return avm_copied (bad_harminv);
  if (*fault = !(n = (int) avm_recoverable_length (operand->head)))
    return avm_copied (counter_overflow);
  signal = (complex *) avm_vector_of_list (operand->head, sizeof(complex), &message, fault);
  fmin = (*fault ? NULL : (double *) avm_value_of_list (operand->tail->head->head, &message, fault));
  fmax = (*fault ? NULL : (double *) avm_value_of_list (operand->tail->head->tail, &message, fault));
  if (*fault)
    return (message);
  if (*fault = !(*fmin < *fmax))
    {
      free (signal);
      return avm_copied (bad_harminv);
    }
  if (!(nf = (int) avm_counter (operand->tail->tail)))
    {
      if (*fault = !!(operand->tail->tail))
	{
	  free (signal);
	  return avm_copied (counter_overflow);
	}
      nf = (11 * n) / 10;
      nf = (nf > 300 ? 300 : nf);
    }
  if (avm_setjmp () != 0)
    {
      avm_clearjmp ();
      free (signal);
      *fault = 1;
      return avm_copied (memory_overflow);
    }
  avm_manage_memory ();
  harminv_solve (d = harminv_data_create (n, signal, *fmin, *fmax, nf));
  h = harminv_get_amplitude (d, 0);
  e = harminv_get_freq_error (d, 0);         /* force remaining memory allocations before stopping management */
  avm_dont_manage_memory ();
  message = extraction (d, fault);
  avm_manage_memory ();
  harminv_data_destroy (d);
  avm_clearjmp ();
  avm_free_managed_memory ();
  free (signal);
  return message;
}




#endif /* HAVE_HARMINV */
#endif /*HAVE_COMPLEX */







list
avm_have_harminv_call (function_name, fault)
  list function_name;
  int *fault;

/* this reports the availability of a function */

{
#if HAVE_HARMINV
#if HAVE_COMPLEX
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_harminv ();
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
#endif
  return NULL;
}











list
avm_harminv_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;

     /* This figures out what function to call and calls it. */
{
#if HAVE_HARMINV
#if HAVE_COMPLEX
  list message;
  int function_number;

  if (*fault)
    return NULL;
  if (! initialized)
    avm_initialize_harminv ();
  if (!(function_number = 0xff & (function_name ? function_name->characterization : 0)))
    {
      message = avm_position (function_name, funs, fault);
      if (*fault = (*fault ? 1 : !message))
	return (message ? message : avm_copied (unrecognized_function_name));
      function_number = message->characterization;
      function_name->characterization = function_number;
      avm_dispose (message);
    }
  switch (function_number)
    {
      case 1: return solution (argument, fault);
    }
#endif /* HAVE_COMPLEX */
#endif /* HAVE_HARMINV */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}






void
avm_initialize_harminv ()

     /* This initializes some static data structures. */
{
  char *funames[] = {"hsolve",NULL};
  list back;
  int string_number;

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_listfuns ();
  avm_initialize_matcon ();
  avm_initialize_chrcodes ();
  shared_cell = avm_join (NULL, NULL);
  wild = avm_strung("*");
  bad_harminv = avm_join (avm_strung ("bad harminv function call"), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  counter_overflow = avm_join (avm_strung ("counter overflow"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized harminv function name"), NULL);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
    avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
}







void
avm_count_harminv ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (bad_harminv);
  avm_dispose (shared_cell);
  avm_dispose (memory_overflow);
  avm_dispose (counter_overflow);
  avm_dispose (unrecognized_function_name);
  funs = NULL;
  bad_harminv = NULL;
  shared_cell = NULL;
  memory_overflow = NULL;
  unrecognized_function_name = NULL;
}
