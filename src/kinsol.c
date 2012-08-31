
/* This file interfaces to some non-linear constrained optimization
   functions from the kinsol library in the sundials-serial package.

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
#include <avm/kinsol.h>
#include <avm/mwrap.h>
#include <math.h>
#if HAVE_KINSOL
#include <sundials_types.h>
#include <sundials_math.h>
#include <sundials_nvector.h>
// #include <sundials_smalldense.h>
#include <kinsol.h>
#include <kinsol_dense.h>
#include <kinsol_spgmr.h>
#include <kinsol_spbcgs.h>
#include <kinsol_sptfqmr.h>
#include <kinsol_spils.h>
#include <nvector_serial.h>

struct fspec {                  /* a pointer to one of these is passed to c functions evaluating the system and jacobian */
  list operator;                /* a function whose roots relative to the origin are sought */
  list jacobian;                /* returns a list of rows, one for each output, each row a list of partial derivatives */
  list message;                 /* returned by either operator or jacobian when evaluation causes an exception */
  int fault;                    /* non-zero in the event of an exception */
  counter number_of_inputs;     /* the length of the list to be passed to the operator as an argument */
  counter number_of_outputs;    /* the length of the list expected to be returned by the operator */
  double *output_origin;        /* constant to be subtracted from the operator's result, therefore having the same length */
  list *row_number;             /* constant array of length number_of_outputs whose ith element is the representation of i */
};

typedef struct fspec *fsptr;
typedef int (*spilsolver)(void*,int);

#endif

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list kinsol_error = NULL;
static list memory_overflow = NULL;
static list bad_kinsol_spec = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list wild = NULL;
static list funs = NULL;

#if HAVE_KINSOL
/* the sundials library has to be built for double precision or it won't be compatible with avram */
#if defined SUNDIALS_DOUBLE_PRECISION

/* the tightest tolerance worth trying */
#define MINIMUM_NORM_TOLERANCE 1E-16
#define MINIMUM_STEP_TOLERANCE 1E-16

/* the number of retries at bigger tolerances before giving up due to slow convergence */
#define TIME_LIMIT 15

/* the factor by which the tolerance is increased on each attempt */
#define MAGNIFIER 4.64158883361278

#define freeif(x) if (x)			\
    free (x)

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#endif





static int
func (u, fval, f_data)
     N_Vector u;
     N_Vector fval;
     void *f_data;

     /* computes the system function whose roots are sought by
	evaluating the virtual code in the given f_data structure */
{
  double *y;
  int i,n;
  list row,operand,result;
  double *item;
  fsptr spec;

  spec = (fsptr) f_data;
  if (spec->fault = (spec->fault ? 1 : !!(spec->message)))
    return -1;
  if (spec->fault = ((NV_LENGTH_S(u) < spec->number_of_inputs) ? 1 : (NV_LENGTH_S(fval) < spec->number_of_outputs)))
    {
      spec->message = avm_copied (kinsol_error);
      return -1;
    }
  operand = avm_list_of_vector((void *) NV_DATA_S(u), spec->number_of_inputs, sizeof(double), &(spec->fault));
  if (spec->fault)
    {
      spec->message = operand;
      return -1;
    }
  row = result = avm_recoverable_apply (avm_copied (spec->operator), operand, &(spec->fault));
  if (spec->fault)
    {
      spec->message = result;
      return -1;
    }
  i = 0;
  y = (double *) NV_DATA_S(fval);
  while ((i < spec->number_of_outputs) ? !(spec->fault = (spec->fault ? 1 : !row)) : 0)
    {
      item = (double *) avm_value_of_list (row->head, &(spec->message), &(spec->fault));
      y[i] = (spec->fault ? 0.0 : ((*item) - ((spec->output_origin)[i])));
      row = row->tail;
      i++;
    }
  avm_dispose (result);
  if (spec->fault = (spec->fault ? 1 : (row ? 1 : !!(spec->message))))
    {
      spec->message = (spec->message ? spec->message : avm_copied (bad_kinsol_spec));
      return -1;
    }
  while (i < NV_LENGTH_S(u))  /* kinsol requires equally many inputs and outputs so the extras are set to zero */
    y[i++] = 0.0;
  return 0;
}







static int
djac (N, J, u, fu, jac_data, tmp1, tmp2)
     long int N;
     N_Vector u;
     N_Vector fu;
     DlsMat J;
     void *jac_data;
     N_Vector tmp1;
     N_Vector tmp2;

     /* This c function is passed to kinsol as the user specified
	jacobian for the dense solution method.  It finds the answer
	by evaluating the virtual code jacobian function given by
	jac_data. */
{
  int i,j;
  list row,col,operand,result;
  double *item;
  fsptr spec;

  spec = (fsptr) jac_data;
  if (spec->fault = (spec->fault ? 1 : !!(spec->message)))
    return -1;
  if (spec->fault = ((NV_LENGTH_S(u) < spec->number_of_inputs) ? 1 : (N < spec->number_of_outputs)))
    {
      spec->message = avm_copied (kinsol_error);
      return -1;
    }
  operand = avm_list_of_vector ((void *) NV_DATA_S(u), spec->number_of_inputs, sizeof(double), &(spec->fault));
  if (spec->fault)
    {
      spec->message = operand;
      return -1;
    }
  row = result = avm_recoverable_apply (avm_copied (spec->jacobian), operand, &(spec->fault));
  if (spec->fault)
    {
      spec->message = result;
      return -1;
    }
  i = 0;
  while ((i < spec->number_of_outputs) ? !(spec->fault = (spec->fault ? 1 : !row)) : 0)
    {
      j = 0;
      col = row->head;
      while ((j < spec->number_of_inputs) ? !(spec->fault = (spec->fault ? 1 : !col)) : 0)
	{
	  item = (double *) avm_value_of_list (col->head, &(spec->message), &(spec->fault));
	  printf("%d %d %d %10.4e\n",spec->fault,i,j,*item);
	  DENSE_ELEM(J,i,j) = (spec->fault ? 0.0 : *item);
	  printf("2\n");
	  col = col->tail;
	  j++;
	}
      spec->fault = (spec->fault ? 1 : !!col);
      row = row->tail;
      i++;
    }
  avm_dispose (result);
  if (spec->fault = (spec->fault ? 1 : !!row))
    {
      spec->message = (spec->message ? spec->message : avm_copied (bad_kinsol_spec));
      return -1;
    }
  return 0;
}











static int
jtimes (v, Jv, u, new_u, jac_data)
     N_Vector v;
     N_Vector Jv;
     N_Vector u;
     booleantype new_u;
     void *jac_data;

     /* This is the c function passed to kinsol as a user specified
        jacobian for the indirect solution methods (i.e., those other
        than dense). The virtual code jacobian function in jac_data is
        assumed to take an argument (i,<x..>) and return only the ith
        row of the jacobian at <x..> instead of the whole thing. This
        function computes the jacobian by multiple invocations of the
        virtual code and stores the matrix product of the jacobian
        with v in the vector Jv, as required by kinsol. */
{
  int i,j;
  list row,col,operand,argument,result;
  double *item;
  fsptr spec;

  spec = (fsptr) jac_data;
  if (spec->fault = (spec->fault ? 1 : !!(spec->message)))
    return -1;
  if (spec->fault = ((NV_LENGTH_S(u) < spec->number_of_inputs) ? 1 : (NV_LENGTH_S(Jv) < spec->number_of_outputs)))
    {
      spec->message = avm_copied (kinsol_error);
      return -1;
    }
  operand = avm_list_of_vector ((void *) NV_DATA_S(u), spec->number_of_inputs, sizeof(double), &(spec->fault));
  if (spec->fault)
    {
      spec->message = operand;
      return -1;
    }
  i = 0;
  while (spec->fault ? 0 : (i < spec->number_of_outputs))
    {
      if (spec->fault = !(argument = avm_recoverable_join (avm_copied (spec->row_number[i]), avm_copied (operand))))
	spec->message = avm_copied (memory_overflow);
      else
	result = avm_recoverable_apply (avm_copied (spec->jacobian), argument, &(spec->fault));
      if (spec->fault)
	{
	  spec->message = (spec->message ? spec->message : result);
	  result = NULL;
	}
      j = 0;
      col = result;
      NV_Ith_S(Jv,i) = 0.0;
      while ((j < spec->number_of_inputs) ? !(spec->fault = (spec->fault ? 1 : !col)) : 0)
	{
	  item = (double *) avm_value_of_list (col->head, &(spec->message), &(spec->fault));
	  NV_Ith_S(Jv,i) = (spec->fault ? 0.0 : NV_Ith_S(Jv,i) + (*item * NV_Ith_S(v,j)));
	  col = col->tail;
	  j++;
	}
      spec->fault = (spec->fault ? 1 : !!col);
      avm_dispose (result);
      i++;
    }
  while (i < NV_LENGTH_S(Jv))  /* kinsol requires equally many inputs and outputs so the extras are set to zero */
    {
      NV_Ith_S(Jv,i) = 0.0;
      i++;
    }
  avm_dispose (operand);
  if (spec->fault)
    {
      spec->message = (spec->message ? spec->message : avm_copied (bad_kinsol_spec));
      return -1;
    }
  return 0;
}







static list
solution (jacobian, constrained, spils_method, operand, fault)
     int jacobian;
     int constrained;
     spilsolver spils_method;
     list operand;
     int *fault;

     /* This is a simplified interface to the kinsol dense solver and
	spils solvers, but not the band solver, with or without
	constraints and with or without a user defined jacobian.

        jacobian should be 0 if there's no user supplied jacobian, in
        which case the default difference approximation will be used,
        and non-zero otherwise.

        constrained should be zero if there are no constraints, and
        non-zero if all variables are constrained to be non-negative.
        There are no other alternatives, but this form incurs no loss
        of generality.

        spils_method is one of &KINSpgmr &KINSpbcg or &KINSptfqmr to
        specify the corresponding indirect solution method, or NULL if
        the dense method is intended.

        operand is a list of the form ((f,j),i,o) if jacobian is
        non-zero, or (f,i,o) otherwise, where f and j are virtual
        machine code functions and i and o are lists of reals. 

        * f represents the function to be optimized, and takes a list
          of reals the length of i to a list of reals the length of
          o. 

	* If spils_method is null, then j computes the jacobian of f
          by taking a list of reals the length of i to a list the
          length of o, in which each item of the latter is a list of
          reals the length of i containing the partial derivatives of
          a particular output with respect to each input. If
          spils_method is not null, then j computes the kth row of the
          jacobian from an argument of the form (k,<x..>) where k is
          a natural number less than the length of o, and <x..> is a
          list of reals the length of i.

	* i is an initial guess for the optimum input of f

	* o is an output of f for which an x near i is sought to
	  satisfy f(x) - o = 0

        fault is a pointer to an integer that's set to a non-zero
        value in the event of an exception due to memory overflows,
        invalid input parameters, bugs in kinsol, or a failure
        evaluating f or j.

        If there's an exception, the result returned is a list
        representing an error message as a list of lists of character
        encodings, or in whatever form f or j returns if one of them
        causes the exception. Otherwise, the result returned is a list
        of reals x the same length as i satisfying f(x) - o = 0 if one
        is found, or the empty list otherwise. Lack of convergence
        doesn't constitute an exceptional condition but could cause the
        result to be empty.

     */

{
  struct fspec spec;
  counter number_of_inputs;
  long int N;
  void *kin_mem;
  N_Vector u,c,s;
  double *input;
  list result;
  double norm_tol;
  double step_tol;
  int try,flag,i,strategy;

#define divergent(flag)				\
  ((flag == KIN_LINESEARCH_NONCONV) ? 1 :	\
   ((flag == KIN_MAXITER_REACHED) ? 1 :		\
    ((flag == KIN_MXNEWT_5X_EXCEEDED) ? 1 :	\
     ((flag == KIN_LSETUP_FAIL) ? 1 :	\
      (flag == KIN_LINESEARCH_BCFAIL)))))

  u = NULL;
  c = NULL;
  s = NULL;
  kin_mem = NULL;
  if (*fault)
    return NULL;
  if (*fault = spec.fault = !(operand ? (operand->head ? operand->tail : NULL) : NULL))
    return avm_copied (bad_kinsol_spec);
  spec.message = spec.jacobian = NULL;
  if (!jacobian)
    spec.operator = operand->head;
  else if (*fault = !((spec.operator = operand->head->head) ? (spec.jacobian = operand->head->tail) : NULL))
    return avm_copied (bad_kinsol_spec);
  spec.number_of_inputs = avm_length (operand->tail->head);
  spec.number_of_outputs = avm_length (operand->tail->tail);
  avm_turn_off_stderr ();                                      /* kinsol whinges too much so temporarily disable stderr */
  N = MAX(spec.number_of_inputs, spec.number_of_outputs);
  result = NULL;
  input = (*fault ? NULL : (double *) avm_vector_of_list (operand->tail->head, sizeof(double), &result, fault));
  spec.output_origin = (*fault ? NULL : (double *) avm_vector_of_list(operand->tail->tail, sizeof(double), &result, fault));
  *fault = (*fault ? 1 : !(kin_mem = KINCreate ()));
  u = (*fault ? NULL : N_VNew_Serial (N));
  if (*fault ? 0 : !(*fault = !u))
    {
      memcpy ((void *) NV_DATA_S (u), (void *) input, sizeof(double) * spec.number_of_inputs);
      for (i = spec.number_of_inputs; i < N; i++)
	NV_Ith_S(u,i) = 0.0;
    }
  // *fault = (*fault ? 1 : (KINSetFdata (kin_mem, (void *) &spec) != KIN_SUCCESS));    /* old API */
  spec.row_number = (*fault ? NULL : (spils_method ? avm_row_number_array (spec.number_of_outputs, fault) : NULL));
  norm_tol = MINIMUM_NORM_TOLERANCE * ((jacobian ? spils_method : NULL) ? 1.0 : 10.0);
  step_tol = MINIMUM_STEP_TOLERANCE * ((jacobian ? spils_method : NULL) ? 1.0 : 10.0);
  *fault = (*fault ? 1 : (KINSetFuncNormTol (kin_mem, norm_tol) != KIN_SUCCESS));
  *fault = (*fault ? 1 : (KINSetScaledStepTol (kin_mem, step_tol) != KIN_SUCCESS));
  if (*fault ? 0 : (constrained ? !(*fault = !(c = N_VNew_Serial (N))) : 0))
    {
      N_VConst (2.0, c);
      *fault = (KINSetConstraints (kin_mem, c) != KIN_SUCCESS);
      N_VDestroy (c);
    }
  *fault = (*fault ? 1 : ((spils_method ? (*spils_method)(kin_mem, 0) : KINDense(kin_mem, N)) != KIN_SUCCESS));
  if (*fault ? 0 : (jacobian ? spils_method : NULL))
    *fault = (KINSpilsSetJacTimesVecFn(kin_mem, &jtimes) != KIN_SUCCESS);
  else if (*fault ? 0 : jacobian)
    *fault = (KINDlsSetDenseJacFn (kin_mem, &djac) != KIN_SUCCESS);
  *fault = (*fault ? 1 : !(s = N_VNew_Serial (N)));
  if (!*fault)
    N_VConst (1.0, s);
  strategy = KIN_NONE;
  strategy = KIN_LINESEARCH;    /* tha alternative is KIN_NONE */
  spec.fault = try = flag = 0;
  *fault = (*fault ? 1 : (KINSetUserData(kin_mem, (void *) &spec) != KIN_SUCCESS));
  *fault = (*fault ? 1 : (KINInit (kin_mem, &func, u) != KIN_SUCCESS));
  while (*fault ? 0 : (((flag = KINSol(kin_mem, u, strategy, s, s)) < 0) ? (divergent(flag) ? (try++ < TIME_LIMIT) : 0) : 0))
    {
      *fault = (spec.fault ? 1 : !!(spec.message));
      *fault = (*fault ? 1 : (KINSetFuncNormTol (kin_mem, norm_tol = (norm_tol * MAGNIFIER)) != KIN_SUCCESS));
      *fault = (*fault ? 1 : (KINSetScaledStepTol (kin_mem, step_tol = (step_tol * MAGNIFIER)) != KIN_SUCCESS));
      memcpy((void *) NV_DATA_S(u), (void *) input, sizeof(double) * spec.number_of_inputs);
      for (i = spec.number_of_inputs; i < N; i++)
	NV_Ith_S(u,i) = 0.0;
    }
  if (input)
    free (input);
  if (spec.output_origin)
    free (spec.output_origin);
  if (spec.row_number)
    avm_dispose_rows (spec.number_of_outputs, spec.row_number);
  if (s)
    N_VDestroy (s);
  if (kin_mem)
    KINFree (&kin_mem);
  if (!(*fault = (*fault ? 1 : (spec.fault ? 1 : (spec.message ? 1 : !!result)))))
    {
      if (*fault = ((flag < 0) ? !divergent(flag) : 0))
	result = avm_copied (kinsol_error);
      else if (flag >= 0)
	result = avm_list_of_vector ((void *) NV_DATA_S(u), spec.number_of_inputs, sizeof(double), fault);
    }
  else if (result ? spec.message : NULL)
    avm_dispose (spec.message);
  else
    result = (result ? result : spec.message);
  if (u)
    N_VDestroy (u);
  avm_turn_on_stderr ();                                                                          /* restore stderr */
  return (*fault ? (result ? result : avm_copied (memory_overflow)) : result);
}







#endif
#endif





list
avm_have_kinsol_call (function_name, fault)
     list function_name;
     int *fault;

     /* this reports the availability of a function */
{
#if HAVE_KINSOL
#if defined SUNDIALS_DOUBLE_PRECISION
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_kinsol ();
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
avm_kinsol_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;
{
#if HAVE_KINSOL
#if defined SUNDIALS_DOUBLE_PRECISION
  list message;
  int function_number;

  if (*fault)
    return NULL;
  if (! initialized)
    avm_initialize_kinsol ();
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
    case 1: return solution (0, 0, NULL, argument, fault);
    case 2: return solution (0, 1, NULL, argument, fault);
    case 3: return solution (1, 0, NULL, argument, fault);
    case 4: return solution (1, 1, NULL, argument, fault);
    case 5: return solution (0, 0, &KINSpgmr, argument, fault);
    case 6: return solution (0, 1, &KINSpgmr, argument, fault);
    case 7: return solution (1, 0, &KINSpgmr, argument, fault);
    case 8: return solution (1, 1, &KINSpgmr, argument, fault);
    case 9: return solution (0, 0, &KINSpbcg, argument, fault);
    case 10: return solution (0, 1, &KINSpbcg, argument, fault);
    case 11: return solution (1, 0, &KINSpbcg, argument, fault);
    case 12: return solution (1, 1, &KINSpbcg, argument, fault);
    case 13: return solution (0, 0, &KINSptfqmr, argument, fault);
    case 14: return solution (0, 1, &KINSptfqmr, argument, fault);
    case 15: return solution (1, 0, &KINSptfqmr, argument, fault);
    case 16: return solution (1, 1, &KINSptfqmr, argument, fault);
    }
#endif
#endif
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}








void
avm_initialize_kinsol ()

     /* This initializes some static data structures. */
{
  char *funames[] = {
    "ud_dense",
    "cd_dense",
    "uj_dense",
    "cj_dense",
    "ud_gmres",
    "cd_gmres",
    "uj_gmres",
    "cj_gmres",
    "ud_bicgs",
    "cd_bicgs",
    "uj_bicgs",
    "cj_bicgs",
    "ud_tfqmr",
    "cd_tfqmr",
    "uj_tfqmr",
    "cj_tfqmr",
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
  kinsol_error = avm_join (avm_strung ("kinsol error"), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  bad_kinsol_spec = avm_join (avm_strung ("bad kinsol specification"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized kinsol function name"), NULL);
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
    avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
}







void
avm_count_kinsol ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (kinsol_error);
  avm_dispose (memory_overflow);
  avm_dispose (bad_kinsol_spec);
  avm_dispose (unrecognized_function_name);
  funs = NULL;
  wild = NULL;
  kinsol_error = NULL;
  memory_overflow = NULL;
  bad_kinsol_spec = NULL;
  unrecognized_function_name = NULL;
}
