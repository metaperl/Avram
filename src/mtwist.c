/* This file provides uniform random number generation and some
   related functions using the Mersenne twistor algorithm. The
   original code from mt19937.c, copyright (C) 1997 Makoto Matsumoto
   and Takuji Nishimura, has been adapted by using the current time as
   a seed and getting it to interface appropriately with avram. The
   additional code is copyright (C) 2006 Dennis Furey, and pertains
   mainly to fast random access to lists representing probability mass
   functions.

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
#include <avm/mtwist.h>
#include <avm/matcon.h>
#if HAVE_FENV
#include <fenv.h>
#endif
#define __USE_ISOC99 1
#include <math.h>
#include <time.h>

/* pointer to a shortcut into a list representing a probability mass function */
typedef struct p_data *point;

/* shortcut into a list representing a probability mass function */
struct p_data
{
  int index;             /* the position in the list */
  list entry;            /* the address of the point within its list */
  double cu_prob;        /* the sum of the probabilities of all points up to this one */
};

/* pointer to a header for an array of entry points */
typedef struct gt_data *guide_table;

/* header for an array of entry points */
struct gt_data
{
  int npts;                   /* always a power of two */
  double norm;                /* in case it doesn't sum to 1 */
  point points;
  int signature;              /* identifies the function that tabulated it */
  guide_table other_tables;
};

/* stores shortcuts into lists that might be used again */
static guide_table guide_tables = NULL;

/* the number of extant guide tables */
static int num_tables = 0;

/* the number of guide tables allowed simultaneously, must be at least 2 */
#define MAX_TABLES 16

/* the maximum number of points in a guide table, must be a power of two > 1 */
#define MAX_POINTS 65536

/* period parameters */
#define N 624
#define M 397

/* the array for the state vector  */
static unsigned long mt[N];

/* mti==N+1 means mt[N] is not initialized */
static int mti = N + 1;

/* non-zero means static variables are initialized */
static int initialized = 0;

/* truth */
static list shared_cell = NULL;

/* error messages as lists of lists of character representations */
static list memory_overflow = NULL;
static list empty_list_draw = NULL;
static list bad_mtwist_spec = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list wild = NULL;
static list funs = NULL;






static inline counter
genat ()

/* Copyright (C) 1997 Makoto Matsumoto and Takuji Nishimura. Any
   feedback is very welcome. For any question, comments, see
   http://www.math.keio.ac.jp/matumoto/emt.html or email
   matumoto@math.keio.ac.jp  */
{
/* Period parameters */  
#define MATRIX_A 0x9908b0df   /* constant vector a */
#define UPPER_MASK 0x80000000 /* most significant w-r bits */
#define LOWER_MASK 0x7fffffff /* least significant r bits */

/* Tempering parameters */   
#define TEMPERING_MASK_B 0x9d2c5680
#define TEMPERING_MASK_C 0xefc60000
#define TEMPERING_SHIFT_U(y)  (y >> 11)
#define TEMPERING_SHIFT_S(y)  (y << 7)
#define TEMPERING_SHIFT_T(y)  (y << 15)
#define TEMPERING_SHIFT_L(y)  (y >> 18)

  unsigned long y;
  static unsigned long mag01[2]={0x0, MATRIX_A};
  int kk;

  if (mti >= N)
    {
      for (kk=0; kk<N-M; kk++)
	{
	  y = (mt[kk] & UPPER_MASK) | (mt[kk+1] & LOWER_MASK);
	  mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1];
	}
      for (; kk<N-1; kk++)
	{
	  y = (mt[kk] & UPPER_MASK) | (mt[kk+1] & LOWER_MASK);
	  mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1];
	}
      y = (mt[N-1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
      mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1];
      mti = 0;
    }
  y = mt[mti++];
  y ^= TEMPERING_SHIFT_U(y);
  y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B;
  y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C;
  y ^= TEMPERING_SHIFT_L(y);
  return (counter) y;
}







static double
genrand ()

/* A C-program for MT19937: Real number version genrand() generates
   one pseudorandom real number (double) which is uniformly
   distributed on [0,1]-interval, for each call. sgenrand(seed) set
   initial values to the working area of 624 words. Before genrand(),
   sgenrand(seed) must be called once. (seed is any 32-bit integer
   except for 0).  Integer generator is obtained by modifying two
   lines.  Coded by Takuji Nishimura, considering the suggestions by
   Topher Cooper and Marc Rieffel in July-Aug. 1997.  */
{
  return ((double) genat ()) / (unsigned long) 0xffffffff;
}








static guide_table
existing_table (signature, operand)
     int signature;
     list operand;

     /* This returns an existing table matching the operand if
	found. */
{
  guide_table result;

  result = guide_tables;
  if (operand)
    while (result)
      {
	if ((result->signature == signature) ? (result->points[0].entry == operand) : 0)
	  return result;
	result = result->other_tables;
      }
  return NULL;
}









static void
lose_newest_table ()

/* This gets rid of the newest table, which better exist. */

{
  int i;
  guide_table new;

  if (!(guide_tables ? num_tables : 0))
    avm_internal_error (53);
  num_tables--;
  for (i = 0; i < guide_tables->npts; i++)
    avm_dispose (guide_tables->points[i].entry);
  free (guide_tables->points);
  guide_tables = (new = guide_tables)->other_tables;
  free (new);
}









static void
lose_oldest_table ()

/* This gets rid of the oldest table, which better exist and not be
   the newest. */

{
  guide_table penultimate;
  int i;

  if (!(guide_tables ? (guide_tables->other_tables ? (num_tables > 1) : 0) : 0))
    avm_internal_error (57);
  num_tables--;
  penultimate = guide_tables;
  while (penultimate->other_tables->other_tables)
    penultimate = penultimate->other_tables;
  for (i = 0; i < penultimate->other_tables->npts; i++)
    avm_dispose (penultimate->other_tables->points[i].entry);
  free (penultimate->other_tables->points);
  free (penultimate->other_tables);
  penultimate->other_tables = NULL;
}











static guide_table
new_table (signature, length)
     int signature;
     counter length;

     /* This creates a new table for the given length if possible and
	frees an old one if the cache is full. The number of points in
	the table is the largest power of 2 not greater than the given
	length that there's room to allocate. */
{
  int i;
  int npts;
  guide_table new;

  if (num_tables == MAX_TABLES)
    lose_oldest_table ();
  if (length > MAX_POINTS)
    npts = MAX_POINTS;
  else
    {
      npts = 1;
      while (npts <= length)
	npts <<= 1;
      npts >>= 1;
    }
  if (!(new = (guide_table) malloc (sizeof(*new))))
    return NULL;
  memset (new, 0, sizeof(*new));
  while ((new->points = (point) malloc (npts * sizeof(*(new->points)))) ? 0 : (npts > 2))
    npts >>= 1;
  if (!(new->points))
    {
      free (new);
      return NULL;
    }
  num_tables++;
  new->npts = npts;
  new->signature = signature;
  memset (new->points, 0, new->npts * sizeof(*(new->points)));
  new->other_tables = guide_tables;
  return (guide_tables = new);
}








static point
starting_point_by_index (table, target)
     guide_table table;
     int target;

     /* This binary searches the table for the point with the maximum
	index not greater than the target. */
{
  int pivot;
  int bit;

  if (!table)
    return NULL;
  if (!(table->points))
    avm_internal_error (33);
  bit = pivot = (table->npts >> 1);
  while (bit)
    {
      if (table->points[pivot].index > target)
	pivot -= bit;
      else if (table->points[pivot].index == target)
	bit = 0;
      pivot |= (bit >>= 1);
    }
  if ((table->points[pivot].index > target) ? pivot : 0)
    pivot--;
  return &(table->points[pivot]);
}










static point
starting_point_by_probability (table, target)
     guide_table table;
     double target;

     /* This binary searches the table for a point with a cumulative
	probability close to the target. The result will always have a
	cumulative probability less than or equal to the target unless
	the target is less than the smallest cumulative probability in
	the table. */
{
  int pivot;
  int bit;

  if (!table)
    return NULL;
  if (!(table->points))
    avm_internal_error (6);
  bit = pivot = (table->npts >> 1);
  while (bit)
    {
      if (table->points[pivot].cu_prob > target)
	pivot -= bit;
      else if (table->points[pivot].cu_prob == target)
	bit = 0;
      pivot |= (bit >>= 1);
    }
  if ((table->points[pivot].cu_prob > target) ? pivot : 0)
    pivot--;
  return &(table->points[pivot]);
}













static list
bern (operand, fault)
     list operand;
     int *fault;

     /* This returns true if a standard uniform draw is less than a
	given probability (i.e., a Bernoulli distribution). The
	default is 1/2. */
{
  double *p;
  list result;

  if (*fault)
    return NULL;
  result = NULL;
  if (!operand)
    return ((genat () & 1) ? avm_copied (shared_cell) : NULL);
  p = (double *) avm_value_of_list (operand, &result, fault);
  if (*fault)
    return (result ? result : avm_copied (bad_mtwist_spec));
  if (fpclassify(*p) == FP_ZERO)
    return NULL;
  if (*fault = (*fault ? 1 : (result ? 1 : (!p ? 1 : ((fpclassify(*p) != FP_NORMAL) ? 1 : ((*p < 0.0) ? 1 : (*p > 1.0)))))))
    return (result ? result : avm_copied (bad_mtwist_spec));
  return ((genrand () < *p) ? avm_copied (shared_cell) : NULL);
}










static list
u_cont (operand, fault)
     list operand;
     int *fault;

     /* This returns a draw from a continuous uniform distribution
	over the range of zero to the operand. The default is 1. */
{
  double *range;
  double draw;
  list result;

  if (*fault)
    return NULL;
  if (!operand)
    {
      draw = genrand ();
      return avm_list_of_value ((void *) &draw, sizeof (double), fault);
    }
  result = NULL;
  range = (double *) avm_value_of_list(operand, &result, fault);
  if (*fault = (*fault ? 1 : (!!result ? 1 : (!range ? 1 : (fpclassify(*range) != FP_NORMAL)))))
    return (result ? result : avm_copied (bad_mtwist_spec));
  draw = *range * genrand ();
  return avm_list_of_value ((void *) &draw, sizeof (double), fault);
}









static list
u_disc (operand, fault)
     list operand;
     int *fault;

     /* This returns a draw from a discrete uniform distribution over
	the range of zero to the operand - 1, or over the full 64 bit
	range if the operand is 0. */
{
  counter modulus,width,random;
  list result;

  if (*fault)
    return NULL;
  if (!operand)
    {
      modulus = (genat() << 32) + genat();
      if (!modulus)
	return NULL;
      if (*fault = !(result = avm_natural (modulus)))
	return avm_copied (memory_overflow);
      return result;
    }
  result = NULL;
  width = avm_counter (operand);
  random = ((width <= 0xffffffff) ? genat () : ((genat() << 32) + genat()));
  if (modulus = (width ? (random % width) : random))
    if (*fault = !(result = avm_natural (modulus)))
      return avm_copied (memory_overflow);
  return result;
}









static list
u_path (operand, fault)
     list operand;
     int *fault;

     /* This returns a non-empty singly branched list of the given
	depth. */
{
  counter modulus,depth,width,random;
  list result;

  if (*fault)
    return NULL;
  if (*fault = !operand)
    return avm_copied (bad_mtwist_spec);
  result = avm_copied (shared_cell);
  random = (((width = avm_counter (operand->tail)) ? (width <= 0xffffffff) : 0) ? genat() : ((genat() << 32) + genat()));
  modulus = (width ? (random % width) : random);
  if (!(depth = avm_counter (operand->head)))
    return result;
  while (*fault ? 0 : depth--)
    {
      *fault = !(result = ((modulus & 1) ? avm_recoverable_join (NULL, result) : avm_recoverable_join (result, NULL)));
      modulus >>= 1;
    }
  if (*fault)
    return avm_copied (memory_overflow);
  return result;
}









static list
u_enum (operand, fault)
     list operand;
     int *fault;

     /* This returns a randomly chosen item of the operand list. */
{
  int target;
  counter length;
  guide_table table;
  int index,item_number,i;
  point starting_point;

#define U_ENUM 0

  if (*fault = (*fault ? 1 : !operand))
    return avm_copied (empty_list_draw);
  length = avm_recoverable_length (operand);
  if (!(table = existing_table (U_ENUM, operand)))
    if (table = new_table (U_ENUM, length))
      {
	item_number = 0;
	for (i = 0; i < table->npts; i++)
	  {
	    if (!operand)
	      avm_internal_error (42);
	    table->points[i].index = item_number;
	    table->points[i].entry = avm_copied (operand);
	    while (operand ? ((((float) item_number) / (float) length) < (((float) (i + 1)) / (float) table->npts)) : 0)
	      {
		operand = operand->tail;
		item_number++;
	      }
	  }
      }
  index = 0;
  if (starting_point = starting_point_by_index (table, target = (int) genat () % length))
    {
      index = starting_point->index;
      operand = starting_point->entry;
    }
  while (index++ < target)
    {
      if (!operand)
	avm_internal_error (3);
      operand = operand->tail;
    }
  if (!operand)
    avm_internal_error (4);
  return avm_copied (operand->head);
}









static guide_table
w_table (disc, operand, result, fault)
     int disc;
     list operand;
     list *result;
     int *fault;

     /* This initializes a guide table with cumulative probabilities
	based on the operand for the w_disc or w_enum functions. A
	non-zero value of disc causes the items of the operand to be
	interpreted as probabilities (per the w_disc function), and a
	zero value cause their right sides to be interpreted as
	probabilities (per the w_enum function). The validity of the
	probabilities is checked as a side effect, so that subsequent
	draws using the table don't have to check them. */
{
  guide_table table;
  counter length;
  double *p;
  int item_number;
  double subtotal;
  int i;

#define W_SIG 1

  if (table = existing_table (W_SIG + disc, operand))
    return table;
  if (*fault = (*fault ? 1 : !!(*result)))
    return NULL;
  if (*fault = !(operand ? (disc ? 1 : !!(operand->head)) : 0))
    *result = avm_copied (empty_list_draw);
  if (*fault = (*fault ? 1 : !(table = new_table (W_SIG + disc, length = (int) avm_length (operand)))))
    *result = (*result ? *result : avm_copied (memory_overflow));
  if (*fault)
    return NULL;
  p = (double *) avm_value_of_list (disc ? operand->head : operand->head->tail, result, fault);
  if (!*fault)
    *fault = !(!*result ? (p ? ((fpclassify(*p) == FP_NORMAL) ? (*p >= 0.0) : 0) : 0) : 0);
  subtotal = (*fault ? 0.0 : *p);
  item_number = 0;
  for (i = 0; i < table->npts; i++)
    {
      table->points[i].cu_prob = subtotal;
      table->points[i].index = item_number;
      table->points[i].entry = (*fault ? NULL : avm_copied (operand));
      if (i + 1 < table->npts)
	while (operand ? ((((float) item_number) / (float) length) < (((float) (i + 1)) / (float) table->npts)) : 0)
	  {
	    *fault = (*fault ? 1 : !((operand = operand->tail) ? (disc ? 1 : !!(operand->head)) : 0));
	    p = (*fault ? NULL : (double *) avm_value_of_list (disc ? operand->head : operand->head->tail, result, fault));
	    if (!*fault)
	      *fault = !(!*result ? (p ? ((fpclassify(*p) == FP_NORMAL) ? (*p >= 0.0) : 0) : 0) : 0);
	    subtotal += (*fault ? 0.0 : *p);
	    item_number++;
	  }
    }
  while (*fault ? 0 : (operand = operand->tail))
    {
      *fault = (disc ? 0 : !(operand->head));
      p = (*fault ? NULL : (double *) avm_value_of_list (disc ? operand->head : operand->head->tail, result, fault));
      if (!*fault)
	*fault = !(!*result ? (p ? ((fpclassify(*p) == FP_NORMAL) ? (*p >= 0.0) : 0) : 0) : 0);
      subtotal += (*fault ? 0.0 : *p);
    }
  table->norm = subtotal;
  if (!*fault)
    return table;
  lose_newest_table ();
  *result = (*result ? *result : avm_copied (bad_mtwist_spec));
  return NULL;
}









static list
w_disc (operand, fault)
     list operand;
     int *fault;

     /* The operand is a list of probabilities describing a discrete
	distribution from which a draw is returned as a natural
	number. Internal errors are reported because the operand
	should be validated by the time the table is built. */
{
  double subtotal;
  double target;
  double *p;
  list result;
  guide_table table;
  point starting_point;
  int index;

  if (*fault)
    return NULL;
  result = NULL;
  table = w_table (1, operand, &result, fault);
  if (*fault = (*fault ? 1 : (result ? 1 : !table)))
    return (result ? result : avm_copied (bad_mtwist_spec));
  if (!(starting_point = starting_point_by_probability (table, target = (genrand () * table->norm))))
    avm_internal_error (52);
  index = starting_point->index;
  operand = starting_point->entry;
  subtotal = starting_point->cu_prob;
  while ((subtotal < target) ? operand : NULL)
    {
      *fault = !(operand = operand->tail);
      p = (*fault ? NULL : (double *) avm_value_of_list (operand->head, &result, fault));
      if (*fault ? 1 : (result ? 1 : !p))
	avm_internal_error (51);
      subtotal += *p;
      index++;
    }
  if (!index)
    return NULL;
  if (*fault = !(result = avm_recoverable_natural (index)))
    return avm_copied (memory_overflow);
  return result;
}








static list
w_enum (operand, fault)
     list operand;
     int *fault;

     /* The operand is a list of <(x,p)..> with p being the
	probability of drawing x. The result is an x drawn from the
	list consistently with the probabilities. Internal errors are
	reported because the operand should have been validated by the
	time the table is built. */
{
  double subtotal;
  double target;
  double *p;
  list result;
  guide_table table;
  point starting_point;

  if (*fault)
    return NULL;
  result = NULL;
  table = w_table (0, operand, &result, fault);
  if (*fault = (*fault ? 1 : (result ? 1 : !table)))
    return (result ? result : avm_copied (bad_mtwist_spec));
  if (!(starting_point = starting_point_by_probability (table, target = (genrand () * table->norm))))
    avm_internal_error (58);
  operand = starting_point->entry;
  subtotal = starting_point->cu_prob;
  while ((subtotal < target) ? operand : NULL)
    {
      *fault = !((operand = operand->tail) ? operand->head : NULL);
      p = (*fault ? NULL : (double *) avm_value_of_list (operand->head->tail, &result, fault));
      if (*fault ? 1 : (result ? 1 : !p))
	avm_internal_error (59);
      subtotal += *p;
    }
  if (!(operand ? operand->head : NULL))
    avm_internal_error (67);
  return avm_copied (operand->head->head);
}








list
avm_have_mtwist_call (function_name, fault)
     list function_name;
     int *fault;

/* this reports the availability of a function */

{
#if HAVE_FENV
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_mtwist ();
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
avm_mtwist_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;

     /* This figures out what function to call and calls it. */
{
#if HAVE_FENV
  list message;
  int function_number;

  if (! initialized)
    avm_initialize_mtwist ();
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
  message = NULL;
  switch (function_number)
    {
    case 1: return bern (argument, fault);
    case 2: return u_cont (argument, fault);
    case 3: return u_disc (argument, fault);
    case 4: return u_enum (argument, fault);
    case 5: return w_disc (argument, fault);
    case 6: return w_enum (argument, fault);
    case 7: return u_path (argument, fault);
    }
#endif /* HAVE_FENV */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}







void
avm_initialize_mtwist ()

     /* This initializes some static data structures. */

{
  char *funames[] = {
    "bern",
    "u_cont",
    "u_disc",
    "u_enum",
    "w_disc",
    "w_enum",
    "u_path",
    NULL};            /* add more function names here up to a total of 255 */
  list back;
  int string_number;
  unsigned long seed;	

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_matcon ();
  avm_initialize_listfuns ();
  avm_initialize_chrcodes ();
  shared_cell = avm_join (NULL, NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  empty_list_draw = avm_join (avm_strung ("draw from empty list"), NULL);
  bad_mtwist_spec = avm_join (avm_strung ("bad mtwist specification"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized mtwist function name"), NULL);
  string_number = 0;
  wild = avm_strung("*");
  funs = back = NULL;
  while (funames[string_number])
    avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
#if HAVE_FENV
  /* setting initial seeds to mt[N] using         */
  /* the generator Line 25 of Table 1 in          */
  /* [KNUTH 1981, The Art of Computer Programming */
  /*    Vol. 2 (2nd Ed.), pp102]                  */
  seed = time(0);
  mt[0]= seed & 0xffffffff;
  for (mti=1; mti<N; mti++)
    mt[mti] = (69069 * mt[mti-1]) & 0xffffffff;
#endif /* HAVE_FENV */
}







void
avm_count_mtwist ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{
  guide_table old;
  counter i;

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (shared_cell);
  avm_dispose (memory_overflow);
  avm_dispose (empty_list_draw);
  avm_dispose (bad_mtwist_spec);
  avm_dispose (unrecognized_function_name);
  while (guide_tables)
    lose_newest_table ();
  funs = NULL;
  wild = NULL;
  empty_list_draw = NULL;
  memory_overflow = NULL;
  bad_mtwist_spec = NULL;
  unrecognized_function_name = NULL;
}
