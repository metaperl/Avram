
/* this file interfaces to linear programming routines from lp_solve

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
#include <avm/lpsolve.h>
#include <avm/apply.h>
#if HAVE_LPSOLVE
#include <lpsolve/lp_lib.h>
#endif

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list bad_lpsolve_spec = NULL;
static list memory_overflow = NULL;
static list unrecognized_function_name = NULL;

/* virtual code program equivalent to guard\<'bad lpsolve specification'>! nleq-<&hll+ nleq-<&lr*+ |=&ll */
static list sorter = NULL;

/* list of the floating point number zero */
static list zero = NULL;

/* function names as lists of lists of character representations */
static list wild = NULL;
static list funs = NULL;





#if HAVE_LPSOLVE



static void
filled(lp, a, b, n, row, message, fault)
     lprec *lp;
     list a;
     list b;
     int n;
     double *row;
     list *message;
     int *fault;

     /* a is the constraint matrix in the form <<((i,j),x)..>..>,
	partitioned by rows, and sorted by columns and rows. b is the
	constraint vector, n is the maximum length of any row, and row
	is a vector of doubles big enough to hold any row. */
{
  int j;
  list t;
  int *col;
  double *y;

  col = NULL;
  if (*fault = (*fault ? 1 : !(col = (int *) malloc (n * sizeof(int)))))
    *message = (*message ? *message : avm_copied (memory_overflow));
  while (*fault ? 0 : a)
    {
      j = 0;
      t = a->head;
      a = a->tail;
      while (*fault ? 0 : t)
	if (!(t->head ? t->head->head : 0))       /* should have been established when sorting */
	  avm_internal_error (109);
	else if (*fault = (j >= n))
	  *message = avm_copied (bad_lpsolve_spec);
	else
	  {
	    col[j] = 1 + (int) avm_counter (t->head->head->tail);
	    y = (double *) avm_value_of_list (t->head->tail, message, fault);
	    row[j++] = *y;
	    t = t->tail;
	  }
      if (*fault = (*fault ? 1 : !b))
	*message = (*message ? *message : avm_copied (bad_lpsolve_spec));
      else
	y = (double *) avm_value_of_list (b->head, message, fault);
      if (!(*fault = (*fault ? 1 : !(add_constraintex(lp, j, row, col, EQ, *y)))))
	b = b->tail;
    }
  if (col)
    free (col);
}








static lprec
*problem_object(cost_vector, constraint_matrix, constraint_vector, rows, columns, message, fault)
     list cost_vector;
     list constraint_matrix;
     list constraint_vector;
     int *rows;
     int *columns;
     list *message;
     int *fault;

     /* This takes the lists specifying the problem to an lprec type
	problem object. */
{
  list y,x;
  double *row,*b;
  lprec *lp;

  *fault = !(cost_vector = avm_recoverable_join (avm_copied (zero), avm_copied (cost_vector)));
  constraint_matrix = (*fault ? NULL : avm_recoverable_apply (avm_copied (sorter), avm_copied (constraint_matrix), fault));
  *rows = avm_length (constraint_matrix);
  if (*fault)
    {
      *message = (constraint_matrix ? constraint_matrix : avm_copied (memory_overflow));
      avm_dispose (cost_vector);
      return NULL;
    }
  lp = make_lp (0, *columns = (int) avm_length (cost_vector));
  row = (double *) avm_vector_of_list (cost_vector, sizeof(double), message, fault);
  if (!(*fault = (*fault ? 1 : !(lp ? !!row  : 0))))
    if (!(*fault = !(set_obj_fn (lp, row))))
      set_add_rowmode(lp, TRUE);
  filled(lp, constraint_matrix, constraint_vector, *columns, row, message, fault);
  avm_dispose (constraint_matrix);
  avm_dispose (cost_vector);
  if (row)
    free (row);
  if (!*fault)
    {
      set_add_rowmode (lp, FALSE);
      set_verbose (lp, NEUTRAL);
      set_minim (lp);
      return lp;
    }
  if (lp)
    delete_lp (lp);
  return NULL;
}











static list
list_of_variables (x, rows, columns, fault)
     double *x;
     int rows;
     int columns;
     int *fault;

     /* extracts no more variables from x than the number of rows,
	putting them in a list <(i,xi)..> ignoring the zero or nearly
	zero values */
{
  list front, back, item, index;
  int column_number, non_zeros, threshold_index;
  double threshold;

  column_number = 0;
  front = back = item = NULL;
  while (*fault ? 0 : (column_number < columns))
    {
      if (x[column_number] != 0.0 ? 1 : x[column_number] != -0.0)
	if (!(*fault = !((index = avm_recoverable_natural (column_number)) ? 1 : !(column_number))))
	  {
	    item = avm_list_of_value((void *) &(x[column_number]), sizeof(double), fault);
	    if (*fault)
	      avm_dispose (index);
	    else if (!(*fault = !(item = avm_recoverable_join (index,item))))
	      {
		avm_recoverable_enqueue(&front, &back, item, fault);
		item = NULL;
	      }
	  }
      column_number++;
    }
  if (*fault ? front : NULL)
    avm_dispose (front);
  return (*fault ? (item ? item : avm_copied (memory_overflow)) : front);
}












static list
solution(operand, fault)
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
  lprec *lp;
  double *x;
  int rows,columns;

  if (*fault)
    return NULL;
  if (*fault = !(operand ? (operand->tail) : NULL))
    return avm_copied (bad_lpsolve_spec);
  result = NULL;
  lp = problem_object (operand->head, operand->tail->head, operand->tail->tail, &rows, &columns, &result, fault);
  if (*fault = (*fault ? 1 : !!result))
    return (result ? result : avm_copied (bad_lpsolve_spec));
  if (solve (lp) == OPTIMAL ? get_ptr_variables(lp, &x) : 0)
    result = list_of_variables (x, rows, columns, fault);
  if (lp)
    delete_lp (lp);
  return result;
}






static list
i_solution(operand, fault)
     list operand;
     int *fault;

     /* operand should represent a triple of (i,c,m,y), where i is a
        list of column numbers indicating the integer variables, and the
        remaining components are as in the solution function. */
{
  list result,i,cmy;
  lprec *lp;
  double *x;
  int rows,columns;

  if (*fault)
    return NULL;
  if (*fault = !(operand ? ((cmy = operand->tail) ? (operand->tail->tail) : NULL) : NULL))
    return avm_copied (bad_lpsolve_spec);
  result = NULL;
  lp = problem_object (cmy->head, cmy->tail->head, cmy->tail->tail, &rows, &columns, &result, fault);
  if (*fault = (*fault ? 1 : !!result))
    return (result ? result : avm_copied (bad_lpsolve_spec));
  i = operand->head;
  while (*fault ? 0 : !!i)
    {
      *fault = ! set_int (lp, 1 + (int) avm_counter (i->head), 1);
      i = i->tail;
    }
  if (*fault)
    return avm_copied (bad_lpsolve_spec);
  if (solve (lp) == OPTIMAL ? get_ptr_variables(lp, &x) : 0)
    result = list_of_variables (x, rows, columns, fault);
  if (lp)
    delete_lp (lp);
  return result;
}






static list
b_solution(operand, fault)
     list operand;
     int *fault;

     /* operand should represent a triple of (b,c,m,y), where b is a
        list of column numbers indicating the binary variables, and the
        remaining components are as in the solution function. */
{
  list result,b,cmy;
  lprec *lp;
  double *x;
  int rows,columns;

  if (*fault)
    return NULL;
  if (*fault = !(operand ? ((cmy = operand->tail) ? (operand->tail->tail) : NULL) : NULL))
    return avm_copied (bad_lpsolve_spec);
  result = NULL;
  lp = problem_object (cmy->head, cmy->tail->head, cmy->tail->tail, &rows, &columns, &result, fault);
  if (*fault = (*fault ? 1 : !!result))
    return (result ? result : avm_copied (bad_lpsolve_spec));
  b = operand->head;
  while (*fault ? 0 : !!b)
    {
      *fault = ! set_binary (lp, 1 + (int) avm_counter (b->head), 1);
      b = b->tail;
    }
  if (*fault)
    return avm_copied (bad_lpsolve_spec);
  if (solve (lp) == OPTIMAL ? get_ptr_variables(lp, &x) : 0)
    result = list_of_variables (x, rows, columns, fault);
  if (lp)
    delete_lp (lp);
  return result;
}






static list
bi_solution(operand, fault)
     list operand;
     int *fault;

     /* operand should represent a triple of ((b,i),c,m,y), where b is a
        list of column numbers indicating the binary variables, and the
        remaining components are as in the solution function. */
{
  list result,b,i,cmy;
  lprec *lp;
  double *x;
  int rows,columns;

  if (*fault)
    return NULL;
  if (*fault = !(operand ? ((cmy = operand->tail) ? ((operand->tail->head) ? (operand->tail->tail) : NULL) : NULL) : NULL))
    return avm_copied (bad_lpsolve_spec);
  result = NULL;
  lp = problem_object (cmy->head, cmy->tail->head, cmy->tail->tail, &rows, &columns, &result, fault);
  if (*fault = (*fault ? 1 : !!result))
    return (result ? result : avm_copied (bad_lpsolve_spec));
  b = operand->head->head;
  while (*fault ? 0 : !!b)
    {
      *fault = ! set_binary (lp, 1 + (int) avm_counter (b->head), 1);
      b = b->tail;
    }
  i = operand->head->tail;
  while (*fault ? 0 : !!i)
    {
      *fault = ! set_int (lp, 1 + (int) avm_counter (i->head), 1);
      i = i->tail;
    }
  if (*fault)
    return avm_copied (bad_lpsolve_spec);
  if (solve (lp) == OPTIMAL ? get_ptr_variables(lp, &x) : 0)
    result = list_of_variables (x, rows, columns, fault);
  if (lp)
    delete_lp (lp);
  return result;
}




#endif




list
avm_have_lpsolve_call (list function_name, int *fault)

/* this reports the availability of a function */

{
#if HAVE_LPSOLVE
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_lpsolve ();
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
avm_lpsolve_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;
{
#if HAVE_LPSOLVE

  list message;
  int function_number;

  if (*fault)
    return NULL;
  if (! initialized)
    avm_initialize_lpsolve ();
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
    case 2: return i_solution (argument, fault);
    case 3: return b_solution (argument, fault);
    case 4: return bi_solution (argument, fault);
    }
#endif /* HAVE_LPSOLVE */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}







void
avm_initialize_lpsolve ()

     /* This initializes some static data structures. */

{
char *sorter_code = "wOsU{mm{A^[wl=krH[{U`>AwkMf[s<ynl[_cjlE]Zye{J<atsr`\
yanSs{@yJyPjWoxKV_QlKHFAzDhv\\\\PJZv\\d[c=htKV[y_]{er@_[VNy@dL[yfOQ{mKOz>=Cv`nd\
OxKV^gIPDcxf<>GJdD<Bh\\hS\\=lWJJf<czcTNvxH[yfOSGN<EzeHf@`=dAxf<PNUx<PDhRL<aNJ<";

  char *funames[] = {
    "stdform",
    "iform",
    "bform",
    "biform",
    NULL};            /* add more function names here up to a total of 255 */
  list back;
  int string_number,fault;

  if (initialized)
      return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_chrcodes ();
  avm_initialize_apply ();
  wild = avm_strung("*");
  zero = avm_scanned_list ("wgfzg]ftVjBg=f]fB]\\");
  bad_lpsolve_spec = avm_join (avm_strung ("bad lpsolve problem specification"), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized lpsolve function name"), NULL);
  sorter = avm_scanned_list (sorter_code);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
      avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
}







void
avm_count_lpsolve ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
      return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (zero);
  avm_dispose (sorter);
  avm_dispose (bad_lpsolve_spec);
  avm_dispose (memory_overflow);
  avm_dispose (unrecognized_function_name);
  sorter = NULL;
  funs = NULL;
  wild = NULL;
  bad_lpsolve_spec = NULL;
  memory_overflow = NULL;
  unrecognized_function_name = NULL;
}
