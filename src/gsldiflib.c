
/* this file incorporates numerical differentiation routines from gsl

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
#include <avm/gsldiflib.h>
#include <math.h>
#include <avm/apply.h>

#if HAVE_GSL

#include <gsl/gsl_math.h>
#include <gsl/gsl_deriv.h>
#include <gsl/gsl_errno.h>

#endif

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list bad_dif_spec = NULL;
static list differentiation_error = NULL;
static list memory_overflow = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list wild = NULL;
static list funs = NULL;

#if HAVE_GSL

struct fspec {
  list operator;
  list message;
  int fault;
};

#define DEFAULT_TOLERANCE 1.0e-8

typedef struct fspec *fsptr;
typedef int (*differentiator)(gsl_function*,double,double,double*,double*);


double
differand (x,params)
     double x;
     void* params;

     /* the c function passed to gsl for differentiaiton */
{
  fsptr spec;
  list result;
  double *y;

  spec = (fsptr) params;
  if (spec->fault = ((spec->fault) ? 1 : !!(spec->message)))
    return 0.0;
  result = avm_list_of_value ((void *) &x, sizeof(double), &spec->fault);
  if (spec->fault)
    {
      spec->message = result;
      return 0.0;
    }
  result = avm_recoverable_apply(avm_copied(spec->operator), result, &(spec->fault));
  if (spec->fault)
    {
      spec->message = result;
      return 0.0;
    }
  y = (double *) avm_value_of_list (result, &(spec->message), &(spec->fault));
  if (spec->fault)
    {
      avm_dispose (result);
      return 0.0;
    }
  x = *y;
  avm_dispose (result);
  return x;
}








static list
derivative(method,argument,fault)
     differentiator method;
     list argument;
     int *fault;

     /* evaluates a derivative by the given method; the argument is a
	list representing (f,x), where f is a real valued function of
	a real variable, and x is a real number */
{
  list result;
  double *x;
  int code;
  gsl_function F;
  double dy,roundoff_error;
  struct fspec spec;

  if (*fault)
    return NULL;
  if (*fault = !(argument ? (argument->head ? argument->tail : NULL) : NULL))
    return avm_copied (bad_dif_spec);
  result = NULL;
  x = (double *) avm_value_of_list(argument->tail,&result,fault);
  if (*fault = (*fault ? 1 : !!result))
    return result;
  spec.operator = argument->head;
  spec.fault = 0;
  spec.message = NULL;
  F.params = (void *) &spec;
  F.function = &differand;
  if (!((code = (*method)(&F, *x, DEFAULT_TOLERANCE, &dy, &roundoff_error)) ? 1 : spec.fault))
    return avm_list_of_value((void *) &dy,sizeof(double),fault);
  if (!spec.message)
    spec.message = avm_recoverable_join(avm_recoverable_strung((char *) gsl_strerror(code),fault),NULL);
  result = (spec.message ? spec.message : avm_copied(differentiation_error));
  *fault = 1;
  return result;
}





static list
t_derivative(method,argument,fault)
     differentiator method;
     list argument;
     int *fault;

     /* evaluates a derivative by the given method; the argument is a
	list representing ((f,t),x), where f is a real valued function
	of a real variable, t is a real tolerance, and x is a real
	number */
{
  list result;
  double *x;
  double *t;
  int code;
  gsl_function F;
  double dy,roundoff_error;
  struct fspec spec;

  if (*fault)
    return NULL;
  if (*fault = !(argument ? (argument->head ? argument->tail : NULL) : NULL))
    return avm_copied (bad_dif_spec);
  if (*fault = !(argument->head->head ? argument->head->tail : NULL))
    return avm_copied (bad_dif_spec);
  result = NULL;
  x = (double *) avm_value_of_list(argument->tail,&result,fault);
  if (*fault = (*fault ? 1 : !!result))
    return result;
  t = (double *) avm_value_of_list(argument->head->tail,&result,fault);
  if (*fault = (*fault ? 1 : !!result))
    return result;
  spec.operator = argument->head->head;
  spec.fault = 0;
  spec.message = NULL;
  F.params = (void *) &spec;
  F.function = &differand;
  if (!((code = (*method)(&F, *x, *t, &dy, &roundoff_error)) ? 1 : spec.fault))
    return avm_list_of_value((void *) &dy,sizeof(double),fault);
  if (!spec.message)
    spec.message = avm_recoverable_join(avm_recoverable_strung((char *) gsl_strerror(code),fault),NULL);
  result = (spec.message ? spec.message : avm_copied(differentiation_error));
  *fault = 1;
  return result;
}






#endif





list
avm_have_gsldif_call (list function_name, int *fault)

/* this reports the availability of a function */

{
#if HAVE_GSL
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_gsldif ();
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
avm_gsldif_call (function_name, argument, fault)
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
    case 1: return derivative((differentiator) &gsl_deriv_central, argument, fault);
    case 2: return derivative((differentiator) &gsl_deriv_forward, argument, fault);
    case 3: return derivative((differentiator) &gsl_deriv_backward, argument, fault);
    case 4: return t_derivative((differentiator) &gsl_deriv_central, argument, fault);
    case 5: return t_derivative((differentiator) &gsl_deriv_forward, argument, fault);
    case 6: return t_derivative((differentiator) &gsl_deriv_backward, argument, fault);
    }
#endif /* HAVE_GSL */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}







void
avm_initialize_gsldif ()

     /* This initializes some static data structures. */

{
  char *funames[] = {
    "central",
    "forward",
    "backward",
    "t_central",
    "t_forward",
    "t_backward",
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
  avm_initialize_chrcodes ();
#if HAVE_GSL
  old_handler = gsl_set_error_handler_off();
#endif
  wild = avm_strung("*");
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  bad_dif_spec = avm_join (avm_strung ("bad derivative specification"), NULL);
  differentiation_error = avm_join (avm_strung ("differentiation error"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized gsldif function name"), NULL);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
    avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
}





void
avm_count_gsldif ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (bad_dif_spec);
  avm_dispose (memory_overflow);
  avm_dispose (differentiation_error);
  avm_dispose (unrecognized_function_name);
  funs = NULL;
  wild = NULL;
  bad_dif_spec = NULL;
  memory_overflow = NULL;
  differentiation_error = NULL;
  unrecognized_function_name = NULL;
}
