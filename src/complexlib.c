
/* this file incorporates functions of complex numbers

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
#include <avm/complexlib.h>
#if HAVE_FENV
#include <fenv.h>
#endif
#if HAVE_COMPLEX
#include <complex.h>
#endif

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list wild = NULL;
static list empty_pair = NULL;
static list memory_overflow = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list funs = NULL;

#if HAVE_FENV
#if HAVE_COMPLEX

typedef complex (*complex_binary_operator)(complex,complex);
typedef complex (*complex_unary_operator)(complex);
typedef double (*real_unary_operator)(complex);





list
complex_creation(operand, fault)
     list operand;
     int *fault;
{
  list message;
  double *re,*im,z[2]; /* assuming a complex is represented as two contiguous doubles, real first */

  if (*fault)
    return NULL;
  message = NULL;
  if (*fault = !operand)
    return avm_copied (empty_pair);
  re = (double *) avm_value_of_list(operand->head,&message,fault);
  if (*fault)
    return message;
  avm_dispose(message);
  message = NULL;
  im = (double *) avm_value_of_list(operand->tail,&message,fault);
  if (*fault)
    return message;
  avm_dispose(message);
  z[0] = *re;
  z[1] = *im;
  return avm_list_of_value((void *) z,sizeof(double) * 2,fault);
}







list
complex_binary_evaluation(operator, operand, fault)
     complex_binary_operator operator;
     list operand;
     int *fault;
{
  list message;
  complex *x,*y,z;
  double v[2],w[2];

  if (*fault)
    return NULL;
  message = NULL;
  if (*fault = !operand)
    return avm_copied(empty_pair);
  x = (complex *) avm_value_of_list(operand->head,&message,fault);
  if (*fault)
    return message;
  avm_dispose(message);
  message = NULL;
  y = (complex *) avm_value_of_list(operand->tail,&message,fault);
  if (*fault)
    return message;
  avm_dispose(message);
  feclearexcept (FE_ALL_EXCEPT);
  if (avm_length(operand->head) == sizeof(complex))
    {
      if (avm_length(operand->tail) == sizeof(complex))
	z = (*operator)(*x,*y);
      else
	{
	  w[0] = (double) *y;
	  w[1] = 0.0;
	  z = (*operator)(*x,(complex) *w);
	}
    }
  else
    {
      v[0] = (double) *x;
      v[1] = 0.0;
      if (avm_length(operand->tail) == sizeof(complex))
	z = (*operator)((complex) *v,*y);
      else
	{
	  w[0] = (double) *y;
	  w[1] = 0.0;
	  z = (*operator)((complex) *v,(complex) *w);
	}
    }
  return avm_list_of_value((void *) &z,sizeof(complex),fault);
}







list
complex_unary_evaluation(operator, operand, fault)
     complex_unary_operator operator;
     list operand;
     int *fault;
{
  list message;
  complex *x,y;
  double z[2];

  if (*fault)
    return NULL;
  message = NULL;
  x = (complex *) avm_value_of_list(operand,&message,fault);
  if (*fault)
    return message;
  avm_dispose(message);
  feclearexcept (FE_ALL_EXCEPT);
  if (avm_length (operand) == sizeof(complex))
    y = (*operator)(*x);
  else
    {
      z[0] = (double) *x;
      z[1] = 0.0;
      y = (*operator)((complex) *z);
    }
  return avm_list_of_value((void *) &y,sizeof(complex),fault);
}








list
real_unary_evaluation(operator, operand, fault)
     real_unary_operator operator;
     list operand;
     int *fault;
{
  list message;
  complex *x;
  double y,z[2];

  if (*fault)
    return NULL;
  message = NULL;
  x = (complex *) avm_value_of_list(operand,&message,fault);
  if (*fault)
    return message;
  avm_dispose(message);
  feclearexcept (FE_ALL_EXCEPT);  
  if (avm_length (operand) == sizeof(complex))
    y = (*operator)(*x);
  else
    {
      z[0] = (double) *x;
      z[1] = 0.0;
      y = (*operator)((complex) *z);
    }
  return avm_list_of_value((void *) &y,sizeof(double),fault);
}







static complex
sum (l,r)
     complex l;
     complex r;

{
  complex x;

  feclearexcept (FE_ALL_EXCEPT);  
  x = l + r;
  return x;
}



static complex
difference (l,r)
     complex l;
     complex r;

{
  complex x;

  feclearexcept (FE_ALL_EXCEPT);  
  x = l - r;
  return x;
}





static complex
inverse_difference (l,r)
     complex l;
     complex r;

{
  complex x;

  feclearexcept (FE_ALL_EXCEPT);  
  x = r - l;
  return x;
}





static complex
product (l,r)
     complex l;
     complex r;

{
  complex x;

  feclearexcept (FE_ALL_EXCEPT);  
  x = l * r;
  return x;
}



static complex
quotient (l,r)
     complex l;
     complex r;

{
  complex x;

  feclearexcept (FE_ALL_EXCEPT);  
  x = l / r;
  return x;
}





static complex
inverse_quotient (l,r)
     complex l;
     complex r;

{
  complex x;

  feclearexcept (FE_ALL_EXCEPT);  
  x = r / l;
  return x;
}




#endif /* HAVE_COMPLEX */
#endif /* HAVE_FENV */




list
avm_have_complex_call (function_name, fault)
     list function_name;
     int *fault;

/* this reports the availability of a function */

{
#if HAVE_FENV
#if HAVE_COMPLEX
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_complex ();
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
avm_complex_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;

     /* This figures out what function to call and calls it. */
{
#if HAVE_FENV
#if HAVE_COMPLEX
  list message;
  int function_number;

  if (*fault)
    return NULL;
  if (! initialized)
    avm_initialize_complex ();
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
    case 1: return complex_creation (argument, fault);
    case 2: return real_unary_evaluation ((real_unary_operator) &creal, argument, fault);
    case 3: return real_unary_evaluation ((real_unary_operator) &cimag, argument, fault);
    case 4: return real_unary_evaluation ((real_unary_operator) &carg, argument, fault);
    case 5: return real_unary_evaluation ((real_unary_operator) &cabs, argument, fault);
    case 6: return complex_unary_evaluation ((complex_unary_operator) &csin, argument, fault);	
    case 7: return complex_unary_evaluation ((complex_unary_operator) &ccos, argument, fault);
    case 8: return complex_unary_evaluation ((complex_unary_operator) &ctan, argument, fault);
    case 9: return complex_unary_evaluation ((complex_unary_operator) &cexp, argument, fault);
    case 10: return complex_unary_evaluation ((complex_unary_operator) &clog, argument, fault);
    case 11: return complex_unary_evaluation ((complex_unary_operator) &conj, argument, fault);
    case 12: return complex_unary_evaluation ((complex_unary_operator) &csqrt, argument, fault);
    case 13: return complex_unary_evaluation ((complex_unary_operator) &csinh, argument, fault);
    case 14: return complex_unary_evaluation ((complex_unary_operator) &ccosh, argument, fault);
    case 15: return complex_unary_evaluation ((complex_unary_operator) &ctanh, argument, fault);
    case 16: return complex_unary_evaluation ((complex_unary_operator) &casinh, argument, fault);
    case 17: return complex_unary_evaluation ((complex_unary_operator) &cacosh, argument, fault);
    case 18: return complex_unary_evaluation ((complex_unary_operator) &catanh, argument, fault);
    case 19: return complex_unary_evaluation ((complex_unary_operator) &casin, argument, fault);
    case 20: return complex_unary_evaluation ((complex_unary_operator) &cacos, argument, fault);
    case 21: return complex_unary_evaluation ((complex_unary_operator) &catan, argument, fault);
    case 22: return complex_binary_evaluation ((complex_binary_operator) &cpow, argument, fault);
    case 23: return complex_binary_evaluation ((complex_binary_operator) &sum, argument, fault);
    case 24: return complex_binary_evaluation ((complex_binary_operator) &difference, argument, fault);
    case 25: return complex_binary_evaluation ((complex_binary_operator) &product, argument, fault);
    case 26: return complex_binary_evaluation ((complex_binary_operator) &quotient, argument, fault);
    case 27: return complex_binary_evaluation ((complex_binary_operator) &inverse_quotient, argument, fault);
    case 28: return complex_binary_evaluation ((complex_binary_operator) &inverse_difference, argument, fault);
    }
#endif /* HAVE_COMPLEX */
#endif /* HAVE_FENV */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}






void
avm_initialize_complex ()

     /* This initializes some static data structures. */
{
  list back;
  int string_number;
  char *funames[] = {
    "create",
    "creal",
    "cimag",
    "carg",
    "cabs",
    "csin",
    "ccos",
    "ctan",
    "cexp",
    "clog",
    "conj",
    "csqrt",
    "csinh",
    "ccosh",
    "ctanh",
    "casinh",
    "cacosh",
    "catanh",

    "casin",
    "cacos",
    "catan",

    "cpow",
    "add",
    "sub",
    "mul",
    "div",
    "vid",
    "bus",
    NULL};            /* add more function names here up to a total of 255 */

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_listfuns ();
  avm_initialize_chrcodes ();
  wild = avm_strung("*");
  empty_pair = avm_join (avm_strung ("empty pair"), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized complex function name"), NULL);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
    avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
}







void
avm_count_complex ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{
  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (empty_pair);
  avm_dispose (memory_overflow);
  avm_dispose (unrecognized_function_name);
  funs = NULL;
  empty_pair = NULL;
  memory_overflow = NULL;
  unrecognized_function_name = NULL;
}
