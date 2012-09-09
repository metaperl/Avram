
/* this file interfaces to sparse matrix routines from libufsparse

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
#include <avm/umf.h>
#if HAVE_UMF
#include <suitesparse/umfpack.h>
#endif

typedef double complex[2];

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list umf_error = NULL;
static list bad_umf_spec = NULL;
static list memory_overflow = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list wild = NULL;
static list funs = NULL;


#define freeif(x) if (x)			\
    free (x)


#if HAVE_UMF




static list
umfpack_i(real, result, mode, Ap, Ai, Ax, Ab, n, fault)
     int real;
     list result;
     int mode;
     int *Ap, *Ai;
     void *Ax, *Ab;   /* could be either double or complex */
     int n;
     int *fault;

     /* This takes a problem specification in compressed column form
	as described in the umfpack user guide. Result and *fault
	should be already initialized on entry, and are left unchanged
	if *fault is true. Some memory allocated to other input
	parameters is freed (unconditionally) by this procedure
	instead of the caller because not all of it is needed by the
	time the result is computed. */
{
  void *x;
  void *Symbolic, *Numeric;
  int status;

  Symbolic = NULL;
  status = 0;
  if (!(*fault = (*fault ? 1 : !!result)))
    {
      if (real)
	status = umfpack_di_symbolic(n, n, Ap, Ai, Ax, &Symbolic, NULL, NULL);
      else
	status = umfpack_zi_symbolic(n, n, Ap, Ai, Ax, NULL, &Symbolic, NULL, NULL);
      if (*fault = (status < 0))
	result = ((status == UMFPACK_ERROR_out_of_memory) ? avm_copied(memory_overflow) : avm_copied(umf_error));
    }
  Numeric = NULL;
  if (!*fault)
    {
      if (real)
	status = umfpack_di_numeric(Ap, Ai, Ax, Symbolic, &Numeric, NULL, NULL);
      else
	status = umfpack_zi_numeric(Ap, Ai, Ax, NULL, Symbolic, &Numeric, NULL, NULL);
      if (*fault = (status < 0))
	result = ((status == UMFPACK_ERROR_out_of_memory) ? avm_copied(memory_overflow) : avm_copied(umf_error));
    }
  if (Symbolic)
    umfpack_di_free_symbolic(&Symbolic);
  x = NULL;
  if (!*fault)
    {
      x = malloc(n * (real ? sizeof(double) : sizeof(complex)));
      if (*fault = !x)
	result = avm_copied(memory_overflow);
    }
  if (!*fault)
    {
      if (real)
	status = umfpack_di_solve(mode, Ap, Ai, Ax, x, Ab, Numeric, NULL, NULL);
      else
	status = umfpack_zi_solve(mode, Ap, Ai, Ax, NULL, x, NULL, Ab, NULL, Numeric, NULL, NULL);
      if (*fault = (status < 0))
	result = ((status == UMFPACK_ERROR_out_of_memory) ? avm_copied(memory_overflow) : avm_copied(umf_error));
    }
  if (Numeric)
    {
      if (real)
	umfpack_di_free_numeric(&Numeric);
      else
	umfpack_zi_free_numeric(&Numeric);
    }
  freeif (Ap);
  freeif (Ai);
  freeif (Ax);
  freeif (Ab);
  if (*fault)
    result = (result ? result : avm_copied(umf_error));
  else if (status == 0)
    result = avm_list_of_vector((void *) x,n,real ? sizeof(double) : sizeof(complex),fault);
  freeif (x);
  return result;
}








static list
umfpack_i_col(real, mode, piab, fault)
     int real;
     int mode;
     list piab;
     int *fault;

     /* piab should represent a tuple of (((p,i),a),b), p and i are
	lists of naturals, a is a list of floats, and b is also a list
	of floats. p and i correspond to the arrays Ap and Ai as
	documented in the Umfpack user guide to represent a sparse
	matrix in compressed column format, a is the list of non-zero
	entries in the matrix, and b is the list of entries in the
	(dense) column vector to be solved. Mode is a umfpack SYS parameter
        like UMFPACK_A that tells the library which system to solve. */

{
  list b,p,i,result;
  int *Ap, *Ai;
  void *Ax, *Ab;       /* could be either double or complex */
  counter Apl,Ail,j;

  if (*fault)
    return (NULL);
  b = result = NULL;
  if (*fault = ! (piab ? (piab->head ? (piab->tail ? piab->head->head : NULL) : NULL): NULL))
    return avm_copied(bad_umf_spec);
  Ap = (int *) malloc((Apl = avm_length(p = piab->head->head->head)) * sizeof(int));
  Ai = (int *) malloc((Ail = avm_length(i = piab->head->head->tail)) * sizeof(int));
  if (real)
    {
      Ax = (*fault ? NULL : avm_vector_of_list (piab->head->tail, sizeof(double), &result, fault));
      Ab = (*fault ? NULL : avm_vector_of_list (b = piab->tail, sizeof(double), &result, fault));
    }
  else
    {
      Ax = (*fault ? NULL : avm_vector_of_list (piab->head->tail, sizeof(complex), &result, fault));
      Ab = (*fault ? NULL : avm_vector_of_list (b = piab->tail, sizeof(complex), &result, fault));
    }
  if (*fault = (*fault ? 1 : !(Ap ? (Ai ? (Ax ? Ab : NULL): NULL): NULL)))
    result = (result ? result : avm_copied(memory_overflow));
  else
    {
      for (j = 0; j < Apl; j++)
	{
	  if (!p)
	    avm_internal_error(48);
	  Ap[j] = (int) avm_counter(p->head);
	  p = p->tail;
	}
      for (j = 0; j < Ail; j++)
	{
	  if (!i)
	    avm_internal_error(49);
	  Ai[j] = (int) avm_counter(i->head);
	  i = i->tail;
	}
      if (*fault ? 0 : (p ? i : NULL))
	avm_internal_error(54);
    }
  return umfpack_i (real, result, mode, Ap, Ai, Ax, Ab, (int) avm_length(b), fault);
}





static list
umfpack_i_trp(real, mode, ab, fault)
     int real;
     int mode;
     list ab;
     int *fault;

     /* ab represents a pair (a,b), where a is a list of triples
        ((i,j),x) with x being the i,jth entry in the input matrix and
        b being the column vector.  Only non-zero values of x are
        required. This form is more convenient than the compressed
        column form but requires an extra conversion step and more
        memory. However, the conversion is likely to be faster here
        than if it's done in virtual code. Mode is a umfpack SYS
        parameter like UMFPACK_A that tells the library which system
        to solve. */
{
  list a,b,result;
  int *Ap, *Ai, *Ti, *Tj, *Map, status, n_row, n_col, Abl;
  counter nz, k;
  void *t, *Ax, *Ab, *Tx;    /* could be either double or complex */

  if (*fault)
    return (NULL);
  if (*fault = ! ab)
    return (avm_copied(bad_umf_spec));
  a = ab->head;
  b = ab->tail;
  Ti = (int *) malloc((nz = avm_length(a)) * sizeof(int));
  Tj = (int *) malloc(nz * sizeof(int));
  Tx = malloc(nz * (real ? sizeof(double) : sizeof(complex)));
  result = NULL;
  if (*fault = ! (Ti ? (Tj ? Tx : NULL): NULL))
    result = avm_copied(memory_overflow);
  Abl = k = 0;
  n_row = 0;
  n_col = 0;
  while (*fault ? 0 : (k < nz))
    {
      if (!a)
	avm_internal_error(60);
      if (*fault = ! (a->head ? a->head->head: NULL))
	result = avm_copied(bad_umf_spec);
      else
	{
	  Ti[k] = (int) avm_counter(a->head->head->head);
	  Tj[k] = (int) avm_counter(a->head->head->tail);
	  n_row = ((n_row < Ti[k]) ? Ti[k] : n_row);
	  n_col = ((n_col < Tj[k]) ? Tj[k] : n_col);
	  t = avm_value_of_list(a->head->tail,&result,fault);
	  if (! *fault)
	    {
	      if (!t)
		avm_internal_error(55);
	      memcpy(Tx + ((k++) * (real ? sizeof(double) : sizeof(complex))),t,real ? sizeof(double) : sizeof(complex));
	    }
	  /*printf("((%d,%d),%0.1f)\n",Ti[k-1],Tj[k-1],((double *) Tx)[k-1]);*/
	  a = a->tail;
	}
    }
  n_row++;
  n_col++;
  /*printf("%d %d\n",n_row,n_col);*/
  if (*fault = (*fault ? 1 : (n_row != n_col)))
    result = (result ? result : avm_copied (bad_umf_spec));
  else
    Abl = n_row;
  if (*fault ? 0 : a)
    avm_internal_error(56);
  Map = Ap = Ai = (int *) (Ax = NULL);
  if (!*fault)
    {
      Ap = (int *) malloc((n_col+1) * sizeof(int));
      Ai = (int *) malloc(nz * sizeof(int));
      Ax = malloc(nz * (real ? sizeof(double) : sizeof(complex)));
      if (*fault = ! (Ap ? (Ai ? Ax : NULL): NULL))
	result = avm_copied(memory_overflow);
    }
  if (!*fault)
    {
      if (real)
	status = umfpack_di_triplet_to_col(n_row, n_col, nz, Ti, Tj, Tx, Ap, Ai, Ax, Map);
      else
	status = umfpack_zi_triplet_to_col(n_row, n_col, nz, Ti, Tj, Tx, NULL, Ap, Ai, Ax, NULL, Map);
      if (*fault = (status != UMFPACK_OK))
	result = ((status == UMFPACK_ERROR_out_of_memory) ? avm_copied(memory_overflow) : avm_copied(bad_umf_spec));
    }
  freeif (Ti);
  freeif (Tj);
  freeif (Tx);
  if (real)
    Ab = (*fault ? NULL : avm_vector_of_list (b, sizeof(double), &result, fault));
  else
    Ab = (*fault ? NULL : avm_vector_of_list (b, sizeof(complex), &result, fault));
  return umfpack_i (real, result, mode, Ap, Ai, Ax, Ab, Abl, fault);
}





#endif




list
avm_have_umf_call (list function_name, int *fault)

/* this reports the availability of a function */

{
#if HAVE_UMF
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_umf ();
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
avm_umf_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;
{
#if HAVE_UMF
  list message;
  int function_number;

  if (*fault)
    return NULL;
  if (! initialized)
    avm_initialize_umf ();
  if (!(function_number = 0xff & (function_name ? function_name->characterization : 0)))
    {
      message = avm_position (function_name, funs, fault);
      if (*fault)
	return (message);
      if (*fault = !message)
	return avm_copied (unrecognized_function_name);
      function_number = message->characterization;
      function_name->characterization = function_number;
      avm_dispose (message);
    }
  switch (function_number)
    {
    case  1: return umfpack_i_trp (1, UMFPACK_A, argument, fault);
    case  2: return umfpack_i_trp (1, UMFPACK_Aat, argument, fault);
    case  3: return umfpack_i_col (1, UMFPACK_A, argument, fault);
    case  4: return umfpack_i_col (1, UMFPACK_Aat, argument, fault);
    case  5: return umfpack_i_trp (0, UMFPACK_A, argument, fault);
    case  6: return umfpack_i_trp (0, UMFPACK_At, argument, fault);
    case  7: return umfpack_i_trp (0, UMFPACK_Aat, argument, fault);
    case  8: return umfpack_i_col (0, UMFPACK_A, argument, fault);
    case  9: return umfpack_i_col (0, UMFPACK_At, argument, fault);
    case 10: return umfpack_i_col (0, UMFPACK_Aat, argument, fault);
    }
#endif /* !HAVE_UMF */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}







void
avm_initialize_umf ()

     /* This initializes some static data structures. */

{
  char *funames[] = { /* see umfpack user guide page 24 */
    "di_a_trp",
    "di_t_trp",
    "di_a_col",
    "di_t_col",
    "zi_a_trp",
    "zi_c_trp",
    "zi_t_trp",
    "zi_a_col",
    "zi_c_col",
    "zi_t_col",
    NULL};            /* add more function names here up to a total of 255 */
  list back;
  int string_number;

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_listfuns ();
  avm_initialize_matcon ();
  avm_initialize_chrcodes ();
  wild = avm_strung("*");
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  umf_error = avm_join (avm_strung ("umf error"), NULL);
  bad_umf_spec = avm_join (avm_strung ("bad umf specification"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized umf function name"), NULL);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
    avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
}





void
avm_count_umf ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (umf_error);
  avm_dispose (bad_umf_spec);
  avm_dispose (memory_overflow);
  avm_dispose (unrecognized_function_name);
  funs = NULL;
  wild = NULL;
  umf_error = NULL;
  memory_overflow = NULL;
  unrecognized_function_name = NULL;
}
