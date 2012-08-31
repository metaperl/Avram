
/* this file glues external math functions into the virtual machine

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
   Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA

*/

#include <avm/common.h>
#include <avm/error.h>
#include <avm/lists.h>
#include <avm/listfuns.h>
#include <avm/compare.h>
#include <avm/chrcodes.h>
#include <avm/mathlib.h>
#if HAVE_FENV
#include <fenv.h>
#endif
#define __USE_ISOC99 1
#include <math.h>
#include <string.h>

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list misprint = NULL;
static list empty_pair = NULL;
static list memory_overflow = NULL;
static list floating_point_exception = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list funs = NULL;

static list shared_cell = NULL;
static list wild = NULL;

#if HAVE_FENV

typedef double (*binary_operator)(double,double);
typedef double (*unary_operator)(double);






static list
binary_evaluation(operator, operand, fault)
     binary_operator operator;
     list operand;
     int *fault;

     /* the operator is a C function taking a pair of doubles to a
	double, and the operand is a list representing a pair of
	doubles */
{
  list message;
  double *x,*y,z;

  if (*fault)
    return NULL;
  message = NULL;
  if (*fault = !operand)
    return avm_copied(empty_pair);
  x = (double *) avm_value_of_list(operand->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  y = (double *) avm_value_of_list(operand->tail,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  feclearexcept (FE_ALL_EXCEPT);  
  z = (*operator)(*x,*y);
  return avm_list_of_value((void *) &z,sizeof(double),fault);
}








static list
unary_evaluation(operator, operand, fault)
     unary_operator operator;
     list operand;
     int *fault;

     /* the operator is a C function taking a double to a double, and
	the operand is a list representing a double */
{
  list message;
  double *x,y;

  if (*fault)
    return NULL;
  message = NULL;
  x = (double *) avm_value_of_list(operand,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  feclearexcept (FE_ALL_EXCEPT);  
  y = (*operator)(*x);
  return avm_list_of_value((void *) &y,sizeof(double),fault);
}









static list
leq(operand, fault)
     list operand;
     int *fault;

     /* computes the less or equal predicate on an operand
	representing a pair of doubles */
{
  list message;
  double *x,*y;

  if (*fault)
    return NULL;
  message = NULL;
  if (*fault = !operand)
    return avm_copied(empty_pair);
  x = (double *) avm_value_of_list(operand->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  y = (double *) avm_value_of_list(operand->tail,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  feclearexcept (FE_ALL_EXCEPT);  
  return ((*x <= *y) ? avm_copied (shared_cell) : NULL);
}












static list
isclass (fp_class, operand, fault)
     int fp_class;
     list operand;
     int *fault;
{
  list message;
  double *x;

  if (*fault)
    return NULL;
  message = NULL;
  x = (double *) avm_value_of_list (operand, &message, fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  return ((fpclassify(*x) == fp_class) ? avm_copied (shared_cell) : NULL);
}









static list
avm_strtod (argument, fault)
     list argument;
     int *fault;

     /* converts a list representing a character string to a list
	representing a double */
{
  list result;
  char *string;
  double d;

  if (*fault)
    return NULL;
  result = NULL;
  string = avm_standard_unstrung (argument, &result, fault);
  if (*fault = (*fault ? 1 : !!result))
    return result;
  errno = 0;
  d = strtod (string, NULL);
  free (string);
  return avm_list_of_value ((void *) &d, sizeof (double), fault);
}








static list
avm_asprintf (argument, fault)
     list argument;
     int *fault;

     /* converts a list representing a pair of a floating point format
	conversion specifier and a double to a list representing a
	character string; the specifer is checked for the absense of
	string conversions such as "%s", which would cause a
	segfault */
{
  char *format;
  char *output;
  list result;
  double *value;
  int b,p,d,i,n;   /* state variables for backslash, percent, digit, index counter and string length */

  if (*fault)
    return NULL;
  if (*fault = !argument)
    return avm_copied (empty_pair);
  result = NULL;
  value = (double *) avm_value_of_list (argument->tail, &result, fault);
  if (*fault = (*fault ? 1 : !!result))
    return result;
  format = avm_standard_unstrung (argument->head, &result, fault);
  if (*fault = (*fault ? 1 : !!result))
    return result;
  b = p = d = i = 0;
  n = strlen (format);
  while (*fault ? 0 : (i < n))
    {
      *fault = ((d ? 1 : p) ? ((format[i] == 's') ? 1 : (format[i] == 'S')) : 0);
      d = ((d ? 1 : p) ? ((format[i]=='-') ? 1 : ((format[i]=='.') ? 1 : ((format[i]>='0') ? (format[i]<='9') : 0))) : 0);
      p = (b ? 0 : (format[i] == '%'));
      b = (b ? 0 : (format[i] == '\\'));
      i++;
    }
  output = NULL;
  if (!(*fault = (*fault ? 1 : (asprintf (&output, format, *value) < 0))))
    result = avm_recoverable_standard_strung (output, fault);
  if (output)
    free (output);
  if (format)
    free (format);
  return (*fault ? (result ? result : avm_copied (misprint)) : result);
}






static double
sum (l,r)
     double l;
     double r;

{
  double x;

  feclearexcept (FE_ALL_EXCEPT);  
  x = l + r;
  return x;
}




static double
difference (l,r)
     double l;
     double r;

{
  double x;

  feclearexcept (FE_ALL_EXCEPT);  
  x = l - r;
  return x;
}




static double
inverse_difference (l,r)
     double l;
     double r;

{
  double x;

  feclearexcept (FE_ALL_EXCEPT);  
  x = r - l;
  return x;
}




static double
product (l,r)
     double l;
     double r;

{
  double x;

  feclearexcept (FE_ALL_EXCEPT);  
  x = l * r;
  return x;
}




static double
quotient (l,r)
     double l;
     double r;

{
  double x;

  feclearexcept (FE_ALL_EXCEPT);  
  x = l / r;
  return x;
}




static double
inverse_quotient (l,r)
     double l;
     double r;

{
  double x;

  feclearexcept (FE_ALL_EXCEPT);  
  x = r / l;
  return x;
}




#endif /* HAVE_FENV */





list
avm_have_math_call (function_name, fault)
  list function_name;
  int *fault;

/* this reports the availability of a function */

{
#if HAVE_FENV
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_math ();
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
avm_math_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;

     /* This figures out what function to call and calls it. */
{
#if HAVE_FENV
  list message;
  int function_number;

  if (*fault)
    return NULL;
  if (! initialized)
    avm_initialize_math ();
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
    case 1: return leq (argument, fault);
    case 2: return isclass (FP_NAN, argument, fault);
    case 3: return isclass (FP_INFINITE, argument, fault);
    case 4: return isclass (FP_ZERO, argument, fault);
    case 5: return isclass (FP_SUBNORMAL, argument, fault);
    case 6: return isclass (FP_NORMAL, argument, fault);
    case 7: return avm_strtod (argument, fault);
    case 8: return avm_asprintf (argument, fault);
    case 9: return unary_evaluation ((unary_operator) &sin, argument, fault);
    case 10: return unary_evaluation ((unary_operator) &cos, argument, fault);
    case 11: return unary_evaluation ((unary_operator) &tan, argument, fault);
    case 12: return unary_evaluation ((unary_operator) &exp, argument, fault);
    case 13: return unary_evaluation ((unary_operator) &log, argument, fault);
    case 14: return unary_evaluation ((unary_operator) &sqrt, argument, fault);
    case 15: return unary_evaluation ((unary_operator) &cbrt, argument, fault);
    case 16: return unary_evaluation ((unary_operator) &asin, argument, fault);
    case 17: return unary_evaluation ((unary_operator) &acos, argument, fault);
    case 18: return unary_evaluation ((unary_operator) &atan, argument, fault);
    case 19: return unary_evaluation ((unary_operator) &expm1, argument, fault);
    case 20: return unary_evaluation ((unary_operator) &log1p, argument, fault);
    case 21: return unary_evaluation ((unary_operator) &sinh, argument, fault);
    case 22: return unary_evaluation ((unary_operator) &cosh, argument, fault);
    case 23: return unary_evaluation ((unary_operator) &tanh, argument, fault);
    case 24: return unary_evaluation ((unary_operator) &fabs, argument, fault);
    case 25: return unary_evaluation ((unary_operator) &asinh, argument, fault);
    case 26: return unary_evaluation ((unary_operator) &acosh, argument, fault);
    case 27: return unary_evaluation ((unary_operator) &atanh, argument, fault);
    case 28: return unary_evaluation ((unary_operator) &ceil, argument, fault);
    case 29: return unary_evaluation ((unary_operator) &trunc, argument, fault);
    case 30: return unary_evaluation ((unary_operator) &round, argument, fault);
    case 31: return unary_evaluation ((unary_operator) &floor, argument, fault);
    case 32: return unary_evaluation ((unary_operator) &ceil, argument, fault);
    case 33: return binary_evaluation ((binary_operator) &sum, argument, fault);
    case 34: return binary_evaluation ((binary_operator) &pow, argument, fault);
    case 35: return binary_evaluation ((binary_operator) &hypot, argument, fault);
    case 36: return binary_evaluation ((binary_operator) &atan2, argument, fault);
    case 37: return binary_evaluation ((binary_operator) &product, argument, fault);
    case 38: return binary_evaluation ((binary_operator) &quotient, argument, fault);
    case 39: return binary_evaluation ((binary_operator) &remainder, argument, fault);
    case 40: return binary_evaluation ((binary_operator) &nextafter, argument, fault);
    case 41: return binary_evaluation ((binary_operator) &difference, argument, fault);
    case 42: return binary_evaluation ((binary_operator) &inverse_quotient, argument, fault);
    case 43: return binary_evaluation ((binary_operator) &inverse_difference, argument, fault);
    }
#endif /* HAVE_FENV */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}






void
avm_initialize_math ()

     /* This initializes some static data structures. */
{
  char *funames[] = {
    "islessequal",
    "isnan",
    "isinfinite",
    "iszero",
    "isubnormal",
    "isnormal",
    "strtod",
    "asprintf",
    "sin",
    "cos",
    "tan",
    "exp",
    "log",
    "sqrt",
    "cbrt",
    "asin",
    "acos",
    "atan",
    "expm1",
    "log1p",
    "sinh",
    "cosh",
    "tanh",
    "fabs",
    "asinh",
    "acosh",
    "atanh",
    "ceil",
    "trunc",
    "round",
    "floor",
    "ceil",
    "add",
    "pow",
    "hypot",
    "atan2",
    "mul",
    "div",
    "remainder",
    "nextafter",
    "sub",
    "vid",
    "bus",
    NULL};            /* add more function names here up to a total of 255 */
  list back;
  int string_number;

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_listfuns ();
  avm_initialize_chrcodes ();
  shared_cell = avm_join (NULL, NULL);
  wild = avm_strung("*");
  empty_pair = avm_join (avm_strung ("empty pair"), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  misprint = avm_join (avm_strung ("invalid asprintf() specifier"), NULL);
  floating_point_exception = avm_join (avm_strung ("floating point exception"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized math function name"), NULL);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
    {
      /* printf("%s\n",funames[string_number]); */
      avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
    }
}









void
avm_count_math ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (misprint);
  avm_dispose (empty_pair);
  avm_dispose (shared_cell);
  avm_dispose (memory_overflow);
  avm_dispose (floating_point_exception);
  avm_dispose (unrecognized_function_name);
  funs = NULL;
  misprint = NULL;
  empty_pair = NULL;
  shared_cell = NULL;
  memory_overflow = NULL;
  floating_point_exception = NULL;
  unrecognized_function_name = NULL;
}
