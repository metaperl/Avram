
/* this file interfaces to linear programming routines from glpk

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
#include <avm/chrcodes.h>
#include <avm/glpklib.h>
#include <avm/mwrap.h>
#if HAVE_GLPK
#include <glpk.h>
#endif

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list bad_glpk_spec = NULL;
static list memory_overflow = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list wild = NULL;
static list funs = NULL;

#if HAVE_GLPK







static LPX
*problem_object(cost_vector, constraint_matrix, constraint_vector, rows, columns, message, fault)
     list cost_vector;
     list constraint_matrix;
     list constraint_vector;
     int *rows;
     int *columns;
     list *message;
     int *fault;

     /* This takes the lists specifying the problem to an LPX type
	problem object. It uses memory managment routines from mwrap.c
	because glpk performs many allocations behind the scenes with
	demonstrable leaks and no policy for exception handling
	defined by the API. Memory management is switched off on exit
	from this function, but will need to be on when the problem
	object is deleted. If there is insufficient memory, a NULL
	value is returned and if we're lucky everything is
	reclaimed. */
{
  int  *ia, *ja, problem_size, position_number, row_number, column_number;
  double *ar, *y, *c, *a;
  LPX *lp;

  if (*fault = (*fault ? 1 : !!(*message)))
    return NULL;
  ia = (int *) malloc((problem_size = 1 + (int) avm_length(constraint_matrix)) * sizeof(int));
  ja = (int *) malloc(problem_size * sizeof(int));
  ar = (double *) malloc(problem_size * sizeof(double));
  if (*fault = !(ia ? (ja ? !!ar : 0) : 0))
    {
      if (ia)
	free (ia);
      if (ja)
	free (ja);
      if (ar)
	free (ar);
      *message = avm_copied (memory_overflow);
      return NULL;
    }
  avm_manage_memory ();                                     /* all allocations by glpk will be logged from this point */
  if (*fault = (avm_setjmp() != 0))
    {
      free (ia);                 /* this point is reached if setjmp is configured and malloc fails in a glpk function */
      free (ja);
      free (ar);
      avm_clearjmp ();
      *message = avm_copied (memory_overflow);
      return NULL;
    }
  if (*fault = !(lp = lpx_create_prob ()))                    /* otherwise hope it returns a NULL if allocation fails */
    {
      free (ia);
      free (ja);
      free (ar);
      avm_clearjmp ();
      avm_free_managed_memory ();
      *message = avm_copied (memory_overflow);
      return NULL;
    }
  lpx_set_obj_dir (lp, LPX_MIN);                 /* success can't be verified via the documented API after this point */
  lpx_set_int_parm (lp, LPX_K_MSGLEV, 0);
  lpx_set_int_parm (lp, LPX_K_PRESOL, 1);
  lpx_add_cols (lp, *columns = (int) avm_length (cost_vector));
  column_number = 0;
  lpx_set_obj_coef (lp, column_number, 0.0);
  while (*fault ? 0 : (column_number++ < *columns))
    {
      avm_dont_manage_memory ();
      c = ((*fault = !cost_vector) ? NULL : (double *) avm_value_of_list(cost_vector->head, message, fault));
      avm_manage_memory ();
      lpx_set_col_bnds(lp, column_number, LPX_LO, 0.0, 0.0);
      lpx_set_obj_coef(lp, column_number, *fault ? 0.0 : *c);
      cost_vector = cost_vector->tail;
    }
  lpx_add_rows(lp, *rows = (int) avm_length(constraint_vector));
  row_number = 0;
  while (*fault ? 0 : (row_number++ < *rows))
    {
      avm_dont_manage_memory ();
      y = ((*fault = !constraint_vector) ? NULL : (double *) avm_value_of_list (constraint_vector->head, message, fault));
      avm_manage_memory ();
      lpx_set_row_bnds (lp, row_number, LPX_FX, *fault ? 0.0 : *y, *fault ? 0.0 : *y);
      constraint_vector = constraint_vector->tail;
    }
  position_number = 0;
  avm_dont_manage_memory ();
  while (constraint_matrix ? !(*fault = (*fault ? 1 : !(constraint_matrix->head ? constraint_matrix->head->head : 0))) : 0)
    {
      ia[++position_number] = 1 + (int) avm_counter(constraint_matrix->head->head->head);
      ja[position_number] = 1 + (int) avm_counter(constraint_matrix->head->head->tail);
      *fault = ((ia[position_number] > *rows) ? 1 : (ja[position_number] > *columns));
      a = (*fault ? NULL : (double *) avm_value_of_list(constraint_matrix->head->tail, message, fault));
      ar[position_number] = (*fault ? 0.0 : *a);
      constraint_matrix = constraint_matrix->tail;
    }
  avm_manage_memory ();
  if (!*fault)
    lpx_load_matrix(lp, problem_size - 1, ia, ja, ar);
  avm_dont_manage_memory ();
  free (ia);
  free (ja);
  free (ar);
  if (!*fault)
    {
      avm_clearjmp ();
      return lp;
    }
  *message = (*message ? *message : avm_copied (bad_glpk_spec));
  avm_manage_memory ();
  if (lp)
    lpx_delete_prob (lp);
  avm_free_managed_memory ();
  avm_clearjmp ();
  return NULL;
}











static list
simplex_solution (lp, columns, fault)
     LPX *lp;
     int columns;
     int *fault;

     /* this gets the simplex solution out of the problem object,
	ignoring the variables that aren't exactly zero */
{
  list front, back, item, index;
  int column_number;
  double x;

  column_number = 0;
  front = back = item = NULL;
  while (*fault ? 0 : (column_number++ < columns))
    if ((x = lpx_get_col_prim (lp, column_number)) != 0)
      if (!(*fault = !((index = avm_recoverable_natural (column_number - 1)) ? 1 : !(column_number - 1))))
	{
	  item = avm_list_of_value((void *) &x, sizeof(double), fault);
	  if (*fault)
	    {
	      if (index)
		avm_dispose (index);
	    }
	  else if (!(*fault = !(item = avm_recoverable_join (index,item))))
	    {
	      avm_recoverable_enqueue(&front, &back, item, fault);
	      item = NULL;
	    }
	}
  if (*fault ? front : NULL)
    avm_dispose (front);
  return (*fault ? (item ? item : avm_copied (memory_overflow)) : front);
}











static list
interior_solution (lp, rows, columns, fault)
     LPX *lp;
     int rows;
     int columns;
     int *fault;

     /* this gets the interior point solution out of the problem
	object taking no more than the right number of variables by
	ignoring the smallest ones */
{
  list front, back, item, index;
  int column_number, non_zeros, threshold_index;
  double x, threshold;

  non_zeros = 0;
  threshold = 0.0;
  while ((non_zeros == rows) ? 0 : rows)
    {
      threshold_index = 0;
      while ((non_zeros == rows) ? 0 : (threshold_index < columns))
	{
	  non_zeros = column_number = 0;
	  threshold = lpx_ipt_col_prim (lp, threshold_index + 1);
	  while ((non_zeros > rows) ? 0 : column_number < columns)
	    non_zeros = non_zeros + ((lpx_ipt_col_prim (lp, ++column_number) >= threshold) ? 1 : 0);
	  if (non_zeros < rows)
	    while ((threshold_index < columns) ? (lpx_ipt_col_prim (lp, threshold_index + 1) >= threshold) : 0)
	      threshold_index++;
	  else if (non_zeros > rows)
	    while ((threshold_index < columns) ? (lpx_ipt_col_prim (lp, threshold_index + 1) <= threshold) : 0)
	      threshold_index++;
	}
      if (non_zeros != rows)
	{
	  non_zeros = 0;
	  threshold = 0.0;
	  rows--;
	}
    }
  column_number = 0;
  front = back = item = NULL;
  while (*fault ? 0 : (column_number++ < columns))
    if ((x = lpx_ipt_col_prim (lp, column_number)) >= threshold)
      if (!(*fault = !((index = avm_recoverable_natural (column_number - 1)) ? 1 : !(column_number - 1))))
	{
	  item = avm_list_of_value((void *) &x, sizeof(double), fault);
	  if (*fault)
	    {
	      if (index)
		avm_dispose (index);
	    }
	  else if (!(*fault = !(item = avm_recoverable_join (index,item))))
	    {
	      avm_recoverable_enqueue(&front, &back, item, fault);
	      item = NULL;
	    }
	}
  if (*fault ? front : NULL)
    avm_dispose (front);
  return (*fault ? (item ? item : avm_copied (memory_overflow)) : front);
}









static list
solution(simplex, operand, fault)
     int simplex;
     list operand;
     int *fault;

     /* operand should represent a triple of (c,m,y), where c is a
	list of cost function coefficients (with no constant term) m
	is a sparse matrix in the form of a list of pairs ((i,j),a)
	where i and j are row and column indices starting from 0 and a
	is real, and y is a list of reals such that the problem
	solution x minimizes cx subject to mx=y and all members of x
	non-negative. The list of indices and values (i,xi) for
	non-zero reals xi in the solution is returned. */
{
  list result;
  LPX *lp;
  int rows,columns;

  if (*fault)
    return NULL;
  if (*fault = !(operand ? (operand->tail) : NULL))
    return avm_copied(bad_glpk_spec);
  result = NULL;
  lp = problem_object (operand->head, operand->tail->head, operand->tail->tail, &rows, &columns, &result, fault);
  if (*fault = (*fault ? 1 : !!result))
    return result;
  avm_turn_off_stdout ();
  if ((simplex ? lpx_simplex(lp) : lpx_interior(lp)) == LPX_E_OK)
    result = (simplex ? simplex_solution(lp, columns, fault) : interior_solution (lp, rows, columns, fault));
  avm_turn_on_stdout ();
  avm_manage_memory ();
  lpx_delete_prob (lp);
  avm_free_managed_memory ();
  return result;
}








#endif




list
avm_have_glpk_call (list function_name, int *fault)

/* this reports the availability of a function */

{
#if HAVE_GLPK
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_glpk ();
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
avm_glpk_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;
{
#if HAVE_GLPK

  list message;
  int function_number;

  if (*fault)
    return NULL;
  if (! initialized)
    avm_initialize_glpk ();
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
    case 1: return solution (0, argument, fault);
    case 2: return solution (1, argument, fault);
    }
#endif /* HAVE_GLPK */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}







void
avm_initialize_glpk ()

     /* This initializes some static data structures. */

{
  char *funames[] = {
    "interior",
    "simplex",
    NULL};            /* add more function names here up to a total of 255 */
  list back;
  int string_number;

  if (initialized)
      return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_chrcodes ();
  avm_initialize_mwrap ();
  wild = avm_strung("*");
  bad_glpk_spec = avm_join (avm_strung ("bad glpk problem specification"), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized glpk function name"), NULL);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
      avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
}







void
avm_count_glpk ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
      return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (bad_glpk_spec);
  avm_dispose (memory_overflow);
  avm_dispose (unrecognized_function_name);
  funs = NULL;
  wild = NULL;
  bad_glpk_spec = NULL;
  memory_overflow = NULL;
  unrecognized_function_name = NULL;
}
