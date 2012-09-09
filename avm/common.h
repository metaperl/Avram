#ifndef AVM_COMMON_H
#define AVM_COMMON_H 1

/* adapted from the autobook reference */

#ifdef __cplusplus
extern "C"
{
#endif

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>

#define _GNU_SOURCE
#define _REENTRANT

#if STDC_HEADERS
#  include <stdlib.h>
#include <string.h>
#elif HAVE_STRINGS_H
#include <strings.h>
#endif				/*STDC_HEADERS */

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif				/*HAVE_ERRNO_H */

  typedef uintmax_t counter;

#ifndef errno
/* Some systems #define this! */
#include <errno.h>
  extern int errno;
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS  0
#define EXIT_FAILURE  1
#endif

  extern char *xstrerror (int errnum);


#ifdef __cplusplus
}
#endif

#endif				/* AVM_COMMON_H */
