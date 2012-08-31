
/* this file contains some functions for converting between lists and
   arrays

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
#include <avm/chrcodes.h>
#include <avm/matcon.h>

#define packed_index(i,j) (upper_triangular ? (((j*(j+1))>>1)+i) : (((((n*(n+1))>>1)-((((n-j)+1)*(n-j))>>1))+i)-j))

/* non-zero means static variables are initialized */
static int initialized = 0;

/* representation of a true boolean */
static list shared_cell = NULL;

/* error messages as lists of lists of character representations */
static list bad_matrix_spec = NULL;
static list bad_vector_spec = NULL;
static list memory_overflow = NULL;
static list counter_overflow = NULL;








void
*avm_vector_of_list(operand, item_size, message, fault)
     list operand;
     size_t item_size;
     list *message;
     int *fault;

     /* takes a list representing a vector of equally sized items to
	a contiguous array representation */
{
  counter num_items,index;
  char *item,*result;
  char *vector;

  if (*fault = (*fault ? 1 : !!(*message)))
    return NULL;
  result = NULL;
  vector = NULL;
  if (*fault = (item_size ? !(num_items = avm_length(operand)) : 1))
    *message = avm_copied(bad_vector_spec);
  if (*fault = (*fault ? 1 : (!operand ? 1 : (item_size != (size_t) avm_length (operand->head)))))
    *message = (*message ? *message : avm_copied(bad_vector_spec));
  if (*fault = (*fault ? 1 : !(vector = result = (char *) malloc(num_items * item_size))))
    *message = (*message ? *message : avm_copied(memory_overflow));
  index = 0;
  while (*fault ? 0 : (index < num_items))
    {
      if (!operand)
	avm_internal_error (82);
      item = (char *) avm_value_of_list (operand->head, message, fault);   /* all item sizes could be verified but aren't */
      if (*fault ? 0 : !item)
	avm_internal_error (83);
      if (!*fault)
	memcpy((void *) vector,(void *) item,item_size);
      vector = vector + item_size;
      operand = operand->tail;
      index++;
    }
  if (!*fault)
    return result;
  if (result)
    free(result);
  *message = (*message ? *message : avm_copied(bad_vector_spec));
  return NULL;
}








list
avm_list_of_vector(vector, num_items, item_size, fault)
     void *vector;
     int num_items;
     size_t item_size;
     int *fault;

     /* takes a contiguous array of n items each of the given size to
	a list representation */
{
  int index;
  list front,back,item;
  char *cursor;

  index = 0;
  front = back = NULL;
  cursor = (char *) vector;
  while (*fault ? 0 : (index++ < num_items))
    {
      item = avm_list_of_value((void *) cursor, item_size, fault);
      if (!*fault)
	avm_recoverable_enqueue(&front,&back,item,fault);
      else if (front)
	avm_dispose(front);
      cursor = cursor + item_size;
    }
  return (*fault ? avm_copied(memory_overflow) : front);
}









void
*avm_matrix_of_list(square, upper_triangular, lower_triangular, column_major, operand, item_size, message, fault)
     int square;
     int upper_triangular;
     int lower_triangular;
     int column_major;
     list operand;
     size_t item_size;
     list *message;
     int *fault;

     /* This transforms a list representing a matrix as a list of rows
        to an array. If the square parameter is non-zero, the operand
        is checked for squareness and a fault is raised if it isn't.
        If either triangular parameter is non-zero, the operand is
        checked for the corresponding triangularity. Otherwise the
        operand is checked for rectitude. Lower triangularity means
        the row lengths form an increasing sequence of consecutive
        integers, and upper triangularity has them decreasing.  Upper
        and lower triangularity are mutually exclusive but independent
        of squareness. A square matrix that's also triangular has the
        length of the longest row equal to the number of rows. */
{
  list y,x;
  char *result,*item;
  char *matrix;
  counter i,j,k,l,h,w,lpad,rpad,stride,strides;

  if (*fault = (*fault ? 1 : !!(*message)))
    return NULL;
  *fault = !(item_size ? (operand ? (operand->head ? (item_size == avm_length(operand->head->head)) : 0) : 0) : 0);
  if (*fault = (*fault ? 1 : (upper_triangular ? lower_triangular : 0)))
    *message = avm_copied(bad_matrix_spec);
  h = w = 0;
  y = operand;
  while (*fault ? NULL : y)
    {
      k = avm_recoverable_length(y->head);
      if (*fault = ((++h) ? (k ? 0 : !!(y->head)) : 1))
	*message = (*message ? *message : avm_copied(counter_overflow));
      w = ((w < k) ? k : w);
      y = y->tail;
    }
  if (*fault = (*fault ? 1 : (square ? (h != w) : 0)))
    *message = (*message ? *message : avm_copied(bad_matrix_spec));
  l = w * h;
  matrix = (char*) (result = NULL);
  if (*fault = (*fault ? 1 : (!l ? 1 : !(matrix = (char *) (result = (void *) malloc(item_size * l))))))
    *message = (*message ? *message : avm_copied(memory_overflow));
  stride = (column_major ? (item_size * h) : item_size);
  strides = (column_major ? (item_size * (l - 1)) : 0);
  x = NULL;
  y = operand;
  i = k = lpad = 0;
  rpad = w - 1;
  while (*fault ? 0 : (y ? (i < l) : 0))
    {
      j = 0;
      x = y->head;
      matrix = (upper_triangular ? (matrix + (lpad * stride)) : matrix);
      while (*fault ? 0 : (x ? (i < l) : 0))
	{
	  item = avm_value_of_list(x->head,message,fault);  /* the item size isn't verified only to save time */
	  if (!*fault)
	    memcpy((void *) matrix,(void *) item,item_size);
	  matrix = matrix + stride;
	  x = x->tail;
	  i++;
	  j++;
	}
      matrix = (lower_triangular ? (matrix + (rpad * stride)) : matrix) - strides;
      *fault = (*fault ? 1 : (j + (lower_triangular ? rpad-- : (upper_triangular ? lpad++ : 0)) != w));
      y = y->tail;
    }
  if (!*fault)
    return result;
  if (result)
    free (result);
  *message = (*message ? *message : avm_copied(bad_matrix_spec));
  return NULL;
}









list
avm_list_of_matrix(matrix, rows, cols, item_size, fault)
     void *matrix;
     int rows,cols;
     size_t item_size;
     int *fault;

     /* takes a contiguous array in row major order representing a
        matrix to a list representation as a list of rows with each
        row being a list of entries; can be used in conjunction with
        avm_matrix_transposition, below, to transform column major
        order matrices to row major order; see lapack.c for an
        example */
{
  int index;
  list front,back,item;
  char *cursor;

  if (*fault)
    return NULL;
  index = 0;
  front = back = NULL;
  cursor = (char *) matrix;
  while (*fault ? 0 : (index < rows))
    {
      item = avm_list_of_vector((void *) cursor, cols, item_size, fault);
      if (!*fault)
	avm_recoverable_enqueue(&front,&back,item,fault);
      else if (front)
	avm_dispose(front);
      cursor = cursor + (item_size * cols);
      index++;
    }
  return (*fault ? avm_copied(memory_overflow) : front);
}









list
avm_list_of_packed_matrix(upper_triangular,operand,n,item_size,fault)
     int upper_triangular;
     void *operand;
     int n;
     size_t item_size;
     int *fault;

     /* inverse of the next function */
{
  int i,j;
  list front_column,back_column,front_row,back_row,item;
  char *matrix;

  if (*fault)
    return NULL;
  if (*fault = (n <= 0))
    return avm_copied(bad_matrix_spec);
  matrix = (char *) operand;
  i = 0;
  item = front_row = back_row = NULL;
  while (*fault ? 0 : (i < n))
    {
      j = (upper_triangular ? i : 0);
      front_column = back_column = NULL;
      while (*fault ? 0 : (j < (upper_triangular ? n : (i + 1))))
	{
	  item = avm_list_of_value((void *) &(matrix[packed_index(i,j) * item_size]),item_size,fault);
	  if (!*fault)
	    {
	      avm_recoverable_enqueue(&front_column,&back_column,item,fault);
	      if (*fault)
		item = NULL;
	    }
	  else if (front_column)
	    avm_dispose(front_column);
	  j++;
	}
      if (!*fault)
	avm_recoverable_enqueue(&front_row,&back_row,front_column,fault);
      else
	avm_dispose(front_row);
      i++;
    }
  return (*fault ? (item ? item : avm_copied(memory_overflow)) : front_row);
}










void
*avm_packed_matrix_of_list(upper_triangular,operand,n,item_size,message,fault)
     int upper_triangular;
     list operand;
     int n;
     size_t item_size;
     list *message;
     int *fault;

     /* The operand is a list representing an n by n symmetric square
	matrix A and the result is a packed array representation m
	with redundant entries omitted the way lapack likes it. That
	is, A[i,j] is stored at m[packed_index(i,j)] using the macro
	defined above. One of two alternative packed representations
	is selected depending on the upper_triangular parameter. The
	operand must be a list of n lists, with each list representing
	a row of the matrix.  Either all items of the operand can be
	of length n, or their lengths can range from 1 to n in either
	a strictly increasing or strictly decreasing sequence. If they
	are all of length n, the upper_triangular parameter determines
	not only the output representation but the choice of items
	from the input that are used. (Symmetry is not checked and
	unused items are ignored.)  If they form an increasing
	sequence, the lower triangular representation is implied and
	upper_triangular must be 0. Otherwise the upper triangular
	representation is implied and upper_triangular must be 1. */

{
  char *matrix;
  char *item,*result;
  int i,j,k,square;
  list row,column;

  if (*fault = (*fault ? 1 : !!(*message)))
    return NULL;
  if (*fault = !(matrix = (char *) (result = (void *) malloc(((n*(n+1))>>1) * item_size))))
    *message = avm_copied(memory_overflow);
  square = (operand ? (operand->tail ? (avm_length(operand->head) == avm_length(operand->tail->head)) : 0) : 0);
  i = 0;
  row = operand;
  while (*fault ? 0 : (i < n))
    {
      j = (upper_triangular ? i : 0);
      column = ((*fault = !row) ? NULL : row->head);
      k = i;
      if (square ? upper_triangular : 0)
	while (*fault ? 0 : k--)
	  column = ((*fault = !column) ? NULL : column->tail);
      while (*fault ? 0 : (j < (upper_triangular ? n : (i + 1))))
	{
	  item = ((*fault = !column) ? NULL : avm_value_of_list (column->head, message, fault));
	  if (!*fault)
	    {
	      memcpy((void *) &(matrix[packed_index(i,j) * item_size]),(void *) item,item_size);
	      column = column->tail;
	    }
	  j++;
	}
      i++;
      k = n - i;
      if (square ? !upper_triangular : 0)
	while (*fault ? 0 : k--)
	  column = ((*fault = !column) ? NULL : column->tail);
      if (!(*fault = (*fault ? 1 : !!column)))
	row = row->tail;
    }
  if (!(*fault = (*fault ? 1 : !!row)))
    return result;
  if (result)
    free (result);
  *message = (*message ? *message : avm_copied(bad_matrix_spec));
  return NULL;
}












void
*avm_matrix_transposition(matrix,rows,cols,item_size)
     void *matrix;
     int rows;
     int cols;
     size_t item_size;

     /* constant space transposition of a contiguous row major ordered
	general rectangular matrix in place; can also be used on a
	matrix in column major order by interchanging the rows and
	cols parameters */
{
#define ALIGNMENT 16         /* power of 2 >= minimum array boundary alignment; maybe unnecessary but machine dependent */

  char *cursor;
  char carry[ALIGNMENT];
  size_t block_size,remaining_size;
  int nadir,lag,orbit,ents;

  if ((rows == 1) ? 1 : (cols == 1))
    return matrix;
  ents = rows * cols;
  cursor = (char *) matrix;
  remaining_size = item_size;
  block_size = ((ALIGNMENT < remaining_size) ? ALIGNMENT : remaining_size);
  while (block_size)
    {
      nadir = 1;                                    /* first and last entries are always fixed points so aren't visited */
      while (nadir + 1 < ents)
	{
	  memcpy(carry,&(cursor[(lag = nadir) * item_size]),block_size);
	  while ((orbit = (lag / rows) + cols * (lag % rows)) > nadir)                       /* follow a complete cycle */
	    {
	      memcpy(&(cursor[lag * item_size]),&(cursor[orbit * item_size]),block_size);
	      lag = orbit;
	    }
	  memcpy(&(cursor[lag * item_size]),carry,block_size);
	  orbit = nadir++;
	  while ((orbit < nadir) ? (nadir + 1 < ents) : 0)     /* find the next unvisited index by an exhaustive search */
	    {
	      orbit = nadir;
	      while ((orbit = (orbit / rows) + cols * (orbit % rows)) > nadir);
	      nadir = ((orbit < nadir) ? nadir + 1 : nadir);
	    }
	}
      cursor = cursor + block_size;
      remaining_size = remaining_size - block_size;
      block_size = ((ALIGNMENT < remaining_size) ? ALIGNMENT : remaining_size);
    }
  return matrix;
}







void
*avm_matrix_reflection(upper_triangular,matrix,n,item_size)
     int upper_triangular;
     void *matrix;
     int n;
     size_t item_size;

     /* fills in the redundant entries in a symmetric square matrix;
	if upper_triangular is non-zero, the upper triangle is copied
	to the lower triangle, and otherwise the lower is copied to
	the upper */
{
  int i,j,rows,cols;
  char *cursor;

  rows = cols = n;
  cursor = (char *) matrix;
  for (i = 0; i < rows; i++)
    for (j = (upper_triangular ? (i + 1) : 0); j < (upper_triangular ? cols : i); j++)
      memcpy(&(cursor[((j * cols) + i) * item_size]),&(cursor[((i * cols) + j) * item_size]),item_size);
}










list
*avm_row_number_array(m,fault)
     counter m;
     int *fault;

     /* returns an array of m lists whose ith element is the
	representation of the natural number i */
{
  list *result;
  counter i;

  if (*fault = (*fault ? 1 : !(result = (list *) malloc(sizeof(list) * m))))
    return NULL;
  result[0] = NULL;
  for (i = 1; i < m; i++)
    {
      if (*fault)
	result[i] = NULL;
      else 
	result[i] = avm_recoverable_join((i & 1) ? avm_copied (shared_cell) : NULL,avm_copied(result[i >> 1]));
      *fault = (*fault ? 1 : !(result[i]));
    }
  if (!*fault)
    return result;
  for (i = 1; i < m; i++)
    if (result[i])
      avm_dispose (result[i]);
  free (result);
  return NULL;
}








void
avm_dispose_rows(m,row_number)
     counter m;
     list *row_number;

     /* frees a structure allocated as above */
{
  counter i;

  if (row_number)
    {
      for (i = 1; i < m; i++)
	avm_dispose (row_number[i]);
      free (row_number);
    }
}










void
avm_initialize_matcon ()

     /* This initializes some static data structures and must be
	called before any other function in this file is called. */
{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_listfuns ();
  avm_initialize_chrcodes ();
  shared_cell = avm_join (NULL, NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  counter_overflow = avm_join (avm_strung ("counter overflow"), NULL);
  bad_matrix_spec = avm_join (avm_strung ("bad matrix specification"), NULL);
  bad_vector_spec = avm_join (avm_strung ("bad vector specification"), NULL);
}






void
avm_count_matcon ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (shared_cell);
  avm_dispose (bad_matrix_spec);
  avm_dispose (bad_vector_spec);
  avm_dispose (memory_overflow);
  avm_dispose (counter_overflow);
  shared_cell = NULL;
  memory_overflow = NULL;
  bad_vector_spec = NULL;
  bad_matrix_spec = NULL;
  counter_overflow = NULL;
}

