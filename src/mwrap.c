
/* This file contains some routines to intercept calls to malloc and
   to inhibit output operations to compensate for badly designed
   external libraries.

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
   Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA

*/

#include <avm/common.h>
#include <avm/error.h>
#include <avm/mwrap.h>
#include <unistd.h>
#include <fcntl.h>
#if HAVE_MALLOC
#include <malloc.h>
#endif

/* points to a list of blocks of memory */
typedef struct memory_node *areas;

struct memory_node
{
  void *first_area;
  size_t size;
  areas other_areas;
};


/* points to a stack of jump buffers */
typedef struct jump_stack_node *jump_stack;

struct jump_stack_node
{
  jmp_buf *latest_jump;
  jump_stack other_jumps;
};


/* a non-zero value means static variables have been initialized */
static int initialized = 0;

/* a non-zero value means messages can be printed to stdout */
static int stderr_on = 1;

/* a non-zero value means messages can be printed to stderr */
static int stdout_on = 1;

/* file handles */
static int offline_stdout = 0;
static int offline_stderr = 0;

/* causes error messages to be printed */
static int debug_mode = 0;

/* non-zero if memory management is active */
static int management_mode = 0;

/* records all memory allocated while the wrapper is active */
static areas managed_memory;

/* temporary storage for the malloc and free hooks provided by libc */
static void *real_malloc;
static void *real_realloc;
static void *real_free;

/* stack of destinations for long jumps in case of allocation failures */
static jump_stack jumps = NULL;

/* the number of jmp_bufs that tried to be created but couldn't */
static counter jump_stack_overflow = 0;

/* used for temporary storage by the avm_setjmp () macro */
jmp_buf *_avm_mwrap_client;


#if !HAVE_SETJMP

int
setjmp (_avm_mwrap_client)
     jmp_buf _avm_mwrap_client;

     /* replacement function that does nothing if the host platform
	doesn't have setjmp */
{
  return 0;
}

#endif






static void
*malloc_wrapper (size, caller)
     size_t size;
     const void *caller;

/* This works around library functions that don't recover gracefully
   from insufficient memory. Any time a function calls malloc, the
   call gets redirected here. If there's insufficient memory, it
   doesn't return to the caller but jumps back to whatever function
   has initialized the jmp_buf _avm_mwrap_client. This function also
   makes a list of all memory areas allocated while it's active so
   they can be freed reliably. */
{
  void *creation;
  areas insertion;

#if HAVE_MALLOC
  avm_dont_manage_memory ();
  if (!(insertion = malloc(sizeof(*insertion))))
    {
#if HAVE_SETJMP
      if (jumps ? (jumps->latest_jump ? !jump_stack_overflow : 0) : 0)
	{
	  avm_free_managed_memory ();
	  longjmp (*(jumps->latest_jump), -1);
	}
#endif
      avm_manage_memory ();                   /* can't escape so the caller better know what to do with a NULL pointer */
      return NULL;
    }
  if (!(creation = malloc (size)))
    {
      free (insertion);
#if HAVE_SETJMP
      if (jumps ? (jumps->latest_jump ? !jump_stack_overflow : 0) : 0)
	{
	  avm_free_managed_memory ();
	  longjmp (*(jumps->latest_jump), -1);
	}
#endif
      avm_manage_memory ();
      return NULL;
    }
  insertion->first_area = creation;
  insertion->size = size;
  insertion->other_areas = managed_memory;
  managed_memory = insertion;
  if (debug_mode)
    printf ("malloc (%u) returns %p\n", (unsigned int) size, creation);
  avm_manage_memory ();
  return creation;
#endif
  avm_internal_error (29);
}






static void
free_wrapper (ptr, caller)
     void *ptr;
     const void *caller;

/* This intercepts calls to free by library functions. If it's freeing
   something that was allocated by the library, the area is removed
   from the list of managed memory. If it tries to free something that
   wasn't allocated, that's a bug in the library which may have
   corrupted the rest of memory so the program is terminated. */
{
  areas deletion,predecessor;

#if HAVE_MALLOC
  avm_dont_manage_memory ();
  if (debug_mode)
    printf ("freeing pointer %p\n", ptr);
  if (!ptr)
    {                           /* in case anyone feels the need to free null pointers */
      avm_manage_memory ();
      return;
    }
  if (managed_memory ? (managed_memory->first_area == ptr) : 0)
    {
      deletion = managed_memory;
      managed_memory = managed_memory->other_areas;
    }
  else
    {
      predecessor = managed_memory;
      while (predecessor ? (predecessor->other_areas ? (predecessor->other_areas->first_area != ptr) : 0) : 0)
	predecessor = predecessor->other_areas;
      if (!(predecessor ? (predecessor->other_areas ? (predecessor->other_areas->first_area == ptr) : 0) : 0))
	avm_internal_error (70);
      deletion = predecessor->other_areas;
      predecessor->other_areas = deletion->other_areas;
    }
  free (deletion->first_area);
  free (deletion);
  avm_manage_memory ();
  return;
#endif
  avm_internal_error (30);
}







static void
*realloc_wrapper (ptr, size, caller)
     void *ptr;
     size_t size;
     const void *caller;
{
  void *new_address;
  areas location;

#if HAVE_MALLOC
  avm_dont_manage_memory ();
  if (!(new_address = realloc (ptr,size)))
    {
#if HAVE_SETJMP
      if (jumps ? (jumps->latest_jump ? !jump_stack_overflow : 0) : 0)
	{
	  avm_free_managed_memory ();
	  longjmp (*(jumps->latest_jump), -1);
	}
#endif
      avm_manage_memory ();
      return NULL;
    }
  if (debug_mode)
    printf ("realloc (%p,%u) returns %p\n", (unsigned int) ptr, size, new_address);
  if (new_address != ptr)
    {
      location = managed_memory;
      while (location ? (location->first_area != ptr) : 0)
	location = location->other_areas;
      if (!location)
	avm_internal_error(78);
      location->first_area = new_address;
      location->size = size;
    }
  avm_manage_memory ();
  return new_address;
#endif
  avm_internal_error (31);
}








jmp_buf 
*avm_new_jmp_buf()

/* pushes a jump buffer onto the stack and returns its address, or
   else increments stack overflow */
{
  jump_stack new_top;
  int saved_management_mode;

#if HAVE_SETJMP
  if (jump_stack_overflow ? jump_stack_overflow++ : 0)
    return NULL;
  if (saved_management_mode = management_mode)
    avm_dont_manage_memory ();
  if (new_top = (jump_stack) malloc (sizeof(*new_top)))
    {
      if (new_top->latest_jump = (jmp_buf *) malloc(sizeof(jmp_buf)))
	{
	  new_top->other_jumps = jumps;
	  jumps = new_top;
	  if (management_mode = saved_management_mode)
	    avm_manage_memory ();
	  return jumps->latest_jump;
	}
      free (new_top);
    }
  jump_stack_overflow++;
  if (management_mode = saved_management_mode)
    avm_manage_memory ();
#endif
  return NULL;
}









void
avm_setnonjmp ()

/* This pushes an empty jump buffer so that jumps will be inhibited
   rather than falling through to the one below. Failure is not an
   option, so if there's insufficient memory, it bumps the
   jump_stack_overflow, which will have the same effect as a
   non-destination in the wrapper functions. In that case, no further
   real or non-destinations will be pushable until after all the
   overflows have been cleared, but more non-destinations can still be
   simulated by overflows. */
{
  jump_stack new_top;
  int saved_management_mode;

#if HAVE_SETJMP
  if (jump_stack_overflow ? jump_stack_overflow++ : 0)
    return;
  if (saved_management_mode = management_mode)
    avm_dont_manage_memory ();
  if (new_top = (jump_stack) malloc (sizeof(*new_top)))
    {
      new_top->latest_jump = NULL;
      new_top->other_jumps = jumps;
      jumps = new_top;
      if (management_mode = saved_management_mode)
	avm_manage_memory ();
      return;
    }
  jump_stack_overflow++;
  if (management_mode = saved_management_mode)
    avm_manage_memory ();
#endif
}









void
avm_clearjmp ()

/* decrements stack_overflow if it's positive, otherwise pops a jump
   buffer or place holder from the stack */
{
  int saved_management_mode;
  jump_stack deletion;

#if HAVE_SETJMP
  if (jump_stack_overflow ? jump_stack_overflow-- : 0)
    return;
  if (saved_management_mode = management_mode)
    avm_dont_manage_memory ();
  if (!jumps)
    avm_internal_error (1);
  jumps = (deletion = jumps)->other_jumps;
  if (deletion->latest_jump)
    free (deletion->latest_jump);
  free (deletion);
  if (management_mode = saved_management_mode)
    avm_manage_memory ();
#endif
}









void
avm_debug_memory ()

/* causes error messages to be printed during memory management */
{
  debug_mode = 1;
}







void
avm_dont_debug_memory ()

/* stops error messages from being printed during memory management */
{
  debug_mode = 0;
}







inline void
avm_manage_memory ()

/* redirects calls to malloc and free to the wrappers defined above */
{
  management_mode = 1;
#if HAVE_MALLOC
  __malloc_hook = malloc_wrapper;
  __realloc_hook = realloc_wrapper;
  __free_hook = free_wrapper;
#endif
}







inline void
avm_dont_manage_memory ()

/* cancels redirection */
{
  management_mode = 0;
#if HAVE_MALLOC
  __malloc_hook = real_malloc;
  __realloc_hook = real_realloc;
  __free_hook = real_free;
#endif
}








void
avm_free_managed_memory ()

/* this frees all the memory in the list of allocated areas and also
   cancels redirection */
{
  areas previous;

  avm_dont_manage_memory ();
  while (managed_memory)
    {
      managed_memory = (previous = managed_memory)->other_areas;
      free(previous->first_area);
      free(previous);
    }
}








inline void
avm_turn_off_stdout ()

/* temporarily disable stdout */
{
  if (!stdout_on)
    return;
  stdout_on = 0;
  offline_stdout = dup (STDOUT_FILENO);
  fclose (stdout);
}









inline void
avm_turn_on_stdout ()

/* restore stdout if it was off */
{
  if (stdout_on)
    return;
  stdout_on = 1;
  dup2 (offline_stdout, STDOUT_FILENO);
  stdout = fdopen(STDOUT_FILENO,"w");    /* fixme: how to do this on non-GNU systems where stdout is a macro */
}










void
avm_turn_off_stderr ()

/* temporarily disable stderr */
{
  if (!stderr_on)
    return;
  stderr_on = 0;
  offline_stderr = dup (STDERR_FILENO);
  fclose (stderr);
}










void
avm_turn_on_stderr ()

/* restore stderr if it was off */
{
  if (stderr_on)
    return;
  stderr_on = 1;
  dup2 (offline_stderr, STDERR_FILENO);
  stderr = fdopen(STDERR_FILENO,"w");    /* fixme: how to do this on non-GNU systems where stderr is a macro */
}







void
avm_initialize_mwrap ()

     /* This initializes some static data structures. */
{
  if (initialized)
    return;
  initialized = 1;
#if HAVE_MALLOC
  real_free = __free_hook;
  real_malloc = __malloc_hook;
  real_realloc = __realloc_hook;
#endif
}








void
avm_count_mwrap ()

/* this checks for memory leaks */

{
  counter unreclaimed;

  if (!initialized)
    return;
  if (!stdout_on)
    avm_turn_on_stdout ();
  if (!stderr_on)
    avm_turn_on_stderr ();
  avm_dont_manage_memory ();
  unreclaimed = initialized = 0;
  while (managed_memory)
    {
      if (debug_mode)
	printf("%d bytes unreclaimed at pointer %p\n", managed_memory->size, managed_memory->first_area);
      unreclaimed = unreclaimed + managed_memory->size;
      managed_memory = managed_memory->other_areas;
    }
  if (unreclaimed)
    avm_reclamation_failure ("managed memory bytes", unreclaimed);
  unreclaimed = 0;
  while (jumps)
    {
      unreclaimed++;
      jumps = jumps->other_jumps;
    }
  if (unreclaimed)
    avm_reclamation_failure ("jump buffers", unreclaimed);
}
