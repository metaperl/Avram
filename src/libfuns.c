
/* this file glues external libraries into the virtual machine

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
#include <avm/chrcodes.h>
#include <avm/listfuns.h>
#include <avm/mathlib.h>
#include <avm/rmathlib.h>
#include <avm/gslintlib.h>
#include <avm/glpklib.h>
#include <avm/gsldiflib.h>
#include <avm/gslevu.h>
#include <avm/complexlib.h>
#include <avm/mtwist.h>
#include <avm/umf.h>
#include <avm/mpfr.h>
#include <avm/lapack.h>
#include <avm/fftw.h>
#include <avm/minpack.h>
#include <avm/kinsol.h>
#include <avm/libfuns.h>
#include <avm/bes.h>
#include <avm/lpsolve.h>
#include <avm/harminv.h>

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list unrecognized_library = NULL;
static list memory_overflow = NULL;

/* library names as lists of lists of character representations */
static list libs = NULL;
static list wild_libs = NULL;

/* wild card search pattern */
static list wild = NULL;




list
avm_library_call (library_name, function_name, argument, fault)
     list library_name;
     list function_name;
     list argument;
     int *fault;

     /* This figures out what library to call and calls it. */
{
  list message;
  int library_number;

  if (! initialized)
      avm_initialize_libfuns ();
  if (*fault)
    return NULL;
  library_number = 0xff & (library_name ? library_name->characterization : 0);
  if (! library_number)
    {
      message = avm_position (library_name, libs, fault);
      if (*fault)
	return (message);
      if (*fault = !message)
	return (avm_copied(unrecognized_library));
      library_number = message->characterization;
      library_name->characterization = library_number;
      avm_dispose (message);
    }
  switch (library_number)
    {
    case 1: return avm_math_call (function_name, argument, fault);
    case 2: return avm_complex_call (function_name, argument, fault);
    case 3: return avm_rmath_call (function_name, argument, fault);
    case 4: return avm_mtwist_call (function_name, argument, fault);
    case 5: return avm_gslint_call (function_name, argument, fault);
    case 6: return avm_gsldif_call (function_name, argument, fault);
    case 7: return avm_gslevu_call (function_name, argument, fault);
    case 8: return avm_glpk_call (function_name, argument, fault);
    case 9: return avm_umf_call (function_name, argument, fault);
    case 10: return avm_mpfr_call (function_name, argument, fault);
    case 11: return avm_lapack_call (function_name, argument, fault);
    case 12: return avm_fftw_call (function_name, argument, fault);
    case 13: return avm_minpack_call (function_name, argument, fault);
    case 14: return avm_kinsol_call (function_name, argument, fault);
    case 15: return avm_bes_call (function_name, argument, fault);
    case 16: return avm_lpsolve_call (function_name, argument, fault);
    case 17: return avm_harminv_call (function_name, argument, fault);
    default: *fault = 1;
    }
  return avm_copied (unrecognized_library);
}






list
avm_have_library_call (library_name, function_name, fault)
     list library_name;
     list function_name;
     int *fault;

     /* This checks whether a library function is available. */

{
  list looked_up;
  list results;
  list temporary;
  list reversal;
  list library_names;
  int library_number;

  if (! initialized)
    avm_initialize_libfuns ();
  if (*fault)
    return NULL;
  results = NULL;
  looked_up = avm_position (library_name, wild_libs, fault);
  if (*fault)
    return (looked_up);
  if (!looked_up)
    return (NULL);
  library_number = looked_up->characterization;
  avm_dispose(looked_up);
  looked_up = NULL;
  if (library_number == 1)
    library_names = avm_copied(libs);
  else
    {
      library_number--;
      if (*fault = !(library_names = avm_recoverable_join(avm_copied(library_name),NULL)))
	return avm_copied (memory_overflow);
    }
  while (*fault ? NULL : library_names)
    {
      switch (library_number)
	{
	case 1: looked_up = avm_have_math_call (function_name, fault); break;
	case 2: looked_up = avm_have_complex_call (function_name, fault); break;
	case 3: looked_up = avm_have_rmath_call (function_name, fault); break;
	case 4: looked_up = avm_have_mtwist_call (function_name, fault); break;
	case 5: looked_up = avm_have_gslint_call (function_name, fault); break;
	case 6: looked_up = avm_have_gsldif_call (function_name, fault); break;
	case 7: looked_up = avm_have_gslevu_call (function_name, fault); break;
	case 8: looked_up = avm_have_glpk_call (function_name, fault); break;
	case 9: looked_up = avm_have_umf_call (function_name, fault); break;
	case 10: looked_up = avm_have_mpfr_call (function_name, fault); break;
	case 11: looked_up = avm_have_lapack_call (function_name, fault); break;
	case 12: looked_up = avm_have_fftw_call (function_name, fault); break;
	case 13: looked_up = avm_have_minpack_call (function_name, fault); break;
	case 14: looked_up = avm_have_kinsol_call (function_name, fault); break;
	case 15: looked_up = avm_have_bes_call (function_name, fault); break;
	case 16: looked_up = avm_have_lpsolve_call (function_name, fault); break;
	case 17: looked_up = avm_have_harminv_call (function_name, fault); break;
	default: *fault = 1;
	  looked_up = avm_copied (unrecognized_library);
	}
      temporary = NULL;
      *fault = (*fault ? 1 : !(temporary = avm_recoverable_join(avm_copied(library_names->head),looked_up)));
      looked_up = (*fault ? NULL : avm_distribution(temporary,fault));
      avm_dispose(temporary);
      library_names = avm_copied((temporary = library_names)->tail);
      avm_dispose(temporary);
      library_number++;
      while (*fault ? NULL : looked_up)
	{
	  results = avm_recoverable_join(avm_copied(looked_up->head),results);
	  looked_up = avm_copied((temporary = looked_up)->tail);
	  avm_dispose(temporary);
	  if (*fault = !results)
	    {
	      avm_dispose(looked_up);
	      looked_up = NULL;
	    }
	}
    }
  avm_dispose(library_names);
  if (*fault)
    {
      avm_dispose (results);
      return (looked_up ? looked_up : avm_copied (memory_overflow));
    }
  reversal = NULL;
  while (results)
    {
      temporary = results->tail;
      results->tail = reversal;
      reversal = results;
      results = temporary;
    }
  return reversal;
}







void
avm_initialize_libfuns ()

     /* This initializes some static data structures. */

{
  list back;
  int string_number;
  char *libnames[] = {
    "math",
    "complex",
    "rmath",
    "mtwist",
    "gslint",
    "gsldif",
    "gslevu",
    "glpk",
    "umf",
    "mpfr",
    "lapack",
    "fftw",
    "minpack",
    "kinsol",
    "bes",
    "lpsolve",
    "harminv",
    NULL};            /* add more library names here up to a total of 255 */

  if (initialized)
    return;
  initialized = 1;
  avm_initialize_math ();
  avm_initialize_lists ();
  avm_initialize_listfuns ();
  avm_initialize_complex ();
  avm_initialize_rmath ();
  avm_initialize_mtwist ();
  avm_initialize_gslint ();
  avm_initialize_gsldif ();
  avm_initialize_gslevu ();
  avm_initialize_glpk ();
  avm_initialize_umf ();
  avm_initialize_mpfr ();
  avm_initialize_lapack ();
  avm_initialize_fftw ();
  avm_initialize_minpack ();
  avm_initialize_kinsol ();
  avm_initialize_bes ();
  avm_initialize_lpsolve ();
  avm_initialize_harminv ();
  avm_initialize_chrcodes ();
  wild = avm_strung ("*");
  unrecognized_library = avm_join (avm_strung ("unrecognized library"), NULL);
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  string_number = 0;
  libs = back = NULL;
  while (libnames[string_number])
    avm_enqueue (&libs, &back, avm_standard_strung (libnames[string_number++]));
  wild_libs = avm_join(avm_copied(wild),avm_copied(libs));
}







void
avm_count_libfuns ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{
  if (!initialized)
    return;
  avm_count_math ();
  avm_count_listfuns ();
  avm_count_complex ();
  avm_count_rmath ();
  avm_count_mtwist ();
  avm_count_gslint ();
  avm_count_gsldif ();
  avm_count_gslevu ();
  avm_count_glpk ();
  avm_count_umf ();
  avm_count_mpfr ();
  avm_count_lapack ();
  avm_count_fftw ();
  avm_count_minpack ();
  avm_count_kinsol ();
  avm_count_bes ();
  avm_count_lpsolve ();
  avm_count_harminv ();
  initialized = 0;
  avm_dispose (libs);
  avm_dispose (wild_libs);
  avm_dispose (unrecognized_library);
  avm_dispose (memory_overflow);
  avm_dispose (wild);
  libs = NULL;
  unrecognized_library = NULL;
}
