
/* this file incorporates statistical functions from the R math library

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
#include <avm/rmathlib.h>
#include <math.h>
#include <avm/apply.h>
#if HAVE_FENV
#if HAVE_RMATH
#include <fenv.h>

typedef double (*r_operator)(double);
typedef double (*rr_operator)(double,double);
typedef double (*rrb_operator)(double,double,int);
typedef double (*rrr_operator)(double,double,double);
typedef double (*rrbb_operator)(double,double,int,int);
typedef double (*rrrb_operator)(double,double,double,int);
typedef double (*rrrbb_operator)(double,double,double,int,int);

#include <Rmath.h>

double
Rlog1p(x)        /* work around a bug somewhere in Rmath */
     double x;
{
  return log1p(x);
}

#endif
#endif

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list bad_rmath_spec = NULL;
static list memory_overflow = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list wild = NULL;
static list funs = NULL;


#if HAVE_FENV
#if HAVE_RMATH




static list
r_evaluation(operator, operand, fault)
     r_operator operator;
     list operand;
     int *fault;

     /* the operator is a C function taking a double to a double, and
	the operand is a list representing a double */
{
  list message;
  double *a,y;

  if (*fault)
    return NULL;
  message = NULL;
  a = (double *) avm_value_of_list(operand,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  feclearexcept (FE_ALL_EXCEPT);  
  y = (*operator)(*a);
  return avm_list_of_value((void *) &y,sizeof(double),fault);
}




static list
rr_evaluation(operator, operand, fault)
     rr_operator operator;
     list operand;
     int *fault;

     /* the operator is a C function taking a pair of doubles to a
	double, and the operand is a list representing a pair of
	doubles */
{
  list message;
  double *a,*b,y;

  if (*fault)
    return NULL;
  message = NULL;
  if (*fault = !operand)
    return avm_copied(bad_rmath_spec);
  a = (double *) avm_value_of_list(operand->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  b = (double *) avm_value_of_list(operand->tail,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  feclearexcept (FE_ALL_EXCEPT);  
  y = (*operator)(*a,*b);
  return avm_list_of_value((void *) &y,sizeof(double),fault);
}





static list
rrb_evaluation(operator, operand, fault)
     rrb_operator operator;
     list operand;
     int *fault;
{
  list message;
  double *a,*b,y;
  int c;

  if (*fault)
    return NULL;
  message = NULL;
  if (*fault = !(operand ? operand->tail : NULL))
    return avm_copied(bad_rmath_spec);
  a = (double *) avm_value_of_list(operand->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  b = (double *) avm_value_of_list(operand->tail->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  c = !!(operand->tail->tail);
  feclearexcept (FE_ALL_EXCEPT);  
  y = (*operator)(*a,*b,c);
  return avm_list_of_value((void *) &y,sizeof(double),fault);
}





static list
rrr_evaluation(operator, operand, fault)
     rrr_operator operator;
     list operand;
     int *fault;
{
  list message;
  double *a,*b,*c,y;

  if (*fault)
    return NULL;
  message = NULL;
  if (*fault = !(operand ? operand->tail : NULL))
    return avm_copied(bad_rmath_spec);
  a = (double *) avm_value_of_list(operand->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  b = (double *) avm_value_of_list(operand->tail->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  c = (double *) avm_value_of_list(operand->tail->tail,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  feclearexcept (FE_ALL_EXCEPT);  
  y = (*operator)(*a,*b,*c);
  return avm_list_of_value((void *) &y,sizeof(double),fault);
}





static list
rrrb_evaluation(operator, operand, fault)
     rrrb_operator operator;
     list operand;
     int *fault;
{
  list message;
  double *a,*b,*c,y;
  int d;

  if (*fault)
    return NULL;
  message = NULL;
  if (*fault = !(operand ? (operand->tail ? operand->tail->tail : NULL) : NULL))
    return avm_copied(bad_rmath_spec);
  a = (double *) avm_value_of_list(operand->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  message = NULL;
  b = (double *) avm_value_of_list(operand->tail->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  c = (double *) avm_value_of_list(operand->tail->tail->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  d = !!(operand->tail->tail->tail);
  feclearexcept (FE_ALL_EXCEPT);  
  y = (*operator)(*a,*b,*c,d);
  return avm_list_of_value((void *) &y,sizeof(double),fault);
}





static list
rrbb_evaluation(operator, operand, fault)
     rrrb_operator operator;
     list operand;
     int *fault;
{
  list message;
  double *a,*b,y;
  int c,d;

  if (*fault)
    return NULL;
  message = NULL;
  if (*fault = !(operand ? (operand->tail ? operand->tail->tail : NULL) : NULL))
    return avm_copied(bad_rmath_spec);
  a = (double *) avm_value_of_list(operand->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  b = (double *) avm_value_of_list(operand->tail->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  c = !!(operand->tail->tail->head);
  d = !!(operand->tail->tail->tail);
  feclearexcept (FE_ALL_EXCEPT);  
  y = (*operator)(*a,*b,c,d);
  return avm_list_of_value((void *) &y,sizeof(double),fault);
}






static list
rrrbb_evaluation(operator, operand, fault)
     rrrbb_operator operator;
     list operand;
     int *fault;
{
  list message;
  double *a,*b,*c,y;
  int d,e;

  if (*fault)
    return NULL;
  if (*fault = !(operand ? (operand->tail ? (operand->tail->tail ? (operand->tail->tail->tail) : NULL) : NULL) : NULL))
    return avm_copied(bad_rmath_spec);
  message = NULL;
  a = (double *) avm_value_of_list(operand->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  b = (double *) avm_value_of_list(operand->tail->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  c = (double *) avm_value_of_list(operand->tail->tail->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  d = !!(operand->tail->tail->tail->head);
  e = !!(operand->tail->tail->tail->tail);
  feclearexcept (FE_ALL_EXCEPT);  
  y = (*operator)(*a,*b,*c,d,e);
  return avm_list_of_value((void *) &y,sizeof(double),fault);
}



#endif
#endif




list
avm_have_rmath_call (function_name,fault)
     list function_name;
     int *fault;

     /* this reports the availability of a function */
{
#if HAVE_FENV
#if HAVE_RMATH
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_rmath ();
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
  return (NULL);
}






list
avm_rmath_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;

     /* This figures out what function to call and calls it. */
{
#if HAVE_FENV
#if HAVE_RMATH
  list message;
  int function_number;

  if (*fault)
    return NULL;
  if (! initialized)
    avm_initialize_rmath ();
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
    case 1: return r_evaluation((r_operator) gammafn, argument, fault);
    case 2: return r_evaluation((r_operator) lgammafn, argument, fault);
    case 3: return r_evaluation((r_operator) digamma, argument, fault);
    case 4: return r_evaluation((r_operator) trigamma, argument, fault);
    case 5: return r_evaluation((r_operator) tetragamma, argument, fault);
    case 6: return r_evaluation((r_operator) pentagamma, argument, fault);
    case 7: return r_evaluation((r_operator) rt, argument, fault);
    case 8: return r_evaluation((r_operator) rexp, argument, fault);
    case 9: return r_evaluation((r_operator) rchisq, argument, fault);
    case 10: return rr_evaluation((rr_operator) beta, argument, fault);
    case 11: return rr_evaluation((rr_operator) lbeta, argument, fault);
    case 12: return rr_evaluation((rr_operator) bessel_j, argument, fault);
    case 13: return rr_evaluation((rr_operator) bessel_y, argument, fault);
    case 14: return rr_evaluation((rr_operator) rnorm, argument, fault);
    case 15: return rr_evaluation((rr_operator) rlnorm, argument, fault);
    case 16: return rr_evaluation((rr_operator) runif, argument, fault);
    case 17: return rr_evaluation((rr_operator) rnchisq, argument, fault);
    case 18: return rrb_evaluation((rrb_operator) dt, argument, fault);
    case 19: return rrb_evaluation((rrb_operator) dexp, argument, fault);
    case 20: return rrb_evaluation((rrb_operator) dchisq, argument, fault);
    case 21: return rrr_evaluation((rrr_operator) bessel_i, argument, fault);
    case 22: return rrr_evaluation((rrr_operator) bessel_k, argument, fault);
    case 23: return rrbb_evaluation((rrbb_operator) pchisq, argument, fault);
    case 24: return rrbb_evaluation((rrbb_operator) qchisq, argument, fault);
    case 25: return rrbb_evaluation((rrbb_operator) pt, argument, fault);
    case 26: return rrbb_evaluation((rrbb_operator) qt, argument, fault);
    case 27: return rrbb_evaluation((rrbb_operator) pexp, argument, fault);
    case 28: return rrbb_evaluation((rrbb_operator) qexp, argument, fault);
    case 29: return rrrb_evaluation((rrrb_operator) dnorm, argument, fault);
    case 30: return rrrb_evaluation((rrrb_operator) dlnorm, argument, fault);
    case 31: return rrrb_evaluation((rrrb_operator) dunif, argument, fault);
    case 32: return rrrb_evaluation((rrrb_operator) dnchisq, argument, fault);
    case 33: return rrrbb_evaluation((rrrbb_operator) pnorm, argument, fault);
    case 34: return rrrbb_evaluation((rrrbb_operator) qnorm, argument, fault);
    case 35: return rrrbb_evaluation((rrrbb_operator) plnorm, argument, fault);
    case 36: return rrrbb_evaluation((rrrbb_operator) qlnorm, argument, fault);
    case 37: return rrrbb_evaluation((rrrbb_operator) punif, argument, fault);
    case 38: return rrrbb_evaluation((rrrbb_operator) qunif, argument, fault);
    case 39: return rrrbb_evaluation((rrrbb_operator) pnchisq, argument, fault);
    case 40: return rrrbb_evaluation((rrrbb_operator) qnchisq, argument, fault);
    case 41: return rrb_evaluation((rrb_operator) dpois, argument, fault);
    case 42: return rrbb_evaluation((rrbb_operator) ppois, argument, fault);
    case 43: return rrbb_evaluation((rrbb_operator) qpois, argument, fault);
    case 44: return r_evaluation((r_operator) rpois, argument, fault);
    }
#endif /* HAVE_RMATH */
#endif /* HAVE_FENV */
   *fault = 1;
  return avm_copied (unrecognized_function_name);
}







void
avm_initialize_rmath ()

     /* This initializes some static data structures. */
{
  char *funames[] = {
    "gammafn",
    "lgammafn",
    "digamma",
    "trigamma",
    "tetragamma",
    "pentagamma",
    "rt",
    "rexp",
    "rchisq",
    "beta",
    "lbeta",
    "bessel_j",
    "bessel_y",
    "rnorm",
    "rlnorm",
    "runif",
    "rnchisq",
    "dt",
    "dexp",
    "dchisq",
    "bessel_i",
    "bessel_k",
    "pchisq",
    "qchisq",
    "pt",
    "qt",
    "pexp",
    "qexp",
    "dnorm",
    "dlnorm",
    "dunif",
    "dnchisq",
    "pnorm",
    "qnorm",
    "plnorm",
    "qlnorm",
    "punif",
    "qunif",
    "pnchisq",
    "qnchisq",
    "dpois",
    "ppois",
    "qpois",
    "rpois",
    NULL};            /* add more function names here up to a total of 255 */
  list back;
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
  bad_rmath_spec = avm_join (avm_strung ("bad rmath specification"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized rmath function name"), NULL);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
    avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
}




void
avm_count_rmath ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (bad_rmath_spec);
  avm_dispose (memory_overflow);
  avm_dispose (unrecognized_function_name);
  funs = NULL;
  bad_rmath_spec = NULL;
  memory_overflow = NULL;
  unrecognized_function_name = NULL;
}
