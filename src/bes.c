
/* this file interfaces to gsl bessel functions

   Copyright (C) 2007 Dennis Furey

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
#include <avm/bes.h>
#if HAVE_GSL
#include <gsl/gsl_errno.h>
#include <gsl/gsl_sf_bessel.h>
#endif
#include <math.h>
#include <string.h>

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list bad_bess = NULL;
static list memory_overflow = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list funs = NULL;

static list shared_cell = NULL;
static list wild = NULL;

#if HAVE_GSL

typedef double (*bess)(double);
typedef double (*family)(int,double);
typedef double (*continuum)(double,double);
typedef double (*zeroid)(unsigned int);








static list
bessel(j0, j1, j2, jn, jnu, operand, fault)
     bess j0,j1,j2;
     family jn;
     continuum jnu;
     list operand;
     int *fault;
{
  double y,*x,*nu;
  list message,order;
  int n;

  if (*fault)
    return NULL;
  if (*fault = !operand)
    return avm_copied (bad_bess);
  message = NULL;
  x = (double *) avm_value_of_list(operand->tail,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return (message ? message : avm_copied (bad_bess));
  order = operand->head;
  if (!order)
    y = (*j0)(*x);
  else if (!(order->tail))
    y = (*j1)(*x);
  else if (j2 ? (order->head ? 0 : !(order->tail->tail)) : 0)
    y = (*j2)(*x);
  else
    {
      while (order->tail)
	order = order->tail;
      if (order->head ? (order->head->head ? 1 : !!(order->head->tail)) : 0)
	{
	  if (*fault = !jnu)
	    return avm_copied (bad_bess);
	  nu = (double *) avm_value_of_list(operand->head,&message,fault);
	  if (*fault = (*fault ? 1 : !!message))
	    return (message ? message : avm_copied (bad_bess));
	  y = (*jnu)(*nu,*x);
	}
      else
	{
	  if (*fault = !(n = (int) avm_counter (operand->head)))
	    return avm_copied (memory_overflow);
	  y = (*jn)(n,*x);
	}
    }
  return avm_list_of_value((void *) &y, sizeof(double), fault);
}









static list
zero(operator, operand, fault)
     zeroid operator;
     list operand;
     int *fault;

     /* the operator is either zero_J0 or zero_J1.  the operand is a
	list representing a double */
{
  double y;
  unsigned int s;

  if (*fault)
    return NULL;
  s = (unsigned int) avm_counter (operand);
  if (*fault = (s ? 0 : !!operand))
    return avm_copied (memory_overflow);
  y = (*operator)(s);
  return avm_list_of_value((void *) &y,sizeof(double),fault);
}










static list
zero_Jnu(operand, fault)
     list operand;
     int *fault;
{
  list message;
  double *nu,z;
  unsigned int s;

  if (*fault)
    return NULL;
  message = NULL;
  if (*fault = !operand)
    return avm_copied(bad_bess);
  nu = (double *) avm_value_of_list(operand->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  s = (unsigned int) avm_counter (operand->tail);
  if (*fault ? 1 : (s ? 0 : !!(operand->tail)))
    return avm_copied (memory_overflow);
  z = gsl_sf_bessel_zero_Jnu(*nu,s);
  return avm_list_of_value((void *) &z,sizeof(double),fault);
}








static list
lnKnu(operand, fault)
     list operand;
     int *fault;
{
  list message;
  double *nu,*x,z;

  if (*fault)
    return NULL;
  message = NULL;
  if (*fault = !operand)
    return avm_copied(bad_bess);
  nu = (double *) avm_value_of_list(operand->head,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  x = (double *) avm_value_of_list(operand->tail,&message,fault);
  if (*fault = (*fault ? 1 : !!message))
    return message;
  z = gsl_sf_bessel_lnKnu(*nu,*x);
  return avm_list_of_value((void *) &z,sizeof(double),fault);
}








#endif /* HAVE_GSL */





list
avm_have_bes_call (function_name, fault)
  list function_name;
  int *fault;

/* this reports the availability of a function */

{
#if HAVE_GSL
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_bes ();
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
avm_bes_call (function_name, argument, fault)
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
    avm_initialize_bes ();
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
    case 1: return bessel (&gsl_sf_bessel_J0,&gsl_sf_bessel_J1,NULL,&gsl_sf_bessel_Jn,&gsl_sf_bessel_Jnu,argument,fault);
    case 2: return bessel (&gsl_sf_bessel_Y0,&gsl_sf_bessel_Y1,NULL,&gsl_sf_bessel_Yn,&gsl_sf_bessel_Ynu,argument,fault);
    case 3: return bessel (&gsl_sf_bessel_I0,&gsl_sf_bessel_I1,NULL,&gsl_sf_bessel_In,&gsl_sf_bessel_Inu,argument,fault);
    case 4: return bessel (&gsl_sf_bessel_K0,&gsl_sf_bessel_K1,NULL,&gsl_sf_bessel_Kn,&gsl_sf_bessel_Knu,argument,fault);
    case 5: return bessel (&gsl_sf_bessel_j0,&gsl_sf_bessel_j1,&gsl_sf_bessel_j2,&gsl_sf_bessel_jl,NULL,argument,fault);
    case 6: return bessel (&gsl_sf_bessel_y0,&gsl_sf_bessel_y1,&gsl_sf_bessel_y2,&gsl_sf_bessel_yl,NULL,argument,fault);
    case 7: return bessel (&gsl_sf_bessel_I0_scaled,
			   &gsl_sf_bessel_I1_scaled,
			   NULL,
			   &gsl_sf_bessel_In_scaled,
			   &gsl_sf_bessel_Inu_scaled,
			   argument,
			   fault);
    case 8: return bessel (&gsl_sf_bessel_K0_scaled,
			   &gsl_sf_bessel_K1_scaled,
			   NULL,
			   &gsl_sf_bessel_Kn_scaled,
			   &gsl_sf_bessel_Knu_scaled,
			   argument,
			   fault);
    case 9: return bessel (&gsl_sf_bessel_i0_scaled,
			   &gsl_sf_bessel_i1_scaled,
			   &gsl_sf_bessel_i2_scaled,
			   &gsl_sf_bessel_il_scaled,
			   NULL,
			   argument,
			   fault);
    case 10: return bessel (&gsl_sf_bessel_k0_scaled,
			   &gsl_sf_bessel_k1_scaled,
			   &gsl_sf_bessel_k2_scaled,
			   &gsl_sf_bessel_kl_scaled,
			   NULL,
			   argument,
			   fault);
    case 11:  return zero (&gsl_sf_bessel_zero_J0, argument, fault);
    case 12:  return zero (&gsl_sf_bessel_zero_J1, argument, fault);
    case 13: return zero_Jnu (argument, fault);
    case 14: return lnKnu (argument, fault);
    }
#endif /* HAVE_GSL */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}






void
avm_initialize_bes ()

     /* This initializes some static data structures. */
{
  char *funames[] = {"J","Y","I","K","j","y","Isc","Ksc","isc","ksc","zJ0","zJ1","zJnu","lnKnu",NULL};
  list back;
  int string_number;
#if HAVE_GSL
  gsl_error_handler_t *old_handler;
#endif

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_listfuns ();
  avm_initialize_chrcodes ();
#if HAVE_GSL
  old_handler = gsl_set_error_handler_off();
#endif
  shared_cell = avm_join (NULL, NULL);
  wild = avm_strung("*");
  bad_bess = avm_join (avm_strung ("bad bessel function call"), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized bessel function name"), NULL);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
    avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
}







void
avm_count_bes ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (bad_bess);
  avm_dispose (shared_cell);
  avm_dispose (memory_overflow);
  avm_dispose (unrecognized_function_name);
  funs = NULL;
  bad_bess = NULL;
  shared_cell = NULL;
  memory_overflow = NULL;
  unrecognized_function_name = NULL;
}
