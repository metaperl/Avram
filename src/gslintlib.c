
/* this file incorporates numerical integration routines from gsl

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
#include <avm/gslintlib.h>
#include <avm/apply.h>
#define __USE_ISOC99 1
#include <math.h>
#if HAVE_SETJMP
#include <setjmp.h>
#else
typdef int jmp_buf;
#endif
#if HAVE_GSL
#include <gsl/gsl_integration.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_errno.h>


struct fspec {
  double error;
  counter neval;
  jmp_buf caller;
  list operator;
  list message;
  int fault;
};

typedef struct fspec *fsptr;

#endif

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list bad_int_spec = NULL;
static list memory_overflow = NULL;
static list slow_convergence = NULL;
static list integration_error = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list wild = NULL;
static list funs = NULL;

#if HAVE_GSL
#if HAVE_SETJMP

#endif

/* the tightest tolerance worth trying */
#define MINIMUM_TOLERANCE 2E-14

/* the number of retries at bigger tolerances before giving up due to slow convergence */
#define RETRY_LIMIT 20

/* the factor by which the tolerance is increased on each attempt */
#define MAGNIFIER 3.16227766016838

/* the number of intervals stored in a work space used by gsl */
#define WORK_SIZE 1000

/* the maximum number of function evaluations allowed for integration before giving up due to slow convergence */
#define MAXIMUM_EVALUATIONS 3600

/* non-convergence codes returned by gsl */
#define ERRORS ((code == GSL_EBADTOL) ? 1 : ((code == GSL_ETOL) ? 1 : (code == GSL_EROUND)))

/* used by integration routines */
static gsl_integration_workspace* work = NULL;




double
integrand (x,params)
     double x;
     void* params;

     /* the c function passed to gsl for integration */
{
  list result;
  double *y;
  fsptr spec;

  spec = (fsptr) params;
  if (spec->fault = (spec->fault ? 1 : !!(spec->message)))
    return 0.0;
  if ((spec->neval)++ > MAXIMUM_EVALUATIONS)
    {
#if HAVE_SETJMP
      longjmp(spec->caller,-1);   /* no other way to recover from non-convergence unless this has been fixed lately */
#else
      avm_error("stuck on a non-converging integral; try qagx_tol");  /* better this than non-termination */
#endif
    }
  result = avm_list_of_value ((void *) &x, sizeof(double), &(spec->fault));
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
one_piece_integral (adaptive, lower_limit, upper_limit, eps, retry_limit, operator, fault)
     int adaptive;
     double lower_limit;
     double upper_limit;
     double eps;
     int retry_limit;
     list operator;
     int *fault;

     /* calls a gsl integration function for the given parameters */
{
  double output;
  gsl_function F;
  int code;
  int tries;
  list result;
  double error;
  int started;
  size_t sneval;
  struct fspec spec;

  if (*fault)
    return NULL;
  tries = 0;
  spec.fault = 0;
  spec.message = NULL;
  spec.operator = operator;
  F.params = &spec;
  F.function = &integrand;
#if HAVE_SETJMP
  if (setjmp(spec.caller) != 0)
    eps = eps * MAGNIFIER;
#endif
  started = spec.fault;
  while (started ? (spec.fault ? 0 : ((++tries <= retry_limit) ? ERRORS : 0)) : (started = 1))
    {
      spec.neval = 0;
      if (!adaptive)
	code = gsl_integration_qng (&F, lower_limit, upper_limit, 0.0, eps, &output, &error, &sneval);
      else if ((lower_limit < 0.0)  ? (fpclassify(lower_limit) ==  FP_INFINITE) : 0)
	{
	  if ((upper_limit > 0.0) ? (fpclassify(upper_limit) == FP_INFINITE) : 0)
	    code = gsl_integration_qagi (&F, 0.0, eps, WORK_SIZE, work, &output, &error);
	  else
	    code = gsl_integration_qagil (&F, upper_limit, 0.0, eps, WORK_SIZE, work, &output, &error);
	}
      else if ((upper_limit > 0.0) ? (fpclassify(upper_limit) == FP_INFINITE) : 0)
	code = gsl_integration_qagiu (&F, lower_limit, 0.0, eps, WORK_SIZE, work, &output, &error);
      else
	code = gsl_integration_qags (&F, lower_limit, upper_limit, 0.0, eps, WORK_SIZE, work, &output, &error);
      eps = MAGNIFIER * eps;
    }
  if (!(code ? 1 : (spec.fault ? 1 : ((tries > retry_limit + 1) ? 1 : !!(spec.message)))))
    return avm_list_of_value((void *) &output,sizeof(double),fault);
  if (*fault = !!spec.message)
    return spec.message;
  if (code != 0)
    {
      spec.message = avm_recoverable_strung((char *) gsl_strerror(code), fault);
      spec.message = (*fault ? spec.message : (spec.message ? avm_recoverable_join(spec.message,NULL) : NULL));
      *fault = 1;
      return (spec.message ? spec.message : avm_copied(integration_error));
    }
  *fault = 1;
  return avm_copied (slow_convergence);
}









static list
piecewise_integral (pts, npts, eps, retry_limit, operator, fault)
     double *pts;
     size_t npts;
     double eps;
     int retry_limit;
     list operator;
     int *fault;

     /* calls a gsl_integration_qagp for the given parameters */
{
  double output;
  gsl_function F;
  int code;
  int tries;
  list result;
  double error;
  int started;
  struct fspec spec;

  if (*fault)
    return NULL;
  tries = 0;
  spec.fault = 0;
  spec.message = NULL;
  spec.operator = operator;
  F.params = &spec;
  F.function = &integrand;
#if HAVE_SETJMP
  if (setjmp(spec.caller) != 0)
    eps = eps * MAGNIFIER;
#endif
  started = spec.fault;
  while (started ? (spec.fault ? 0 : ((++tries <= retry_limit) ? ERRORS : 0)) : (started = 1))
    {
      spec.neval = 0;
      code = gsl_integration_qagp (&F, pts, npts, 0.0, eps, WORK_SIZE, work, &output, &error);
      eps = MAGNIFIER * eps;
    }
  if (!(code ? 1 : (spec.fault ? 1 : ((tries > retry_limit + 1) ? 1 : !!(spec.message)))))
    return avm_list_of_value((void *) &output,sizeof(double),fault);
  if (*fault = !!spec.message)
    return spec.message;
  if (code != 0)
    {
      spec.message = avm_recoverable_strung((char *) gsl_strerror(code), fault);
      spec.message = (*fault ? spec.message : (spec.message ? avm_recoverable_join(spec.message,NULL) : NULL));
      *fault = 1;
      return (spec.message ? spec.message : avm_copied(integration_error));
    }
  *fault = 1;
  return avm_copied (slow_convergence);
}









static list
integral (piecewise, adaptive, fixed_tolerance, operand, fault)
     int piecewise;
     int adaptive;
     int fixed_tolerance;
     list operand;
     int *fault;

     /* operand is either (f,a,b), ((f,t),a,b), (f,p), or ((f,t),p),
	with function f, tolerance t, lower limit a, upper limit b and
	list of endpoints p */
{
  double *lower_limit;
  double *upper_limit;
  double *pts;
  size_t npts;
  double *eps;
  list f,t,a,b,p;
  list result;

  if (*fault)
    return NULL;
  npts = 0;
  pts = NULL;
  t = result = NULL;
  *fault = !(operand ? ((f = operand->head) ? (p = operand->tail) : NULL) : NULL);
  if (*fault = (*fault ? 1 : !(fixed_tolerance ? ((f = operand->head->head) ? !!(t = operand->head->tail) : 0) : 1)))
    return avm_copied (bad_int_spec);
  if (*fault = !((a = p->head) ? (b = p->tail) : 0))
    return avm_copied (bad_int_spec);
  if (piecewise ? !(piecewise = !!(p->tail->tail)) : 0)
    {
      b = p->tail->head;
      adaptive = 1;
    }
  eps = (fixed_tolerance ? (double *) avm_value_of_list (t, &result, fault) : NULL);
  if (piecewise)
    {
      npts = (size_t) avm_length (p);
      pts = (*fault ? NULL : (double *) avm_vector_of_list (p, sizeof(double), &result, fault));
    }
  else
    {
      lower_limit = (*fault ? NULL : (double *) avm_value_of_list (a, &result, fault));
      upper_limit = (*fault ? NULL : (double *) avm_value_of_list (b, &result, fault));
    }
  if (*fault = (*fault ? 1 : !!result))
    return (result ? result : avm_copied (bad_int_spec));
  if (!piecewise)
    {
      if (fixed_tolerance)
	return one_piece_integral (adaptive, *lower_limit, *upper_limit, *eps, 0, f, fault);
      return one_piece_integral (adaptive, *lower_limit, *upper_limit, MINIMUM_TOLERANCE, RETRY_LIMIT, f, fault);
    }
  if (fixed_tolerance)
    result = piecewise_integral (pts, npts, *eps, 0, f, fault);
  else
    result = piecewise_integral (pts, npts, MINIMUM_TOLERANCE, RETRY_LIMIT, f, fault);
  if (pts)
    free (pts);
  return result;
}









#endif




list
avm_have_gslint_call (list function_name, int *fault)

/* this reports the availability of a function */

{
#if HAVE_GSL
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_gslint ();
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
avm_gslint_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;
{
#if HAVE_GSL
  list message;
  int function_number;

  if (*fault)
    return NULL;
  if (! initialized)
    avm_initialize_gslint ();
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
    case 1: return integral (0, 0, 0, argument, fault);
    case 2: return integral (0, 0, 1, argument, fault);
    case 3: return integral (0, 1, 0, argument, fault);
    case 4: return integral (0, 1, 1, argument, fault);
    case 5: return integral (1, 1, 0, argument, fault);
    case 6: return integral (1, 1, 1, argument, fault);
    }
#endif /* HAVE_GSL */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}







void
avm_initialize_gslint ()

     /* This initializes some static data structures. */

{
  char *funames[] = {
    "qng",
    "qng_tol",
    "qagx",
    "qagx_tol",
    "qagp",
    "qagp_tol",
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
  wild = avm_strung ("*");
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  slow_convergence = avm_join (avm_strung ("slow convergence"), NULL);
  integration_error = avm_join (avm_strung ("integration error"), NULL);
  bad_int_spec = avm_join (avm_strung ("bad integral specfication"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized gslint function name"), NULL);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
    avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
#if HAVE_GSL
  old_handler = gsl_set_error_handler_off();
  if (!(work = gsl_integration_workspace_alloc(WORK_SIZE)))
    avm_error ("can't initialize gsl integration workspace");
#endif
}






void
avm_count_gslint ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{
  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (bad_int_spec);
  avm_dispose (memory_overflow);
  avm_dispose (slow_convergence);
  avm_dispose (integration_error);
  avm_dispose (unrecognized_function_name);
#if HAVE_GSL
  gsl_integration_workspace_free (work);
#endif
  funs = NULL;
  wild = NULL;
  bad_int_spec = NULL;
  memory_overflow = NULL;
  slow_convergence = NULL;
  unrecognized_function_name = NULL;
}
