
/* This file interfaces to some non-linear optimization functions from
   minpack. It needs the minpack c header file, which might be Debian
   specific because the upstream source is in Fortran, or else the
   configuration script will disable it. Best to get the header from
   somewhere and try recompiling if necessary.

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
#include <avm/apply.h>
#include <avm/matcon.h>
#include <avm/chrcodes.h>
#include <avm/minpack.h>
#if HAVE_MINPACK
#include <math.h>
#include <minpack.h>
#endif

/* points to a stack of function specifications */
typedef struct spec_stack_node *spec_stack;

/* a stack of these is needed for re-entrancy */
struct spec_stack_node
{
  list fcn;       /* computes the function being optimized */
  list jac;       /* computes the jacobian */
  int fault;
  list message;
  double *origin;
  list *row_number;
  int number_of_outputs;     /* the output vector length of fcn, in case it's less than the input length */
  spec_stack other_specs;
};

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list minpack_error = NULL;
static list memory_overflow = NULL;
static list bad_minpack_spec = NULL;
static list unrecognized_function_name = NULL;

/* the stack of function specifications whose top is referenced globally by minpack functions */
static spec_stack top = NULL;

/* function names as lists of lists of character representations */
static list wild = NULL;
static list funs = NULL;

#if HAVE_MINPACK

static list avm_lmder (list, int *);
static list avm_lmdif (list, int *);
static list avm_lmstr (list, int *);
static list avm_hybrd (list, int *);
static list avm_hybrj (list, int *);

/* the tightest tolerance worth trying */
#define MINIMUM_TOLERANCE 1E-15

/* the number of retries at bigger tolerances before giving up due to slow convergence */
#define TIME_LIMIT 25

/* the factor by which the tolerance is increased on each attempt */
#define MAGNIFIER 4.64158883361278

#define freeif(x) if (x)			\
    free (x)





static spec_stack
new_top()

     /* returns a pointer to the new top of the stack if it can be allocated */
{
  spec_stack next_top;

  if (!(next_top = (spec_stack) malloc (sizeof(*next_top))))
    return NULL;
  memset (next_top, 0, sizeof(*next_top));
  next_top->other_specs = top;
  return (top = next_top);
}








static void
pop_spec()

/* gets rid of the spec on the top of the stack */

{
  spec_stack previous_top;

  if (!top)
    avm_internal_error (2);
  top = (previous_top = top)->other_specs;
  free (previous_top);
}









static void
lmder_fcn(m,n,x,fvec,fjac,ldfjac,iflag)
     int *m;
     int *n;
     double *x;
     double *fvec;
     double *fjac;
     int *ldfjac;
     int *iflag;

     /* the c function to be passed to the minpack lmder function;
	evaluates the function in the global variable top->fcn which
	is expected to take a list of n reals to a list of m reals, or
	evaluates the function in the global variable top->jac, which
	expected to take a list of n reals to a matrix with m rows and
	n columns */
{
  list operand,result,row,col;
  int i,j;
  double *item;

  operand = (top->fault ? NULL : avm_list_of_vector((void *) x,*n,sizeof(double),&(top->fault)));
  if (top->fault)
    {
      top->message = (top->message ? top->message : operand);
      *iflag = -1;
      return;
    }
  row = result = avm_recoverable_apply(avm_copied((*iflag == 1) ? top->fcn : top->jac),operand,&(top->fault));
  if (top->fault)
    {
      top->message = result;
      *iflag = -1;
      return;
    }
  i = 0;
  operand = NULL;
  while (top->fault ? 0 : (i < *m))
    {
      if (*iflag == 1)
	{
	  item = ((top->fault = !row) ? NULL : (double *) avm_value_of_list(row->head,&operand,&(top->fault)));
	  fvec[i] = ((top->fault) ? 0.0 : (*item - top->origin[i]));
	}
      else if (!((top->fault) = !row))
	{
	  j = 0;
	  col = row->head;
	  while ((top->fault) ? 0 : (j < *n))
	    {
	      item = ((top->fault = !col) ? NULL : (double *) avm_value_of_list(col->head,&operand,&(top->fault)));
	      fjac[(j++ * *m) + i] = (top->fault ? 0.0 : *item);
	      col = (top->fault ? col : col->tail);
	    }
	  top->fault = (top->fault ? 1 : !!col);
	}
      row = (top->fault ? row : row->tail);
      i++;
    }
  avm_dispose (result);
  if (!(top->fault = (top->fault ? 1 : !!row)))
    return;
  top->message = (operand ? operand : avm_copied(bad_minpack_spec));
  *iflag = -1;
  return;
}







static list
avm_lmder (operand, fault)
     list operand;
     int *fault;

     /* the operand represents ((f,j),x,y) where f and j are functions
	and x and y are lists of reals of length n and m respectively.
	y is the preferred output of f, not necessarily 0, and x is
	the initial estimate of the input. j is a function that takes
	a list of reals to the jacobian of f represented as a list of
	rows. The jacobian is a matrix whose ith row is the list of
	partial derivatives of the ith output component of f with
	respect to each input component. The result returned is a more
	accurate estimate of the input if one is found, but is empty
	otherwise. Although a different algorithm is used, this
	interface is the same as that of avm_hybrj except that the
	output list may be longer than the input. If the output is
	shorter than the input, (i.e., m < n), this function calls
	avm_hybrj instead. */
{
  int m;
  int n;
  double *x;
  double *fvec;
  double *fjac;
  int ldfjac;
  double ftol;
  double xtol;
  double gtol;
  int maxfev;
  double *diag;
  int mode;
  double factor;
  int nprint;
  int info;
  int nfev;
  int njev;
  int *ipvt;
  double *qtf;
  double *wa1;
  double *wa2;
  double *wa3;
  double *wa4;
  list result;
  int tries;
  spec_stack top;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(operand ? (operand->head ? (operand->head->head ? (operand->head->tail ? !!(operand->tail):0):0):0):0))
    return avm_copied (bad_minpack_spec);
  m = (int) avm_length(operand->tail->tail);
  n = (int) avm_length(operand->tail->head);
  if (*fault = !(m ? n : 0))
    return avm_copied (bad_minpack_spec);
  if (n > m)
    return avm_hybrj (operand, fault);
  if (!(top = new_top()))
    return avm_copied (memory_overflow);
  top->fcn = operand->head->head;
  top->jac = operand->head->tail;
  x = (double *) avm_vector_of_list(operand->tail->head,sizeof(double),&result,fault);
  fvec = (double *) malloc(sizeof(double) * m);
  fjac = (double *) malloc(sizeof(double) * m * n);
  ldfjac = m;
  ftol = MINIMUM_TOLERANCE;
  xtol = MINIMUM_TOLERANCE;
  gtol = 0.0;
  maxfev = 100 * (n + 1);
  diag = (double *) malloc(sizeof(double) * n);
  mode = 1;
  factor = 100.0;
  nprint = 0;
  ipvt = (int *) malloc(sizeof(int) * n);
  qtf = (double *) malloc(sizeof(double) * n);
  wa1 = (double *) malloc(sizeof(double) * n);
  wa2 = (double *) malloc(sizeof(double) * n);
  wa3 = (double *) malloc(sizeof(double) * n);
  wa4 = (double *) malloc(sizeof(double) * m);
  top->origin = (*fault ? NULL : (double *) avm_vector_of_list(operand->tail->tail,sizeof(double),&result,fault));
  if (!*fault)
    *fault = !(x? (fvec? (fjac? (diag? (ipvt? (qtf? (wa1? (wa2? (wa3? (wa4 ? !!(top->origin):0):0):0):0):0):0):0):0):0):0);
  top->message = NULL;
  tries = info = top->fault = 0;
  while (*fault ? 0 : (!((info == 1) ? 1 : ((info == 2) ? 1 : (info == 3))) ? (tries++ < TIME_LIMIT) : 0))
    {
      lmder_ (&lmder_fcn, &m, &n, x, fvec, fjac, &ldfjac, &ftol, &xtol, &gtol, &maxfev, diag, &mode, &factor, &nprint, &info,
	      &nfev, &njev, ipvt, qtf, wa1, wa2, wa3, wa4);
      if (!info)
	avm_internal_error (102);
      if (*fault = (top->fault ? 1 : !!(top->message)))
	{
	  if (result ? top->message : NULL)
	    avm_dispose (top->message);
	  else
	    result = (top->message ? top->message : avm_copied(bad_minpack_spec));
	}
      ftol = ftol * MAGNIFIER;
      xtol = xtol * MAGNIFIER;
    }
  freeif (fvec);
  freeif (fjac);
  freeif (diag);
  freeif (ipvt);
  freeif (qtf);
  freeif (wa1);
  freeif (wa2);
  freeif (wa3);
  freeif (wa4);
  freeif (top->origin);
  pop_spec ();
  if (*fault ? 0 : ((info == 1) ? 1 : ((info == 2) ? 1 : (info == 3))))
    result = avm_list_of_vector((void *) x,n,sizeof(double),fault);
  freeif (x);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}











static void
lmstr_fcn(m,n,x,fvec,fjrow,iflag)
     int *m;
     int *n;
     double *x;
     double *fvec;
     double *fjrow;
     int *iflag;

     /* the c function to be passed to the minpack lmstr function;
	evaluates the function in the global variable top->fcn, which
	is expected to take a list of n reals to a list of m reals, or
	evaluates the function in the global variable top->jac, which
	is expected to take a pair (i,<x1..xn>) of a natural in the
	range 0..m-1 and a list of n reals to a list of n reals; also
	assumes the global variable top->row_number has been
	initialized at least to size m */
{
  list operand,result,row;
  int i;
  double *item;

  if (top->fault = (top->fault ? 1 : ((*iflag <= 0) ? 1 : (*iflag > (*m + 1)))))
    {
      top->message = (top->message ? top->message : avm_copied(minpack_error));
      *iflag = -1;
      return;
    }
  operand = avm_list_of_vector((void *) x,*n,sizeof(double),&(top->fault));
  if (top->fault ? 0 : (*iflag > 1))
    operand = avm_recoverable_join(avm_copied(top->row_number[(*iflag) - 2]),operand);
  if (top->fault = (top->fault ? 1 : !operand))
    {
      top->message = (operand ? operand : avm_copied(memory_overflow));
      *iflag = -1;
      return;
    }
  row = result = avm_recoverable_apply(avm_copied((*iflag == 1) ? top->fcn : top->jac), operand, &(top->fault));
  if (top->fault)
    {
      top->message = result;
      *iflag = -1;
      return;
    }
  i = 0;
  operand = NULL;
  while (top->fault ? 0 : (i < ((*iflag == 1) ? *m : *n)))
    {
      item = ((top->fault = !row) ? NULL : (double *) avm_value_of_list(row->head,&operand,&(top->fault)));
      if (*iflag == 1)
	fvec[i] = (top->fault ? 0.0 : (*item - top->origin[i]));
      else
	fjrow[i] = (top->fault ? 0.0 : *item);
      row = (top->fault ? row : row->tail);
      i++;
    }
  avm_dispose (result);
  if (!(top->fault = (top->fault ? 1 : !!row)))
    return;
  top->message = (operand ? operand : avm_copied(bad_minpack_spec));
  *iflag = -1;
  return;
}






static list
avm_lmstr (operand, fault)
     list operand;
     int *fault;

     /* the operand represents ((f,j),x,y) where f and j are functions
        and x and y are lists of reals of length n and m respectively.
        y is the preferred output of f, not necessarily 0, and x is
        the initial estimate of the input. j is a function that takes
        row number and a list of reals to the corresponding row of the
        jacobian of f. The jacobian is a matrix whose ith row is the
        list of partial derivatives of the ith output component of f
        with respect to each input component. The result returned is a
        more accurate estimate of the input if one is found, but is
        empty otherwise. This interface is the same as that of lmder
        except that jacobian function has a different calling
        convention allowing less memory to be used, appropriately for
        problems with a very large output vector and a moderate sized
        input vector. It's an error for m to be less than n. */
{
  int m;
  int n;
  double *x;
  double *fvec;
  double *fjac;
  int ldfjac;
  double ftol;
  double xtol;
  double gtol;
  int maxfev;
  double *diag;
  int mode;
  double factor;
  int nprint;
  int info;
  int nfev;
  int njev;
  int *ipvt;
  double *qtf;
  double *wa1;
  double *wa2;
  double *wa3;
  double *wa4;
  list result;
  int tries;
  int i;
  spec_stack top;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(operand ? (operand->head ? (operand->head->head ? (operand->head->tail ? !!(operand->tail):0):0):0):0))
    return avm_copied (bad_minpack_spec);
  m = (int) avm_length(operand->tail->tail);
  n = (int) avm_length(operand->tail->head);
  if (*fault = !(m ? (n ? (n <= m) : 0) : 0))
    return avm_copied (bad_minpack_spec);
  if (!(top = new_top()))
    return avm_copied (memory_overflow);
  top->fcn = operand->head->head;
  top->jac = operand->head->tail;
  x = (double *) avm_vector_of_list(operand->tail->head,sizeof(double),&result,fault);
  fvec = (double *) malloc(sizeof(double) * m);
  fjac = (double *) malloc(sizeof(double) * m * n);
  ldfjac = m;
  ftol = MINIMUM_TOLERANCE;
  xtol = MINIMUM_TOLERANCE;
  gtol = 0.0;
  maxfev = 100 * (n + 1);
  diag = (double *) malloc(sizeof(double) * n);
  mode = 1;
  factor = 100.0;
  nprint = 0;
  ipvt = (int *) malloc(sizeof(int) * n);
  qtf = (double *) malloc(sizeof(double) * n);
  wa1 = (double *) malloc(sizeof(double) * n);
  wa2 = (double *) malloc(sizeof(double) * n);
  wa3 = (double *) malloc(sizeof(double) * n);
  wa4 = (double *) malloc(sizeof(double) * m);
  top->origin = (*fault ? NULL : (double *) avm_vector_of_list(operand->tail->tail,sizeof(double),&result,fault));
  top->row_number = (*fault ? NULL : avm_row_number_array(m,fault));
  if (!(*fault = (*fault ? 1 : !(top->origin ? top->row_number : NULL))))
    *fault = !(x ? (fvec ? (fjac ? (diag ? (ipvt ? (qtf ? (wa1 ? (wa2 ? (wa3 ? !!wa4 : 0):0):0):0):0):0):0):0):0);
  top->message = NULL;
  tries = info = top->fault = 0;
  while (*fault ? 0 : (!((info == 1) ? 1 : ((info == 2) ? 1 : (info == 3))) ? (tries++ < TIME_LIMIT) : 0))
    {
      top->fault = 0;
      top->message = NULL;
      lmstr_ (&lmstr_fcn, &m, &n, x, fvec, fjac, &ldfjac, &ftol, &xtol, &gtol, &maxfev, diag, &mode, &factor, &nprint, &info,
	      &nfev, &njev, ipvt, qtf, wa1, wa2, wa3, wa4);
      if (!info)
	avm_internal_error (103);
      if (*fault = (top->fault ? 1 : !!(top->message)))
	{
	  if (result ? top->message : NULL)
	    avm_dispose (top->message);
	  else
	    result = (top->message ? top->message : avm_copied(bad_minpack_spec));
	}
      xtol = xtol * MAGNIFIER;
      ftol = ftol * MAGNIFIER;
    }
  if(top->row_number)
      avm_dispose_rows(m,top->row_number);
  freeif (fvec);
  freeif (fjac);
  freeif (diag);
  freeif (ipvt);
  freeif (qtf);
  freeif (wa1);
  freeif (wa2);
  freeif (wa3);
  freeif (wa4);
  freeif (top->origin);
  pop_spec ();
  if (*fault ? 0 : ((info == 1) ? 1 : ((info == 2) ? 1 : (info == 3))))
    result = avm_list_of_vector((void *) x,n,sizeof(double),fault);
  freeif (x);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}







static void
lmdif_fcn(m,n,x,fvec,iflag)
     int *m;
     int *n;
     double *x;
     double *fvec;
     int *iflag;

     /* the c function to be passed to the minpack lmdif function;
	evaluates the function in the global variable top->fcn, which is
	expected to take a list of n reals to a list of m reals */
{
  list operand,result,row;
  int i;
  double *item;

  operand = (top->fault ? NULL : avm_list_of_vector((void *) x,*n,sizeof(double),&(top->fault)));
  if (top->fault)
    {
      top->message = (top->message ? top->message : operand);
      *iflag = -1;
      return;
    }
  row = result = avm_recoverable_apply(avm_copied(top->fcn),operand,&(top->fault));
  if (top->fault)
    {
      top->message = result;
      *iflag = -1;
      return;
    }
  operand = NULL;
  i = 0;
  while (top->fault ? 0 : (i < *m))
    {
      item = ((top->fault = !row) ? NULL : (double *) avm_value_of_list(row->head,&operand,&(top->fault)));
      fvec[i] = (top->fault ? 0.0 : (*item - top->origin[i]));
      row = (top->fault ? row : row->tail);
      i++;
    }
  avm_dispose (result);
  if (!(top->fault = (top->fault ? 1 : !!row)))
    return;
  top->message = (operand ? operand : avm_copied(bad_minpack_spec));
  *iflag = -1;
  return;
}






static list
avm_lmdif (operand, fault)
     list operand;
     int *fault;

     /* the operand represents (f,x,y) where f is a function and x and
        y are lists of reals of length n and m respectively.  y is the
        preferred output of f, not necessarily 0, and x is the initial
        estimate of the input. The result returned is a more accurate
        estimate of the input if one is found, but is empty otherwise.
        This function differs from lmder because no jacobian is
        specified, which may make it less efficient or less accurate.
        If y is shorter than x (i.e., m < n), this function calls
        avm_hybrd instead. */
{
  int m;
  int n;
  double *x;
  double *fvec;
  double ftol;
  double xtol;
  double gtol;
  int maxfev;
  double epsfcn;
  double *diag;
  int mode;
  double factor;
  int nprint;
  int info;
  int nfev;
  double *fjac;
  int ldfjac;
  int *ipvt;
  double *qtf;
  double *wa1;
  double *wa2;
  double *wa3;
  double *wa4;
  list result;
  int tries;
  spec_stack top;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(operand ? (operand->head ? !!(operand->tail) : 0) : 0))
    return avm_copied (bad_minpack_spec);
  m = (int) avm_length(operand->tail->tail);
  n = (int) avm_length(operand->tail->head);
  if (*fault = !(m ? n : 0))
    return avm_copied (bad_minpack_spec);
  if (n > m)
    return avm_hybrd (operand, fault);
  if (!(top = new_top()))
    return avm_copied (memory_overflow);
  top->fcn = operand->head;
  x = (double *) avm_vector_of_list(operand->tail->head,sizeof(double),&result,fault);
  fvec = (double *) malloc(sizeof(double) * m);
  ftol = MINIMUM_TOLERANCE;
  xtol = MINIMUM_TOLERANCE;
  gtol = 0.0;
  maxfev = 100 * (n + 1);
  epsfcn = 0.0;
  diag = (double *) malloc(sizeof(double) * n);
  mode = 1;
  factor = 100.0;
  nprint = 0;
  fjac = (double *) malloc(sizeof(double) * n * m);
  ldfjac = m;
  ipvt = (int *) malloc(sizeof(int) * n);
  qtf = (double *) malloc(sizeof(double) * n);
  wa1 = (double *) malloc(sizeof(double) * n);
  wa2 = (double *) malloc(sizeof(double) * n);
  wa3 = (double *) malloc(sizeof(double) * n);
  wa4 = (double *) malloc(sizeof(double) * m);
  top->origin = (*fault ? NULL : (double *) avm_vector_of_list(operand->tail->tail,sizeof(double),&result,fault));
  if (!*fault)
    *fault = !(x? (fvec? (diag? (fjac? (ipvt? (qtf? (wa1? (wa2? (wa3? (wa4? !!(top->origin):0):0):0):0):0):0):0):0):0):0);
  top->message = NULL;
  tries = info = top->fault = 0;
  while (*fault ? 0 : (!((info == 1) ? 1 : ((info == 2) ? 1 : (info == 3))) ? (tries++ < TIME_LIMIT) : 0))
    {
      lmdif_ (&lmdif_fcn, &m, &n, x, fvec, &ftol, &xtol, &gtol, &maxfev, &epsfcn,diag, &mode, &factor, &nprint, &info, &nfev,
	      fjac, &ldfjac, ipvt, qtf, wa1, wa2, wa3, wa4);
      if (!info)
	avm_internal_error (104);
      if (*fault = (top->fault ? 1 : !!(top->message)))
	{
	  if (result ? top->message : NULL)
	    avm_dispose (top->message);
	  else
	    result = (top->message ? top->message : avm_copied(bad_minpack_spec));
	}
      xtol = xtol * MAGNIFIER;
      ftol = ftol * MAGNIFIER;
    }
  freeif (fvec);
  freeif (fjac);
  freeif (diag);
  freeif (ipvt);
  freeif (qtf);
  freeif (wa1);
  freeif (wa2);
  freeif (wa3);
  freeif (wa4);
  freeif (top->origin);
  pop_spec ();
  if (*fault ? 0 : ((info == 1) ? 1 : ((info == 2) ? 1 : (info == 3))))
    result = avm_list_of_vector((void *) x,n,sizeof(double),fault);
  freeif (x);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}






static void
hybrd_fcn(n,x,fvec,iflag)
     int *n;
     double *x;
     double *fvec;
     int *iflag;

     /* the c function to be passed to the minpack hybrd function;
	evaluates the function described by the global variable
	top->fcn, which is expected to take a list of n reals to a
	list of at most n reals, and passes the difference between
	that result and the global variable top->origin to minpack. If
	the output length is less than n, the vector passed to minpack
	is padded with zeros. The global variable
	top->number_of_outputs must match the actual output list
	length. */
{
  list operand,result,row;
  int i;
  double *item;

  operand = (top->fault ? NULL : avm_list_of_vector((void *) x,*n,sizeof(double),&(top->fault)));
  if (top->fault)
    {
      top->message = (top->message ? top->message : operand);
      *iflag = -1;
      return;
    }
  row = result = avm_recoverable_apply(avm_copied(top->fcn),operand,&(top->fault));
  if (top->fault)
    {
      top->message = result;
      *iflag = -1;
      return;
    }
  operand = NULL;
  i = 0;
  while (top->fault ? 0 : (i < top->number_of_outputs))
    {
      item = ((top->fault = !row) ? NULL : (double *) avm_value_of_list(row->head,&operand,&(top->fault)));
      fvec[i] = (top->fault ? 0.0 : (*item - top->origin[i]));
      row = (top->fault ? row : row->tail);
      i++;
    }
  avm_dispose (result);
  while (i < *n)
    fvec[i++] = 0.0;
  if (!(top->fault = (top->fault ? 1 : !!row)))
    return;
  top->message = (operand ? operand : avm_copied(bad_minpack_spec));
  *iflag = -1;
  return;
}







static list
avm_hybrd (operand, fault)
     list operand;
     int *fault;

     /* the operand represents (f,x,y) where f is a function and x and
	y are lists of reals. y is the preferred output of f, not
	necessarily 0, and x is the initial estimate of the input. The
	result returned is a more accurate estimate of the input
	consistent with the given output if one is found, but is empty
	otherwise. If the output list is longer than the input list,
	avm_lmdif is called instead, and if it's shorter, it's
	padded automatically. */
{
  int n;
  double *x;
  double *fvec;
  double xtol;
  int maxfev;
  int ml;
  int mu;
  double epsfcn;
  double *diag;
  int mode;
  double factor;
  int nprint;
  int info;
  int nfev;
  double *fjac;
  int ldfjac;
  double *r;
  int lr;
  double *qtf;
  double *wa1;
  double *wa2;
  double *wa3;
  double *wa4;
  list result;
  int tries;
  spec_stack top;
  int number_of_outputs;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(operand ? (operand->head ? operand->tail : NULL) : NULL))
    return avm_copied (bad_minpack_spec);
  n = (int) avm_length (operand->tail->head);
  if (n < (number_of_outputs = (int) avm_length (operand->tail->tail)))
    return avm_lmdif (operand, fault);
  if (!(top = new_top()))
    return avm_copied (memory_overflow);
  top->number_of_outputs = number_of_outputs;
  top->fcn = operand->head;
  x = (double *) avm_vector_of_list(operand->tail->head,sizeof(double),&result,fault);
  fvec = (double *) malloc(sizeof(double) * n);
  xtol = MINIMUM_TOLERANCE;
  maxfev = 200 * (n + 1);
  ml = n - 1;
  mu = n - 1;
  epsfcn = 0.0;
  diag = (double *) malloc(sizeof(double) * n);
  mode = 1;
  factor = 100.0;
  nprint = 0;
  ldfjac = n;
  fjac = (double *) malloc(sizeof(double) * ldfjac * n);
  lr = (n * (n + 1)) / 2;
  r = (double *) malloc(sizeof(double) * lr);
  qtf = (double *) malloc(sizeof(double) * n);
  wa1 = (double *) malloc(sizeof(double) * n);
  wa2 = (double *) malloc(sizeof(double) * n);
  wa3 = (double *) malloc(sizeof(double) * n);
  wa4 = (double *) malloc(sizeof(double) * n);
  top->origin = (*fault ? NULL : (double *) avm_vector_of_list(operand->tail->tail,sizeof(double),&result,fault));
  if (!*fault)
    *fault = !(x? (fvec? (diag? (fjac? (r? (qtf? (wa1? (wa2? (wa3? (wa4? !!(top->origin):0):0):0):0):0):0):0):0):0):0);
  top->message = NULL;
  tries = info = top->fault = 0;
  while (*fault ? 0 : ((info != 1) ? (tries++ < TIME_LIMIT) : 0))
    {
      hybrd_ (&hybrd_fcn, &n, x, fvec, &xtol, &maxfev, &ml, &mu, &epsfcn, diag, &mode, &factor, &nprint, &info, &nfev, fjac,
	      &ldfjac, r, &lr, qtf, wa1, wa2, wa3, wa4);
      if (!info)
	avm_internal_error (100);
      if (*fault = (top->fault ? 1 : !!(top->message)))
	{
	  if (result ? top->message : NULL)
	    avm_dispose (top->message);
	  else
	    result = (top->message ? top->message : avm_copied(bad_minpack_spec));
	}
      xtol = xtol * MAGNIFIER;
    }
  freeif (fvec);
  freeif (diag);
  freeif (fjac);
  freeif (r);
  freeif (qtf);
  freeif (wa1);
  freeif (wa2);
  freeif (wa3);
  freeif (wa4);
  freeif (top->origin);
  pop_spec ();
  if (*fault ? 0 : (info == 1))
    result = avm_list_of_vector((void *) x,n,sizeof(double),fault);
  freeif (x);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}








static void
hybrj_fcn(n,x,fvec,fjac,ldfjac,iflag)
     int *n;
     double *x;
     double *fvec;
     double *fjac;
     int *ldfjac;
     int *iflag;

     /* the c function to be passed to the minpack hybrj function;
	evaluates the function described by the global variable
	top->fcn, which is expected to take a list of n reals to a
	list of at most n reals, or evaluates the function described
	by the global variable top->jac, which is expected to take a
	list of n reals to list of n lists of at most n reals. If the
	actual output length is less than n, it must match the global
	variable top->number_of_outputs, and the arrays passed to
	minpack are padded with zeros. */
{
  list operand,result,row,col;
  int i,j;
  double *item;

  operand = (top->fault ? NULL : avm_list_of_vector((void *) x,*n,sizeof(double),&(top->fault)));
  if (top->fault)
    {
      top->message = (top->message ? top->message : operand);
      *iflag = -1;
      return;
    }
  row = result = avm_recoverable_apply(avm_copied((*iflag == 1) ? top->fcn : top->jac), operand, &(top->fault));
  if (top->fault)
    {
      top->message = result;
      *iflag = -1;
      return;
    }
  i = 0;
  operand = NULL;
  while (top->fault ? 0 : (i < top->number_of_outputs))
    {
      if (*iflag == 1)
	{
	  item = ((top->fault = !row) ? NULL : (double *) avm_value_of_list(row->head, &operand, &(top->fault)));
	  fvec[i] = (top->fault ? 0.0 : (*item - top->origin[i]));
	}
      else if (!(top->fault = !row))
	{
	  j = 0;
	  col = row->head;
	  while (top->fault ? 0 : (j < *n))
	    {
	      item = ((top->fault = !col) ? NULL : (double *) avm_value_of_list(col->head, &operand, &(top->fault)));
	      fjac[(j++ * *n) + i] = (top->fault ? 0.0 : *item);
	      col = (top->fault ? col : col->tail);
	    }
	  top->fault = (top->fault ? 1 : !!col);
	}
      row = (top->fault ? row : row->tail);
      i++;
    }
  while (i < *n)
    {
      if (*iflag == 1)
	fvec[i] = 0.0;
      else
	{
	  j = 0;
	  while (j < *n)
	    fjac[(j++ * *n) + i] = 0.0;
	}
      i++;
    }
  avm_dispose (result);
  if (!(top->fault = (top->fault ? 1 : !!row)))
    return;
  top->message = (operand ? operand : avm_copied (bad_minpack_spec));
  *iflag = -1;
  return;
}








static list
avm_hybrj (operand, fault)
     list operand;
     int *fault;

     /* the operand represents ((f,j),x,y) where f and j are functions
	and x and y are lists of reals of length n. y is the preferred
	output of f, not necessarily 0, and x is the initial estimate
	of the input. j is a function that takes a list of reals to
	the jacobian of f represented as a list of rows. The jacobian
	is a matrix whose ith row is the list of partial derivatives
	of the ith output component of f with respect to each input
	component. The result returned is a more accurate estimate of
	the input if one is found, but is empty otherwise. If the
	output list is longer than the input list, avm_lmder is called
	instead, and if it's shorter it's automatically padded. */
{
  int n;
  double *x;
  double *fvec;
  double *fjac;
  int ldfjac;
  double xtol;
  int maxfev;
  double *diag;
  int mode;
  double factor;
  int nprint;
  int info;
  int nfev;
  int njev;
  double *r;
  int lr;
  double *qtf;
  double *wa1;
  double *wa2;
  double *wa3;
  double *wa4;
  list result;
  int tries;
  spec_stack top;
  int number_of_outputs;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(operand ? (operand->head ? (operand->head->head ? (operand->head->tail ? !!(operand->tail):0):0):0):0))
    return avm_copied (bad_minpack_spec);
  n = (int) avm_length(operand->tail->head);
  if (n < (number_of_outputs = (int) avm_length (operand->tail->tail)))
    return avm_lmder (operand, fault);
  if (!(top = new_top()))
    return avm_copied (memory_overflow);
  top->fcn = operand->head->head;
  top->jac = operand->head->tail;
  top->number_of_outputs = number_of_outputs;
  x = (double *) avm_vector_of_list(operand->tail->head,sizeof(double),&result,fault);
  fvec = (double *) malloc(sizeof(double) * n);
  fjac = (double *) malloc(sizeof(double) * n * n);
  ldfjac = n;
  xtol = MINIMUM_TOLERANCE;
  maxfev = 200 * (n + 1);
  diag = (double *) malloc(sizeof(double) * n);
  mode = 1;
  factor = 100.0;
  nprint = 0;
  lr = (n * (n + 1))/2;
  r = (double *) malloc(sizeof(double) * lr);
  qtf = (double *) malloc(sizeof(double) * n);
  wa1 = (double *) malloc(sizeof(double) * n);
  wa2 = (double *) malloc(sizeof(double) * n);
  wa3 = (double *) malloc(sizeof(double) * n);
  wa4 = (double *) malloc(sizeof(double) * n);
  top->origin = (*fault ? NULL : (double *) avm_vector_of_list(operand->tail->tail,sizeof(double),&result,fault));
  if (!*fault)
    *fault = !(x? (fvec? (diag? (fjac? (r? (qtf? (wa1? (wa2? (wa3? (wa4? !!(top->origin):0):0):0):0):0):0):0):0):0):0);
  top->message = NULL;
  tries = info = top->fault = 0;
  while (*fault ? 0 : ((info != 1) ? (tries++ < TIME_LIMIT) : 0))
    {
      hybrj_ (&hybrj_fcn, &n, x, fvec, fjac, &ldfjac, &xtol, &maxfev, diag, &mode, &factor, &nprint, &info, &nfev, &njev, r,
              &lr, qtf, wa1, wa2, wa3, wa4);
      if (!info)
	avm_internal_error (101);
      if (*fault = (top->fault ? 1 : !!(top->message)))
	{
	  if (result ? top->message : NULL)
	    avm_dispose (top->message);
	  else
	    result = (top->message ? top->message : avm_copied(bad_minpack_spec));
	}
      xtol = xtol * MAGNIFIER;
    }
  freeif (fvec);
  freeif (diag);
  freeif (fjac);
  freeif (r);
  freeif (qtf);
  freeif (wa1);
  freeif (wa2);
  freeif (wa3);
  freeif (wa4);
  freeif (top->origin);
  pop_spec ();
  if (*fault ? 0 : (info == 1))
    result = avm_list_of_vector((void *) x,n,sizeof(double),fault);
  freeif (x);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}






#endif





list
avm_have_minpack_call (function_name, fault)
     list function_name;
     int *fault;

     /* this reports the availability of a function */
{
#if HAVE_MINPACK
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_minpack ();
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
avm_minpack_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;
{
#if HAVE_MINPACK
  list message;
  int function_number;

  if (*fault)
    return NULL;
  if (! initialized)
    avm_initialize_minpack ();
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
    case 1: return avm_lmder (argument, fault);
    case 2: return avm_lmstr (argument, fault);
    case 3: return avm_lmdif (argument, fault);
    case 4: return avm_hybrd (argument, fault);
    case 5: return avm_hybrj (argument, fault);
    }
#endif /* HAVE_MINPACK */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}








void
avm_initialize_minpack ()

     /* This initializes some static data structures. */
{
  char *funames[] = {
    "lmder",
    "lmstr",
    "lmdif",
    "hybrd",
    "hybrj",
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
  minpack_error = avm_join (avm_strung ("minpack error"), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  bad_minpack_spec = avm_join (avm_strung ("bad minpack specification"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized minpack function name"), NULL);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
    avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
}







void
avm_count_minpack ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{
  counter unreclaimed;

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (minpack_error);
  avm_dispose (memory_overflow);
  avm_dispose (bad_minpack_spec);
  avm_dispose (unrecognized_function_name);
  funs = NULL;
  wild = NULL;
  minpack_error = NULL;
  memory_overflow = NULL;
  bad_minpack_spec = NULL;
  unrecognized_function_name = NULL;
  unreclaimed = 0;
  while (top)
    {
      unreclaimed++;
      top = top->other_specs;
    }
  if (unreclaimed)
    avm_reclamation_failure ("spec stacks", unreclaimed);
}
