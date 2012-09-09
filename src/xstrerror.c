/* xstrerror.c -- strerror wrapper with bound checking
   Fri Jun 16 18:30:00 1995  Pat Rankin  <rankin@eql.caltech.edu>
   This code is in the public domain.

   Modified by Dennis Furey, February 26, 2001, to use the
   HAVE_STRERROR symbol in case this is being compiled on a system
   that doesn't have it. */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>

#if HAVE_STRERROR
extern char *strerror ();
#else
char *strerror (){return(NULL);}
#endif

/* If strerror returns NULL, we'll format the number into a static buffer.  */
#define ERRSTR_FMT "undocumented error #%d"
static char xstrerror_buf[sizeof ERRSTR_FMT + 20];

/* This is like strerror, but result is never a null pointer.  */
char *
xstrerror (errnum)
     int errnum;
{
  char *errstr = strerror (errnum);

  /* If `errnum' is out of range, result might be NULL.  We'll fix that.  */
  if (!errstr)
    {
      sprintf (xstrerror_buf, ERRSTR_FMT, errnum);
      errstr = xstrerror_buf;
    }

  return errstr;
}
