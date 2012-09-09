
/* this file interfaces to some fourier transform functions from fftw

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
#include <avm/compare.h>
#include <avm/listfuns.h>
#include <avm/matcon.h>
#include <avm/chrcodes.h>
#include <avm/fftw.h>
#include <math.h>

#if HAVE_FFTW
#include <fftw3.h>
#endif

#define RE 0
#define IM 1

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list fftw_error = NULL;
static list bad_fftw_spec = NULL;
static list memory_overflow = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list wild = NULL;
static list funs = NULL;

#if HAVE_FFTW






static list
dft_1d (forward, operand, fault)
     int forward;
     list operand;
     int *fault;

     /* one dimensional fft from complex to complex data, takes a list
	of complex numbers with the direction specified by the forward
	parameter i.e., forward=0 means the inverse transform */
{
  fftw_complex *in;
  fftw_plan p;
  int n,i;
  list result;
  double scale;

  if (*fault)
    return NULL;
  result = NULL;
  scale = 1.0 / sqrt(n = (int) avm_length(operand));
  in = (fftw_complex *) avm_vector_of_list(operand,sizeof(fftw_complex),&result,fault);
  p = (*fault ? NULL : fftw_plan_dft_1d(n,in,in,forward ? FFTW_FORWARD : FFTW_BACKWARD,FFTW_ESTIMATE));
  if (!(*fault = (*fault ? 1 : !(in ? !!p : 0))))
    fftw_execute(p);
  if (p)
    fftw_destroy_plan(p);
  if (!*fault)
    for (i = 0; i < n; i++)
      {
	in[i][RE] = scale * in[i][RE];
	in[i][IM] = scale * in[i][IM];
      }
  result = (*fault ? result : avm_list_of_vector((void *) in,n,sizeof(fftw_complex),fault));
  if (in)
    free (in);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}








static list
dft_2d (forward, operand, fault)
     int forward;
     list operand;
     int *fault;

     /* two dimensional fft from complex to complex data, takes a list
	of lists of complex numbers with the direction specified by
	the forward parameter; i.e., forward=0 means the inverse
	transform */
{
  fftw_complex *in;
  fftw_plan p;
  int nx,ny,i;
  list result;
  double scale;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(operand ? operand->head : NULL))
    return avm_copied(bad_fftw_spec);
  ny = (int) avm_length(operand);
  nx = (int) avm_length(operand->head);
  scale = 1.0 / sqrt(nx * ny);
  in = (fftw_complex *) avm_matrix_of_list(0,0,0,0,operand,sizeof(fftw_complex),&result,fault);
  p = (*fault ? NULL : fftw_plan_dft_2d(nx,ny,in,in,forward ? FFTW_FORWARD : FFTW_BACKWARD,FFTW_ESTIMATE));
  if (!(*fault = (*fault ? 1 : !(in ? !!p : 0))))
    fftw_execute(p);
  if (p)
    fftw_destroy_plan(p);
  if (!*fault)
    for (i = 0; i < (nx * ny); i++)
      {
	in[i][RE] = scale * in[i][RE];
	in[i][IM] = scale * in[i][IM];
      }
  result = (*fault ? result : avm_list_of_matrix((void *) in,nx,ny,sizeof(fftw_complex),fault));
  if (in)
    free (in);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}









static list
dht_1d (operand, fault)
     list operand;
     int *fault;

     /* one dimensional Hartley transform, which is its own inverse;
	takes a list of reals to a list of reals */
{
  double *in;
  fftw_plan p;
  int n,i;
  list result;
  double scale;

  if (*fault)
    return NULL;
  result = NULL;
  scale = 1.0 / sqrt(n = (int) avm_length(operand));
  in = (double *) avm_vector_of_list(operand,sizeof(double),&result,fault);
  p = (*fault ? NULL : fftw_plan_r2r_1d(n,in,in,FFTW_DHT,FFTW_ESTIMATE));
  if (!(*fault = (*fault ? 1 : !(in ? !!p : 0))))
    fftw_execute(p);
  if (p)
    fftw_destroy_plan(p);
  if (!*fault)
    for (i = 0; i < n; i++)
      in[i] = scale * in[i];
  result = (*fault ? result : avm_list_of_vector((void *) in,n,sizeof(double),fault));
  if (in)
    free (in);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}









static list
dht_2d (operand, fault)
     list operand;
     int *fault;

     /* 2 dimensional Hartley transform, which is its own inverse;
	takes a list of lists of reals to a list of lists of reals*/
{
  double *in;
  fftw_plan p;
  int nx,ny,i;
  list result;
  double scale;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(operand ? operand->head : NULL))
    return avm_copied(bad_fftw_spec);
  ny = (int) avm_length(operand);
  nx = (int) avm_length(operand->head);
  scale = 1.0 / sqrt(nx * ny);
  in = (double *) avm_matrix_of_list(0,0,0,0,operand,sizeof(double),&result,fault);
  p = (*fault ? NULL : fftw_plan_r2r_2d(nx,ny,in,in,FFTW_DHT,FFTW_DHT,FFTW_ESTIMATE));
  if (!(*fault = (*fault ? 1 : !(in ? !!p : 0))))
    fftw_execute(p);
  if (p)
    fftw_destroy_plan(p);
  if (!*fault)
    for (i = 0; i < (nx * ny); i++)
      in[i] = scale * in[i];
  result = (*fault ? result : avm_list_of_matrix((void *) in,nx,ny,sizeof(double),fault));
  if (in)
    free (in);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}







#endif





list
avm_have_fftw_call (function_name, fault)
     list function_name;
     int *fault;

/* this reports the availability of a function */

{
#if HAVE_FFTW
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_fftw ();
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
  return (NULL);
}








list
avm_fftw_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;
{
#if HAVE_FFTW
  list message;
  int function_number;

  if (*fault)
    return NULL;
  if (! initialized)
    avm_initialize_fftw ();
  if (!(function_number = 0xff & (function_name ? function_name->characterization : 0)))
    {
      message = avm_position (function_name, funs, fault);
      if (*fault)
	return (message);
      if (*fault = !message)
	return(avm_copied (unrecognized_function_name));
      function_number = message->characterization;
      function_name->characterization = function_number;
      avm_dispose (message);
    }
  switch (function_number)
    {
    case 1: return dft_1d (1, argument, fault);
    case 2: return dft_1d (0, argument, fault);
    case 3: return dft_2d (1, argument, fault);
    case 4: return dft_2d (0, argument, fault);
    case 5: return dht_1d (argument, fault);
    case 6: return dht_2d (argument, fault);
    }
#endif /* HAVE_FFTW */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}







void
avm_initialize_fftw ()

     /* This initializes some static data structures. */
{
  char *funames[] = {
    "u_fw_dft",
    "u_bw_dft",
    "b_fw_dft",
    "b_bw_dft",
    "u_dht",
    "b_dht",
    NULL};            /* add more function names here up to a total of 255 */
  list back;
  int string_number;

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_matcon ();
  avm_initialize_chrcodes ();
  wild = avm_strung("*");
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  fftw_error = avm_join (avm_strung ("fftw error"), NULL);
  bad_fftw_spec = avm_join (avm_strung ("bad fftw specification"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized fftw function name"), NULL);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
    avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
}







void
avm_count_fftw ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (fftw_error);
  avm_dispose (bad_fftw_spec);
  avm_dispose (memory_overflow);
  avm_dispose (unrecognized_function_name);
  funs = NULL;
  wild = NULL;
  fftw_error = NULL;
  memory_overflow = NULL;
  unrecognized_function_name = NULL;
}
