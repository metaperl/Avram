
/* virtual glue code generating functions

   Copyright (C) 2010 Dennis Furey

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

#include <avm/chrcodes.h>
#include <avm/common.h>
#include <avm/error.h>
#include <avm/lists.h>

/* Non-zero means static variables have been initialized. */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list memory_overflow = NULL;

/* used for constructing virtual code */
static list shared_cell = NULL;

/* constants used to construct a merging function from a predicate */
static list merge_0 = NULL;
static list merge_1 = NULL;
static list merge_2 = NULL;
static list merge_3 = NULL;
static list merge_4 = NULL;







list
avm_compose (operator, preprocessor, fault)
     list operator;
     list preprocessor;
     int *fault;

     /* returns the virtual machine code for the composition of a pair of functions */
{
  if (!initialized)
    avm_initialize_vglue ();
  *fault = (*fault ? 1 : !operator ? 1 : !preprocessor);
  if (!operator ? 0 : operator->tail ? 0 : !(operator->head) ? 0 : !(operator->head->head))
    {
      avm_dispose (preprocessor);
      preprocessor = NULL;
    }
  else
    {
      *fault = (*fault ? 1 : !(operator = avm_recoverable_join (operator, preprocessor)));
      *fault = (*fault ? 1 : !(operator = avm_recoverable_join (operator, NULL)));
    }
  if (*fault)
    {
      avm_dispose (operator);
      avm_dispose (preprocessor);
      return NULL;
    }
  return operator;
}









list
avm_map (operator, fault)
     list operator;
     int *fault;

     /* returns the virtual machine code for the map of a function */
{
  if (!initialized)
    avm_initialize_vglue ();
  *fault = (*fault ? 1 : ! operator);
  *fault = (*fault ? 1 : ! (operator = avm_recoverable_join (NULL, operator)));
  *fault = (*fault ? 1 : ! (operator = avm_recoverable_join (operator, NULL)));
  *fault = (*fault ? 1 : ! (operator = avm_recoverable_join (avm_copied (shared_cell), operator)));
  if (*fault)
    {
      avm_dispose (operator);
      return NULL;
    }
  return operator;
}







list
avm_reduce (operator, fault)
     list operator;
     int *fault;

     /* returns the virtual machine code for the reduce of a function with an empty vacuous case */
{
  if (!initialized)
    avm_initialize_vglue ();
  *fault = (*fault ? 1 : ! operator);
  *fault = (*fault ? 1 : ! (operator = avm_recoverable_join (operator, NULL)));
  *fault = (*fault ? 1 : ! (operator = avm_recoverable_join (operator, NULL)));
  *fault = (*fault ? 1 : ! (operator = avm_recoverable_join (avm_copied (shared_cell), operator)));
  if (*fault)
    {
      avm_dispose (operator);
      return NULL;
    }
  return operator;
}









list
avm_sort (operator, fault)
     list operator;
     int *fault;

     /* takes a predicate to a function that sorts a list according to it */
{
  if (!initialized)
    avm_initialize_vglue ();
  *fault = (*fault ? 1 : ! operator);
  *fault = (*fault ? 1 : ! (operator = avm_recoverable_join (operator, NULL)));
  *fault = (*fault ? 1 : ! (operator = avm_recoverable_join (operator, avm_copied (shared_cell))));
  *fault = (*fault ? 1 : ! (operator = avm_recoverable_join (avm_copied (shared_cell), operator)));
  if (*fault)
    {
      avm_dispose (operator);
      return NULL;
    }
  return operator;
}










list
avm_merge (predicate, fault)
     list predicate;
     int *fault;

     /* takes a predicate to a function that merges two lists according to it */
{
  if (!initialized)
    avm_initialize_vglue ();
  *fault = (*fault ? 1 : ! predicate);
  *fault = (*fault ? 1 : ! (predicate = avm_recoverable_join (predicate, avm_copied (merge_0))));
  *fault = (*fault ? 1 : ! (predicate = avm_recoverable_join (predicate, NULL)));
  *fault = (*fault ? 1 : ! (predicate = avm_recoverable_join (predicate, avm_copied (merge_1))));
  *fault = (*fault ? 1 : ! (predicate = avm_recoverable_join (predicate, avm_copied (merge_2))));
  *fault = (*fault ? 1 : ! (predicate = avm_recoverable_join (avm_copied (merge_3), predicate)));
  *fault = (*fault ? 1 : ! (predicate = avm_recoverable_join (predicate, avm_copied (merge_4))));
  *fault = (*fault ? 1 : ! (predicate = avm_recoverable_join (avm_copied (merge_4), predicate)));
  *fault = (*fault ? 1 : ! (predicate = avm_recoverable_join (predicate, avm_copied (merge_3))));
  *fault = (*fault ? 1 : ! (predicate = avm_recoverable_join (predicate, NULL)));
  *fault = (*fault ? 1 : ! (predicate = avm_recoverable_join (predicate, NULL)));
  *fault = (*fault ? 1 : ! (predicate = avm_recoverable_join (predicate, NULL)));
  if (*fault)
    {
      avm_dispose (predicate);
      return NULL;
    }
  return predicate;
}












void
avm_initialize_vglue ()

     /* This initializes static data structures. */
{
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  shared_cell = avm_join (NULL, NULL);
  merge_0 = avm_scanned_list ("goL<");
  merge_1 = avm_scanned_list ("yHip]n\\");
  merge_2 = avm_scanned_list ("yHgp]nD");
  merge_3 = avm_scanned_list ("f\\");
  merge_4 = avm_scanned_list ("g<");
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
}










void
avm_count_vglue ()

{
  if (!initialized)
    return;
  avm_dispose (memory_overflow);
  avm_dispose (merge_0);
  avm_dispose (merge_1);
  avm_dispose (merge_2);
  avm_dispose (merge_3);
  avm_dispose (merge_4);
  avm_dispose (shared_cell);
  shared_cell = NULL;
  merge_0 = NULL;
  merge_1 = NULL;
  merge_2 = NULL;
  merge_3 = NULL;
  merge_4 = NULL;
  memory_overflow = NULL;
  initialized = 0;
}
