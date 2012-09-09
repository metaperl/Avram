
/* this file interfaces to a couple of linear algebra functions from lapack

   Copyright (C) 2006,2009 Dennis Furey

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
#include <avm/matcon.h>
#include <avm/chrcodes.h>
#include <avm/lapack.h>
#if HAVE_LAPACK
typedef double complex[2];
#define RE 0
#define IM 1
#endif

/* non-zero means static variables are initialized */
static int initialized = 0;

/* error messages as lists of lists of character representations */
static list lapack_error = NULL;
static list bad_lapack_spec = NULL;
static list memory_overflow = NULL;
static list unrecognized_function_name = NULL;

/* function names as lists of lists of character representations */
static list wild = NULL;
static list funs = NULL;

#define MIN(x,y) ((x < y) ? x : y)
#define MAX(x,y) ((x < y) ? y : x)

/* the smallest number whose inverse is finitely representable */
static double safemin = 0.0;

#if HAVE_LAPACK

/* use the LU factorization to compute the solution to a real system of linear equations A * X = B */
extern void
dgesvx_(char *fact, char *trans, int *n, int *nrhs, double *a, int *lda, double *af, int *ldaf, int *ipiv, char *equed,
	double *r, double *c, double *b, int *ldb, double *x, int *ldx, double *rcond, double *ferr, double *berr,
	double *work, int *iwork, int *info);

/* use the LU factorization to compute the solution to a complex system of linear equations A * X = B */
extern void
zgesvx_(char *fact, char *trans, int *n, int *nrhs, complex *a, int *lda, complex *af, int *ldaf, int *ipiv,
	char *equed, double *r, double *c, complex *b, int *ldb, complex *x, int *ldx, double *rcond, double *ferr,
	double *berr, complex *work, double *rwork, int *info);

/* compute the singular value decomposition (SVD) of a real M-by-N matrix A */
extern void
dgesdd_(char *jobz, int *m, int *n, double *a, int *lda, double *s, double *u, int *ldu, double *vt, int *ldvt,
	double *work, int *lwork, int *iwork, int *info);

/* compute the singular value decomposition (SVD) of a complex M-by-N matrix A */
extern void
zgesdd_(char *jobz, int *m, int *n, complex *a, int *lda, double *s, complex *u, int *ldu, complex *vt, int *ldvt,
	complex *work, int *lwork, double *rwork, int *iwork, int *info);

/* compute the minimum-norm solution to a real linear least squares problem */
extern void
dgelsd_(int *m, int *n, int *nrhs, double *a, int *lda, double *b, int *ldb, double *s, double *rcond, int *rank,
	double *work, int *lwork, int *iwork, int *info);

/* compute the minimum-norm solution to a complex linear least squares problem */
extern void
zgelsd_(int *m, int *n, int *nrhs, complex *a, int *lda, complex *b, int *ldb, double *s, double *rcond, int *rank,
	complex *work, int *lwork, double *rwork, int *iwork, int *info);

/* compute all the eigenvalues and, optionally, eigenvectors of a real symmetric matrix A in packed storage */
extern void
dspev_(char *jobz, char *uplo, int *n, double *ap, double *w, double *z, int *ldz, double *work, int *info);

/* compute selected eigenvalues and, optionally, eigenvectors of a real symmetric matrix */
extern void
dsyevr_(char *jobz, char *range, char *uplo, int *n, double *a, int *lda, double *vl, double *vu, int *il, int *iu,
	double *abstol, int *m, double *w, double *z, int *ldz, int *isuppz, double *work, int *lwork, int *iwork,
	int *liwork, int *info);

/* compute all the eigenvalues and, optionally, eigenvectors of a complex Hermitian matrix A in packed storage */
extern void
zhpev_(char *jobz, char *uplo, int *n, complex *ap, double *w, complex *z, int *ldz, complex *work, double *rwork,
       int *info);

/* compute selected eigenvalues and, optionally, eigenvectors of a complex Hermitian matrix */
extern void
zheevr_(char *jobz, char *range, char *uplo, int *n, complex *a, int *lda, double *vl, double *vu, int *il, int *iu,
	double *abstol, int *m, double *w, complex *z, int *ldz, int *isuppz, complex *work, int *lwork, double *rwork,
	int *lrwork, int *iwork, int *liwork, int *info);

/* compute for an N-by-N real nonsymmetric matrix A, the eigenvalues and, optionally, the left and/or right eigenvectors */
extern void
dgeevx_(char *balanc, char *jobvl, char *jobvr, char *sense, int *n, double *a, int *lda, double *wr, double *wi,
	double *vl, int *ldvl, double *vr, int *ldvr, int *ilo, int *ihi, double *scale, double *abnrm, double *rconde,
	double *rcondv, double *work, int *lwork, int *iwork, int *info);

/* compute for an N-by-N complex nonsymmetric matrix A, the eigenvalues and, optionally, the left and/or right eigenvectors */
extern void
zgeevx_(char *balanc, char *jobvl, char *jobvr, char *sense, int *n, complex *a, int *lda, complex *w, complex *vl,
	int *ldvl, complex *vr, int *ldvr, int *ilo, int *ihi, double *scale, double *abnrm, double *rconde,
	double *rcondv, complex *work, int *lwork, double *rwork, int *info);

/* compute the Shur decomposition for an N-by-N real non-symmetric matrix A */ 
extern void
dgeesx_(char *jobvs, char *sort, void *select, char *sense, int *n, double *a, int *lda, int *sdim, double *wr, double *wi,
        double *vs, int *ldvs, double *rconde, double *rcondv, double *work, int * lwork, int *iwork, int *liwork, int *bwork,
        int *info);

/* compute the Shur decomposition for an N-by-N complex non-symmetric matrix A */ 
extern void
zgeesx_(char *jobvs, char *sort, void *select, char *sense, int *n, complex *a, int *lda, int *sdim, complex *w, complex *vs,
        int *ldvs, double *rconde, double *rcondv, complex *work, int *lwork, double *rwork, int *bwork, int *info);

/* compute the Cholesky factorization of a real symmetric positive definite matrix A stored in packed format */
extern void
dpptrf_(char *uplo, int *n, double *ap, int *info);

/* compute the Cholesky factorization of a complex Hermitian positive definite matrix A stored in packed format */
extern void
zpptrf_(char *uplo, int *n, complex *ap, int *info);

/* determines double precision machine parameters */
extern double
dlamch_(char *cmach);

/* choose problem-dependent parameters for the local environment */
extern int
ilaenv_(int *ispec,char *name,char *opts,int *n1, int *n2,int *n3,int *n4);

/* solve the linear equality-constrained least squares (LSE) problem */
extern void
dgglse_(int *m, int *n, int *p, double *a, int *lda, double *b, int *ldb, double *c, double *d, double *x, double *work,
	int *lwork, int *info);

/* solve the linear equality-constrained least squares (LSE) problem */
extern void
zgglse_(int *m, int *n, int *p, complex *a, int *lda, complex *b, int *ldb, complex *c, complex *d, complex *x, complex *work,
	int *lwork, int *info);

/* solve a general Gauss-Markov linear model (GLM) problem */
extern void
dggglm_(int *n, int *m, int *p, double *a, int *lda, double *b, int *ldb, double *d, double *x, double *y, double *work,
	int *lwork, int *info);

/* solve a general Gauss-Markov linear model (GLM) problem */
extern void
zggglm_(int *n, int *m, int *p, complex *a, int *lda, complex *b, int *ldb, complex *d, complex *x, complex *y, complex *work,
	int *lwork, int *info);




static list
dgesvx_caller (ab, fault)
     list ab;
     int *fault;

     /* takes a list representing a pair (<<a..>..>,<b..>) where the
	left is a row ordered square matrix and returns the solution
	of the corresponding system of linear equations as a list of
	numbers; returns an empty list if the matrix is singular */
{
  char fact;
  char trans;
  int n;
  int nrhs;
  double *a;
  int lda;
  double *af;
  int ldaf,*ipiv;
  char equed;
  double *r,*c,*b;
  int ldb;
  double *x;
  int ldx;
  double rcond,*ferr,*berr,*work;
  int *iwork,info;
  list result;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(ab ? (avm_length(ab->head) == avm_length(ab->tail)) : 0))
    return avm_copied(bad_lapack_spec);
  fact = 'E';
  trans = 'N';
  n = avm_length(ab->head);
  nrhs = 1;
  a = (double *) avm_matrix_of_list(1,0,0,1,ab->head,sizeof(double),&result,fault);
  lda = n;
  af = (double *) malloc(n * n * sizeof(double));
  ldaf = n;
  ipiv = (int *) malloc(n * sizeof(int));
  r = (double *) malloc(n * sizeof(double));
  c = (double *) malloc(n * sizeof(double));
  b = (double *) avm_vector_of_list(ab->tail,sizeof(double),&result,fault);
  ldb = n;
  x = (double *) malloc(n * sizeof(double));
  ldx = n;
  ferr = (double *) malloc(nrhs * sizeof(double));
  berr = (double *) malloc(nrhs * sizeof(double));
  work = (double *) malloc(4 * n * sizeof(double));
  iwork = (int *) malloc(n * sizeof(int));
  if (!*fault)
    *fault = !(a ? (af ? (ipiv ? (r ? (c ? (b ? (x ? (ferr ? (berr ? (work ? !!iwork : 0):0):0):0):0):0):0):0):0):0);
  if (*fault)
    result = (result ? result : avm_copied(memory_overflow));
  else
    dgesvx_(&fact,&trans,&n,&nrhs,a,&lda,af,&ldaf,ipiv,&equed,r,c,b,&ldb,x,&ldx,&rcond,ferr,berr,work,iwork,&info);
  if (a)
    free(a);
  if (af)
    free (af);
  if (ipiv)
    free (ipiv);
  if (r)
    free(r);
  if (c)
    free (c);
  if (b)
    free (b);
  if (ferr)
    free (ferr);
  if (berr)
    free (berr);
  if (work)
    free (work);
  if (iwork)
    free (iwork);
  if (*fault ? 1 : (info > n))
    result = (result ? result : avm_copied(lapack_error));
  else if (info < 0)
    avm_internal_error (84);
  else if (info == 0)
    result = avm_list_of_vector((void *) x,n,sizeof(double),fault);
  if (x)
    free (x);
  return result;
}







static list
zgesvx_caller (operand, fault)
     list operand;
     int *fault;

     /* same as above but for complex numbers */
{
  char fact;
  char trans;
  int n;
  int nrhs;
  complex *a;
  int lda;
  complex *af;
  int ldaf;
  int *ipiv;
  char equed;
  double *r;
  double *c;
  complex *b;
  int ldb;
  complex *x;
  int ldx;
  double rcond;
  double *ferr;
  double *berr;
  complex *work;
  double *rwork;
  int info;
  list result;

  if (*fault)
    return NULL;
  if (*fault = !(operand ? (avm_length(operand->head) == avm_length(operand->tail)) : 0))
    result = avm_copied (bad_lapack_spec);
  fact = 'E';
  trans = 'N';
  n = (int) avm_length(operand);
  nrhs = 1;
  a = (*fault ? NULL : (complex *) avm_matrix_of_list(1,0,0,1,operand->head,sizeof(complex),&result,fault));
  lda = n;
  ldaf = n;
  af = malloc(sizeof(complex) * ldaf * n);
  ipiv = (int *) malloc(sizeof(int) * n);
  r = (double *) malloc(sizeof(double) * n);
  c = (double *) malloc(sizeof(double) * n);
  ldb = n;
  b = (*fault ? NULL : (complex *) avm_vector_of_list(operand->tail,sizeof(double),&result,fault));
  ldx = n;
  x = (complex *) malloc(sizeof(complex) * ldx * nrhs);
  ferr = (double *) malloc(sizeof(double) * nrhs);
  berr = (double *) malloc(sizeof(double) * nrhs);
  work = (complex *) malloc(sizeof(complex) * 2 * n);
  rwork = (double *) malloc(sizeof(double) * 2 * n);
  if (!*fault)
    *fault = !(a ? (af ? (ipiv ? (r ? (c ? (b ? (x ? (ferr ? (berr ? (work ? !!rwork : 0):0):0):0):0):0):0):0):0):0);
  if (!*fault)
    zgesvx_(&fact,&trans,&n,&nrhs,a,&lda,af,&ldaf,ipiv,&equed,r,c,b,&ldb,x,&ldx,&rcond,ferr,berr,work,rwork,&info);
  if (a)
    free(a);
  if (af)
    free (af);
  if (ipiv)
    free (ipiv);
  if (r)
    free(r);
  if (c)
    free (c);
  if (b)
    free (b);
  if (ferr)
    free (ferr);
  if (berr)
    free (berr);
  if (work)
    free (work);
  if (rwork)
    free (rwork);
  if (*fault ? 1 : (info > n))
    result = (result ? result : avm_copied(lapack_error));
  else if (info < 0)
    avm_internal_error (89);
  else if (info == 0)
    result = avm_list_of_vector((void *) x,n,sizeof(complex),fault);
  if (x)
    free (x);
  return result;
}







static list
dgesdd_caller (operand, fault)
     list operand;
     int *fault;

     /* takes a list of m time series each of length n and returns a
	list of basis vectors each of length n for the singular value
	decomposition; the number of basis vectors is at most min(m,n)
	but could be less if the input time series aren't linearly
	independent; an empty list could be returned due to failure of
	convergence */
{
#define EPS 1.0e-8  /* for deciding the rank; consecutive singular values shouldn't fall by more than this ratio */

  char jobz;
  int m;
  int n;
  double *a;
  int lda;
  double *s;
  double *u;
  int ldu;
  double *vt;
  int ldvt;
  double *work;
  int lwork;
  int *iwork;
  int info;
  list result;
  int ucol,vtcol,prank,optimum_lwork;

  if (*fault)
    return NULL;
  if (*fault = !operand)
    return avm_copied(bad_lapack_spec);
  result = NULL;
  jobz = 'O';
  m = (int) avm_length(operand);
  n = (int) avm_length(operand->head);
  a = (double *) avm_matrix_of_list(0,0,0,1,operand,sizeof(double),&result,fault);
  lda = m;
  s = (double *) malloc(sizeof(double) * MIN(m,n));
  ucol = ldu = ((m < n) ? m : 1);
  u = (double *) malloc(ldu * ucol * sizeof(double)); 
  vtcol = ldvt = ((m < n) ? 1 : n);
  vt = (double *) malloc(ldvt * vtcol * sizeof(double));
  work = (double *) malloc(sizeof(double));
  lwork = (3 * MIN(m,n) * MIN(m,n)) + MAX(MAX(m,n),(5 * MIN(m,n) * MIN(m,n)) + (4 * MIN(m,n)));
  iwork = (int *) malloc(sizeof(int) * 8 * MIN(m,n));
  if (!(*fault = (*fault ? 1 : !(a ? (s ? (u ? (vt ? (work ? !!iwork : 0) : 0) : 0) : 0) : 0))))
    {
      optimum_lwork = -1;
      dgesdd_(&jobz,&m,&n,a,&lda,s,u,&ldu,vt,&ldvt,work,&optimum_lwork,iwork,&info);
      optimum_lwork = work[0];
      free(work);
      if (work = (double *) malloc(sizeof(double) * optimum_lwork))
	lwork = optimum_lwork;
      else
	*fault = !(work = (double *) malloc(sizeof(double) * lwork));
    }
  if (*fault)
    result = (result ? result : avm_copied(memory_overflow));
  else
    dgesdd_(&jobz,&m,&n,a,&lda,s,u,&ldu,vt,&ldvt,work,&lwork,iwork,&info);
  if (*fault ? 0 : (info < 0))
    avm_internal_error (85);
  if (u)
    free (u);
  if (work)
    free (work);
  if (iwork)
    free (iwork);
  prank = 1;
  if (s)
    {
      while (*fault ? 0 : ((prank < MIN(m,n) ? ((safemin < s[prank - 1]) ? ((s[prank] / s[prank - 1]) > EPS) : 0) : 0)))
	prank++;
      free (s);
    }
  if (*fault ? 0 : ((info == 0) ? (m < n) : 0))
    {
      free(vt);
      vt = NULL;
      a = (double *) avm_matrix_transposition((void *) a,n,m,sizeof(double));
      result = avm_list_of_matrix((void *) a,MIN(prank + 1,m),n,sizeof(double),fault);
    }
  else if (*fault ? 0 : (info == 0))
    {
      free (a);
      a = NULL;
      vt = (double *) avm_matrix_transposition((void *) vt,n,n,sizeof(double));
      result = avm_list_of_matrix((void *) vt,MIN(prank + 1,n),n,sizeof(double),fault);
    }
  if (a)
    free (a);
  if (vt)
    free (vt);
  return (*fault ? (result ? result : avm_copied(lapack_error)) : result);
}








static list
zgesdd_caller (operand, fault)
     list operand;
     int *fault;

     /* same as above for complex numbers */
{
  char jobz;
  int m;
  int n;
  complex *a;
  int lda;
  double *s;
  complex *u;
  int ldu;
  complex *vt;
  int ldvt;
  complex *work;
  int lwork;
  double *rwork;
  int *iwork;
  int info;
  list result;
  int ucol,vtcol,opt_lwork;
  int prank;

#define EPS 1.0e-8  /* for deciding the rank; consecutive singular values shouldn't fall by more than this ratio */

  if (*fault)
    return NULL;
  if (*fault = !operand)
    return avm_copied(bad_lapack_spec);
  result = NULL;
  jobz = 'A';
  m = (int) avm_length(operand);
  n = (int) avm_length(operand->head);
  a = (complex *) avm_matrix_of_list(0,0,0,1,operand,sizeof(complex),&result,fault);
  lda = m;
  s = (double *) malloc(sizeof(double) * MIN(m,n));
  ucol = ldu = ((m < n) ? m : 1);
  u = (complex *) malloc(ldu * ucol * sizeof(complex));
  vtcol = ldvt = ((m < n) ? 1 : n);
  vt = (complex *) malloc(ldvt * vtcol * sizeof(complex));
  work = (complex *) malloc(sizeof(complex));
  lwork = (MIN(m,n) * MIN(m,n)) + (2 * MIN(m,n)) + MAX(m,n);
  rwork = (double *) malloc(sizeof(double) * (5 * MIN(m,n) * MIN(m,n) + 5 * MIN(m,n)));
  iwork = (int *) malloc(sizeof(int) * 8 * MIN(m,n));
  if (!(*fault = !(a ? (s ? (u ? (vt ? (work ? (rwork ? !!iwork : 0) : 0) : 0) : 0) : 0) : 0)))
    {
      opt_lwork = -1;
      zgesdd_(&jobz,&m,&n,a,&lda,s,u,&ldu,vt,&ldvt,work,&opt_lwork,rwork,iwork,&info);
      opt_lwork = work[0][RE];
      free(work);
      if (work = (complex *) malloc(sizeof(complex) * opt_lwork))
	lwork = opt_lwork;
      else
	*fault = !(work = (complex *) malloc(sizeof(complex) * lwork));
    }
  if (*fault)
    result = (result ? result : avm_copied(memory_overflow));
  else
    zgesdd_(&jobz,&m,&n,a,&lda,s,u,&ldu,vt,&ldvt,work,&lwork,rwork,iwork,&info);
  if (*fault ? 0 : (info < 0))
    avm_internal_error (90);
  if (u)
    free (u);
  if (work)
    free (work);
  if (iwork)
    free (iwork);
  if (rwork)
    free (rwork);
  prank = 1;
  if (s)
    {
      while (*fault ? 0 : ((prank < MIN(m,n) ? ((safemin < s[prank - 1]) ? ((s[prank] / s[prank - 1]) > EPS) : 0) : 0)))
	prank++;
      free (s);
    }
  if (*fault ? 0 : ((info == 0) ? (m < n) : 0))
    {
      free(vt);
      vt = NULL;
      a = (complex *) avm_matrix_transposition((void *) a,n,m,sizeof(complex));
      result = avm_list_of_matrix((void *) a,MIN(prank + 1,m),n,sizeof(complex),fault);
    }
  else if (*fault ? 0 : (info == 0))
    {
      free (a);
      a = NULL;
      vt = (complex *) avm_matrix_transposition((void *) vt,n,n,sizeof(complex));
      result = avm_list_of_matrix((void *) vt,MIN(prank + 1,n),n,sizeof(complex),fault);
    }
  if (a)
    free (a);
  if (vt)
    free (vt);
  return (*fault ? (result ? result : avm_copied(lapack_error)) : result);
}









static list
dgelsd_caller (operand, fault)
     list operand;
     int *fault;

     /* operand represents a pair (a,b) of a matrix and a vector; a
	vector of coefficients for the least squares fit is returned,
	but it could be empty due to failure of convergence */
{
  int m;
  int n;
  int nrhs;
  double *a;
  int lda;
  double *b;
  int ldb;
  double *s;
  double rcond;
  int rank;
  double *work;
  int lwork;
  int *iwork;
  int info;
  list result;
  int opt_lwork;
  double *req_b;
  int ispec;
  char *name = "DGELSD";
  char *opts = "";
  int smlsiz;
  int nlvl;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(operand ? (operand->head ? (avm_length(operand->head) == avm_length(operand->tail)) : 0) : 0))
    return avm_copied (bad_lapack_spec);
  m = (int) avm_length(operand->head);
  n = (int) avm_length(operand->head->head);
  nrhs = 1;
  a = (double *) avm_matrix_of_list(0,0,0,1,operand->head,sizeof(double),&result,fault);
  lda = m;
  b = (*fault ? NULL : (double *) avm_vector_of_list(operand->tail,sizeof(double),&result,fault));
  if (*fault ? 0 : (n > m))
    if (!(*fault = !(req_b = (double *) realloc((void *) b,sizeof(double) * n))))
      b = req_b;
  ldb = MAX(n,m);
  s = (double *) malloc(sizeof(double) * MIN(m,n));
  rcond = -1.0;
  ispec = 9;
  smlsiz = ilaenv_(&ispec,name,opts,&n,&m,&nrhs,&lda);
  ispec = MIN(m,n) / ((smlsiz = MAX(smlsiz,25)) + 1);
  nlvl = 0;
  while (ispec)
    {
      ispec = ispec >> 1;
      nlvl++;
    }
  work = (double *) malloc(sizeof(double));
  lwork = (12 * MIN(m,n)) + (2 * MIN(m,n) * smlsiz) + (8 * MIN(m,n) * nlvl) + (MIN(m,n) * nrhs) + ((smlsiz+1) * (smlsiz+1));
  iwork = (int *) malloc(sizeof(int) * (3 * MIN(m,n) * nlvl + 11 * MIN(m,n)));
  if (!(*fault = (*fault ? 1 : !(a ? (b ? (s ? (work ? !!iwork : 0) : 0) : 0) : 0))))
    {
      opt_lwork = -1;
      dgelsd_(&m,&n,&nrhs,a,&lda,b,&ldb,s,&rcond,&rank,work,&opt_lwork,iwork,&info);
      opt_lwork = work[0];
      free(work);
      if (work = (double *) malloc(sizeof(double) * opt_lwork))
	lwork = opt_lwork;
      else
	*fault = !(work = (double *) malloc(sizeof(double) * lwork));
    }
  if (!*fault)
    {
      dgelsd_(&m,&n,&nrhs,a,&lda,b,&ldb,s,&rcond,&rank,work,&lwork,iwork,&info);
      if ((info < 0) ? (info >= -14) : 0)
	avm_internal_error (80);
      if (*fault = (info < 0))
	result = (result ? result : avm_copied (lapack_error));
    }
  if (a)
    free (a);
  if (s)
    free (s);
  if (work)
    free (work);
  if (iwork)
    free (iwork);
  if (*fault ? 0 : (info == 0))
    result = avm_list_of_vector((void *) b,n,sizeof(double),fault);
  if (b)
    free (b);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}








static list
zgelsd_caller (operand, fault)
     list operand;
     int *fault;

     /* same as above for complex numbers */
{
  int m;
  int n;
  int nrhs;
  complex *a;
  int lda;
  complex *b;
  int ldb;
  double *s;
  double rcond;
  int rank;
  complex *work;
  int lwork;
  double *rwork;
  int *iwork;
  int info;
  list result;
  int opt_lwork;
  complex *req_b;
  int ispec;
  char *name = "ZGELSD";
  char *opts = "";
  int smlsiz;
  int nlvl;

  if (*fault)
    return NULL;
  if (*fault = !(operand ? (operand->head ? (avm_length(operand->head) == avm_length(operand->tail)) : 0) : 0))
    return avm_copied (bad_lapack_spec);
  result = NULL;
  m = (int) avm_length(operand->head);
  n = (int) avm_length(operand->head->head);
  nrhs = 1;
  a = (complex *) avm_matrix_of_list(1,0,0,1,operand->head,sizeof(complex),&result,fault);
  lda = n;
  b = (*fault ? NULL : (complex *) avm_vector_of_list(operand->tail,sizeof(complex),&result,fault));
  if (*fault ? 0 : (n > m))
    if (!(*fault = !(req_b = (complex *) realloc((void *) b,sizeof(complex) * n))))
      b = req_b;
  ldb = 1;
  s = (double *) malloc(sizeof(double) * MIN(m,n));
  rcond = -1.0;
  work = (complex *) malloc(sizeof(complex));
  lwork = (2 * MIN(m,n)) + (MIN(m,n) * nrhs);
  ispec = 9;
  smlsiz = ilaenv_(&ispec,name,opts,&n,&m,&nrhs,&lda);
  ispec = MIN(m,n) / ((smlsiz = MAX(smlsiz,25)) + 1);
  nlvl = 0;
  while (ispec)
    {
      ispec = ispec >> 1;
      nlvl++;
    }
  if (n < m)
    rwork = (double *) malloc(sizeof(double) * ((10*n)+(2*n*smlsiz)+(8*n*nlvl)+(3*smlsiz*nrhs)+((smlsiz+1)*(smlsiz+1))));
  else
    rwork = (double *) malloc(sizeof(double) * ((10*m)+(2*m*smlsiz)+(8*m*nlvl)+(3*smlsiz*nrhs)+((smlsiz+1)*(smlsiz+1))));
  iwork = (int *) malloc(sizeof(int) * ((3 * MIN(m,n) * nlvl) + (11 * MIN(m,n))));
  if (!(*fault = (*fault ? 1 : !(a ? (b ? (s ? (work ? (rwork ? !!iwork : 0) : 0) : 0) : 0) : 0))))
    {
      opt_lwork = -1;
      zgelsd_(&m,&n,&nrhs,a,&lda,b,&ldb,s,&rcond,&rank,work,&opt_lwork,rwork,iwork,&info);
      opt_lwork = work[0][RE];
      free(work);
      if (work = (complex *) malloc(sizeof(complex) * opt_lwork))
	lwork = opt_lwork;
      else
	*fault = !(work = (complex *) malloc(sizeof(complex) * lwork));
    }
  if (!*fault)
    {
      zgelsd_(&m,&n,&nrhs,a,&lda,b,&ldb,s,&rcond,&rank,work,&lwork,rwork,iwork,&info);
      if ((info < 0) ? (info > -15) : 0)
	avm_internal_error (91);
      else if (info < 0)
	result = (result ? result : avm_copied(lapack_error));
    }
  if (a)
    free (a);
  if (s)
    free (s);
  if (work)
    free (work);
  if (rwork)
    free (rwork);
  if (iwork)
    free (iwork);
  if (*fault ? 0 : (info == 0))
    result = (result ? result : avm_list_of_vector((void *) b,n,sizeof(complex),fault));
  if (b)
    free (b);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}









static list
dspev_caller (operand, fault)
     list operand;
     int *fault;

     /* computes eigenvectors and eigenvalues of a symmetric real
	matrix the same as dsyevr_caller but with a slower algorithm
	and using less memory and packed arrays */
{
  char jobz;
  char uplo;
  int n;
  double *ap;
  double *w;
  double *z;
  int ldz;
  double *work;
  int info;
  list result;

  if (*fault)
    return NULL;
  result = NULL;
  jobz = 'V';
  uplo = ((operand ? (operand->head ? operand->head->tail : NULL) : NULL) ? 'U' : 'L');
  n = (int) avm_length(operand);
  ap = (double *) avm_packed_matrix_of_list(uplo == 'U',operand,n,sizeof(double),&result,fault);
  w = (double *) malloc(sizeof(double) * n);
  ldz = n;
  z = (double *) malloc(sizeof(double) * ldz * n);
  work = (double *) malloc(sizeof(double) * 3 * n);
  if (!(*fault = (*fault ? 1 : !(ap ? (w ? (z ? !!work : 0) : 0) : 0))))
    dspev_(&jobz,&uplo,&n,ap,w,z,&ldz,work,&info);
  if (*fault ? 0 : ((info < 0) ? (info >= -9) : 0))
    avm_internal_error (88);
  if (ap)
    free (ap);
  if (work)
    free (work);
  result = (*fault ? result : avm_list_of_matrix((void *) z,ldz,n,sizeof(double),fault));
  if (z)
    free (z);
  result = (*fault ? result : avm_recoverable_join(result,avm_list_of_vector((void *) w,n,sizeof(double),fault)));
  if (w)
    free (w);
  *fault = (*fault ? 1 : !result);
  return (result ? result : avm_copied (memory_overflow));
}










static list
dsyevr_caller (operand, fault)
     list operand;
     int *fault;

     /* takes a list representing a symmetric real matrix and returns
        a list representing a pair (<<e..>..>,<l..>) with one item on
        the left for each eigenvector and one item on the right for
        each eigenvalue; since the input matrix is symmetric, the
        upper or lower triangular portion may be omitted, which would
        be indicated by the rows having increasing or decreasing
        lengths, respectively; if the whole matrix is given, the
        lower triangular part is ignored */
{
  char jobz;
  char range;
  char uplo;
  int n;
  double *a;
  int lda;
  double vl;
  double vu;
  int il;
  int iu;
  double abstol;
  int m;
  double *w;
  double *z;
  int ldz;
  int *isuppz;
  double *work;
  int lwork;
  int *iwork;
  int liwork;
  int info;
  list result;
  int opt_lwork,opt_liwork;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(operand ? operand->head : NULL))
    return avm_copied(bad_lapack_spec);
  jobz = 'V';
  range = 'A';
  uplo = (operand->head->tail ? 'U' : 'L');
  lda = n = (int) avm_length(operand);
  if (operand->tail ? (avm_length(operand->head) == avm_length(operand->tail->head)) : 1)
    a = (double *) avm_matrix_of_list(1,0,0,1,operand,sizeof(double),&result,fault);
  else
    a = (double *) avm_matrix_of_list(1,uplo == 'U',uplo == 'L',1,operand,sizeof(double),&result,fault);
  abstol = -1.0;
  m = n;
  w = (double *) malloc(sizeof(double) * n);
  ldz = n;
  z = (double *) malloc(sizeof(double) * ldz * m);
  isuppz = (int *) malloc(sizeof(int) * 2 * m);
  work = (double *) malloc(sizeof(double));
  lwork = 26 * n;                                     /* bigger is better */
  iwork = (int *) malloc(sizeof(int));
  liwork = 10 * n;                                    /* bigger is better */
  if (!(*fault = (*fault ? 1 : !(a ? (w ? (z ? (isuppz ? (work ? !!iwork : 0) : 0) : 0) : 0) : 0))))
    {
      opt_liwork = opt_lwork = -1;
      dsyevr_(&jobz,&range,&uplo,&n,a,&lda,&vl,&vu,&il,&iu,&abstol,&m,w,z,&ldz,isuppz,work,&opt_lwork,iwork,&opt_liwork,
	      &info);
      opt_lwork = work[0];
      opt_liwork = iwork[0];
      free(work);
      free(iwork);
      if (work = (double *) malloc(sizeof(double) * opt_lwork))
	lwork = opt_lwork;
      else
	*fault = !(work = (double *) malloc(sizeof(double) * lwork));
      if (iwork = (int *) malloc(sizeof(int) * opt_liwork))
	liwork = opt_liwork;
      else
	*fault = (*fault ? 1 : !(iwork = (int *) malloc(sizeof(int) * liwork)));
    }
  if (!*fault)
    {
      dsyevr_(&jobz,&range,&uplo,&n,a,&lda,&vl,&vu,&il,&iu,&abstol,&m,w,z,&ldz,isuppz,work,&lwork,iwork,&liwork,&info);
      if ((info < 0) ? (info >= -21) : 0)
	avm_internal_error (86);
      if (*fault = (info != 0))
	result = (result ? result : avm_copied (lapack_error));
    }
  if (a)
    free (a);
  if (isuppz)
    free (isuppz);
  if (work)
    free (work);
  if (iwork)
    free (iwork);
  result = (*fault ? result : avm_list_of_matrix((void *) z,ldz,m,sizeof(double),fault));
  if (z)
    free (z);
  result = (*fault ? result : avm_recoverable_join(result,avm_list_of_vector((void *) w,n,sizeof(double),fault)));
  if (w)
    free (w);
  *fault = (*fault ? 1 : !result);
  if (*fault ? !result : 0)
    {
      *fault = 0;
      result = dspev_caller(operand, fault);
    }
  return (result ? result : avm_copied (memory_overflow));
}









static list
zhpev_caller (operand, fault)
     list operand;
     int *fault;

     /* complex eigenvectors and real eigenvalues of a complex
	Hermitian matrix, as below but with less space and a slower
	or less accurate algorithm */
{
  char jobz;
  char uplo;
  int n;
  complex *ap;
  double *w;
  complex *z;
  int ldz;
  complex *work;
  double *rwork;
  int info;
  list result;

  if (*fault)
    return NULL;
  if (*fault = !(operand ? operand->head : NULL))
    return avm_copied(bad_lapack_spec);
  result = NULL;
  jobz = 'V';
  uplo = (operand->head->tail ? 'U' : 'L');
  n = (int) avm_length (operand);
  ap = (complex *) avm_packed_matrix_of_list(uplo == 'U',operand,n,sizeof(complex),&result,fault);
  w = (double *) malloc(sizeof(double) * n);
  ldz = ((jobz == 'V') ? n : 1);
  z = (complex *) malloc(sizeof(complex) * ldz * n);
  work = (complex *) malloc((sizeof(complex) * 2 * n) - 1);
  rwork = (double *) malloc((sizeof(double) * 3 * n) - 2);
  if (!(*fault = (*fault ? 1 : (ap ? (w ? (z ? (work ? !!rwork : 0) : 0) : 0) : 0))))
    zhpev_(&jobz,&uplo,&n,ap,w,z,&ldz,work,rwork,&info);
  if (*fault ? 0 : ((info < 0) ? (info >= -10) : 0))
    avm_internal_error (94);
  if (ap)
    free (ap);
  if (work)
    free (work);
  if (rwork)
    free (rwork);
  result = (*fault ? result : avm_list_of_matrix((void *) z,n,ldz,sizeof(complex),fault));
  if (z)
    free (z);
  result = (*fault ? result : avm_recoverable_join(result,avm_list_of_vector((void *) w,n,sizeof(double),fault)));
  if (w)
    free (w);
  *fault = (*fault ? 1 : !result);
  return (result ? result : avm_copied (memory_overflow));
}










static list
zheevr_caller (operand, fault)
     list operand;
     int *fault;

     /* complex eigenvectors and real eigenvalues of a Hermitian
	matrix, optionally in upper or lower triangular form */
{
  char jobz;
  char range;
  char uplo;
  int n;
  complex *a;
  int lda;
  double vl;
  double vu;
  int il;
  int iu;
  double abstol;
  int m;
  double *w;
  complex *z;
  int ldz;
  int *isuppz;
  complex *work;
  int lwork;
  double *rwork;
  int lrwork;
  int *iwork;
  int liwork;
  int info;
  list result;
  int opt_lwork,opt_lrwork,opt_liwork;

  if (*fault)
    return NULL;
  if (*fault = !(operand ? operand->head : NULL))
    return avm_copied(bad_lapack_spec);
  result = NULL;
  jobz = 'V';
  range = 'A';
  uplo = (operand->head->tail ? 'U' : 'L');
  n = (int) avm_length (operand);
  lda = n;
  if (operand->tail ? (avm_length(operand->head) == avm_length(operand->tail->head)) : 1)
    a = (complex *) avm_matrix_of_list(1,0,0,1,operand,sizeof(complex),&result,fault);
  else
    a = (complex *) avm_matrix_of_list(1,uplo == 'U',uplo == 'L',1,operand,sizeof(complex),&result,fault);
  vl = vu = 0.0;
  il = iu = 0;
  abstol = -1.0;
  m = n;
  w = (double *) malloc(sizeof(complex) * m);
  ldz = ((jobz == 'V') ? n : 1);
  z = (complex *) malloc(sizeof(complex) * m * ldz);
  isuppz = (int *) malloc(sizeof(int) * 2 * m);
  work = (complex *) malloc(sizeof(complex));
  lwork = 2 * n;
  rwork = (double *) malloc(sizeof(double));
  lrwork = 24 * n;
  iwork = (int *) malloc(sizeof(int));
  liwork = 10 * n;
  if (!(*fault = !(a ? (w ? (z ? (isuppz ? (work ? (rwork ? !!iwork : 0) : 0) : 0) : 0) : 0) : 0)))
    {
      opt_lwork = opt_lrwork = opt_liwork = -1;
      zheevr_(&jobz,&range,&uplo,&n,a,&lda,&vl,&vu,&il,&iu,&abstol,&m,w,z,&ldz,isuppz,work,
	      &opt_lwork,rwork,&opt_lrwork,iwork,&opt_liwork,&info);
      opt_lwork = work[0][RE];
      opt_lrwork = rwork[0];
      opt_liwork = iwork[0];
      free (work);
      free(rwork);
      free(iwork);
      if (work = (complex *) malloc(sizeof(complex) * opt_lwork))
	lwork = opt_lwork;
      else if (!(work = (complex *) malloc(sizeof(complex) * lwork)))
	*fault = 1;
      if (rwork = (double *) malloc(sizeof(double) * opt_lrwork))
	lrwork = opt_lrwork;
      else if (!(rwork = (double *) malloc(sizeof(double) * lrwork)))
	*fault = 1;
      if (iwork = (int *) malloc(sizeof(int) * opt_liwork))
	liwork = opt_liwork;
      else if (!(iwork = (int *) malloc(sizeof(int) * liwork)))
	*fault = 1;
    }
  if (!*fault)
    {
      zheevr_(&jobz,&range,&uplo,&n,a,&lda,&vl,&vu,&il,&iu,&abstol,&m,w,z,&ldz,isuppz,work,
	      &lwork,rwork,&lrwork,iwork,&liwork,&info);
      if ((info < 0) ? (info >= -23) : 0)
	avm_internal_error (107);
      if (*fault = (info > 0))
	result = (result ? result : avm_copied(lapack_error));
    }
  if (a)
    free (a);
  if (isuppz)
    free (isuppz);
  if (work)
    free (work);
  if (rwork)
    free (rwork);
  if (iwork)
    free (iwork);
  result = (*fault ? result : avm_list_of_matrix((void *) z,ldz,m,sizeof(complex),fault));
  if (z)
    free (z);
  result = (*fault ? result : avm_recoverable_join(result,avm_list_of_vector((void *) w,n,sizeof(double),fault)));
  if (w)
    free (w);
  *fault = (*fault ? 1 : !result);
  if (*fault ? !result : 0)
    {
      *fault = 0;
      result = zhpev_caller(operand, fault);
    }
  return (result ? result : avm_copied(memory_overflow));
}











static list
dgeevx_decoder(wr,wi,vr,n,result,fault)
     double *wr;
     double *wi;
     double *vr;
     int n;
     list result;
     int *fault;

     /* gets the eigenvectors and eigenvalues in complex form out of
	the dgeevx parameters wr,wi,vr, and n, and also disposes of
	the parameters; *fault could be true on entry in which case
	the memory will still be freed if necessary but the given
	result or a memory overflow message will be returned */
{
  complex *wz,*vz;
  list vectors,values;
  int i,j;

  values = NULL;
  wz = (complex *) malloc(sizeof(complex) * n);
  if (!(*fault = (*fault ? 1 : !wz)))
    {
      for (i = 0; i < n; i++)
	{
	  wz[i][RE] = wr[i];
	  wz[i][IM] = wi[i];
	}
      values = avm_list_of_vector((void *) wz,n,sizeof(complex),fault);
      if (*fault)
	{
	  result = values;
	  values = NULL;
	}
    }
  if (wz)
    free (wz);
  if (wr)
    free (wr);
  vectors = NULL;
  vz = (complex *) malloc(sizeof(complex) * n * n);
  if (!(*fault = (*fault ? 1 : !vz)))
    {
      i = 0;
      while (i < n)
	if (wi[i] == 0.0)
	  {
	    for (j = 0; j < n; j++)
	      {
		vz[i * n + j][RE] = vr[j * n + i];
		vz[i * n + j][IM] = 0.0;
	      }
	    i++;
	  }
	else
	  {
	    for (j = 0; j < n; j++)
	      {
		vz[i * n + j][RE] = vz[(i + 1) * n + j][RE] = vr[j * n + i];
		vz[i * n + j][IM] = vr[j * n + i + 1];
		vz[(i + 1) * n + j][IM] = -vr[j * n + i + 1];
	      }
	    i = i + 2;
	  }
      vectors = avm_list_of_matrix((void *) vz,n,n,sizeof(complex),fault);
      if (*fault)
	{
	  result = vectors;
	  vectors = NULL;
	}
      else
	{
	  *fault = !(result = avm_recoverable_join(vectors,values));
	  values = vectors = NULL;
	}
    }
  if (wi)
    free (wi);
  if (values)
    avm_dispose (values);
  if (vectors)
    avm_dispose (vectors);
  if (vz)
    free (vz);
  if (vr)
    free (vr);
  return (*fault ? (result ? result : avm_copied (memory_overflow)) : result);
}






static list
dgeevx_caller (operand, fault)
     list operand;
     int *fault;

     /* takes a list representing a non-symmetric real square matrix
	and returns the complex right eigenvectors and eigenvalues
	(<<e..>..>,<v..>) */
{
  char balanc;
  char jobvl;
  char jobvr;
  char sense;
  int n;
  double *a;
  int lda;
  double *wr;
  double *wi;
  double *vl;
  int ldvl;
  double *vr;
  int ldvr;
  int ilo;
  int ihi;
  double *scale;
  double abnrm;
  double *rconde;
  double *rcondv;
  double *work;
  int lwork;
  int *iwork;
  int info;
  list result;
  int opt_lwork;

  result = NULL;
  if (*fault)
    return NULL;
  balanc = 'B';
  jobvl = 'N';
  jobvr = 'V';
  sense = 'N';
  n = (int) avm_length(operand);
  a = (double *) avm_matrix_of_list(1,0,0,1,operand,sizeof(double),&result,fault);
  lda = n;
  wr = (double *) malloc(sizeof(double) * n);
  wi = (double *) malloc(sizeof(double) * n);
  ldvl = ((jobvl == 'V') ? n : 1);
  vl = (double *) malloc(sizeof(double) * ldvl * ((jobvl == 'V') ? n : 1));
  ldvr = ((jobvr == 'V') ? n : 1);
  vr = (double *) malloc(sizeof(double) * ldvr * ((jobvr == 'V') ? n : 1));
  scale = (double *) malloc(sizeof(double) * n);
  rconde = (double *) malloc(sizeof(double) * n);
  rcondv = (double *) malloc(sizeof(double) * n);
  work = (double *) malloc(sizeof(double));
  lwork = (((sense = 'V') ? 1 : (sense = 'B')) ? (n * (n + 6)) : (((jobvl = 'V') ? 1 : (jobvr = 'V')) ? (3 * n) : (2 * n)));
  iwork = (int *) malloc(sizeof(int) * (((sense == 'N') ? 1 : (sense == 'E')) ? 1 : ((2 * n) - 2)));
  if (!*fault)
    *fault = !(a ? (wr ? (wi ? (vl ? (vr ? (scale ? (rconde ? (rcondv ? (work ? !!iwork : 0): 0): 0): 0): 0): 0): 0): 0): 0);
  if (!*fault)
    {
      opt_lwork = -1;
      dgeevx_(&balanc,&jobvl,&jobvr,&sense,&n,a,&lda,wr,wi,vl,&ldvl,vr,&ldvr,&ilo,&ihi,scale,&abnrm,rconde,rcondv,work,
	      &opt_lwork,iwork,&info);
      opt_lwork = work[0];
      free(work);
      if (work = (double *) malloc(sizeof(double) * opt_lwork))
	lwork = opt_lwork;
      else
	*fault = !(work = (double *) malloc(sizeof(double) * lwork));
    }
  if (!*fault)
    {
      dgeevx_(&balanc,&jobvl,&jobvr,&sense,&n,a,&lda,wr,wi,vl,&ldvl,vr,&ldvr,&ilo,&ihi,scale,&abnrm,rconde,rcondv,work,
	      &lwork,iwork,&info);
      if ((info < 0) ? (info >= -23) : 0)
	avm_internal_error (81);
      if (*fault = (info != 0))
	result = (result ? result : avm_copied (lapack_error));
    }
  if (a)
    free (a);
  if (vl)
    free (vl);
  if (scale)
    free (scale);
  if (rconde)
    free (rconde);
  if (rcondv)
    free (rcondv);
  if (work)
    free (work);
  if (iwork)
    free(iwork);
  return dgeevx_decoder(wr,wi,vr,n,result,fault);
}








static list
zgeevx_caller (operand, fault)
     list operand;
     int *fault;

     /* complex eigenvectors and eigenvalues of a non-symmetric
	complex square matrix; could return nil due to lack of
	convergence, which isn't an exception */
{
  char balanc;
  char jobvl;
  char jobvr;
  char sense;
  int n;
  complex *a;
  int lda;
  complex *w;
  complex *vl;
  int ldvl;
  complex *vr;
  int ldvr;
  int ilo;
  int ihi;
  double *scale;
  double abnrm;
  double *rconde;
  double *rcondv;
  complex *work;
  int lwork;
  double *rwork;
  int info;
  list result;
  int opt_lwork;

  if (*fault)
    return NULL;
  result = NULL;
  balanc = 'B';
  jobvl = 'N';
  jobvr = 'V';
  sense = 'N';
  n = (int) avm_length(operand);
  a = (complex *) avm_matrix_of_list(1,0,0,1,operand,sizeof(complex),&result,fault);
  lda = n;
  w = (complex *) malloc(sizeof(complex) * n);
  ldvl = ((jobvl == 'V') ? n : 1);
  vl = (complex *) malloc(sizeof(complex) * ldvl * n);
  ldvr = ((jobvr == 'V') ? n : 1);
  vr = (complex *) malloc(sizeof(complex) * ldvr * n);
  scale = (double *) malloc(sizeof(double) * n);
  rconde = (double *) malloc(sizeof(double) * n);
  rcondv = (double *) malloc(sizeof(double) * n);
  work = (complex *) malloc(sizeof(complex));
  lwork = (((sense == 'N') ? 1 : (sense == 'E')) ? (2 * n) : ((n * n) + (2 * n)));
  rwork = (double *) malloc(sizeof(double) * 2 * n);
  if (!*fault)
    *fault = !(a ? (w ? (vl ? (vr ? (scale ? (rconde ? (rcondv ? (work ? !!rwork : 0) : 0) : 0) : 0) : 0) : 0) : 0) : 0);
  if (!*fault)
    {
      opt_lwork = -1;
      zgeevx_(&balanc,&jobvl,&jobvr,&sense,&n,a,&lda,w,vl,&ldvl,vr,&ldvr,&ilo,&ihi,scale,&abnrm,rconde,rcondv,work,
	      &opt_lwork,rwork,&info);
      opt_lwork = work[0][RE];
      free(work);
      if (work = (complex *) malloc(sizeof(complex) * opt_lwork))
	lwork = opt_lwork;
      else
	*fault = !(work = (complex *) malloc(sizeof(complex) * lwork));
    }
  if (!*fault)
    {
      zgeevx_(&balanc,&jobvl,&jobvr,&sense,&n,a,&lda,w,vl,&ldvl,vr,&ldvr,&ilo,&ihi,scale,&abnrm,rconde,rcondv,work,
	      &lwork,rwork,&info);
      if ((info < 0) ? (info >= -22) : 0)
	avm_internal_error (93);
      else if (*fault = (info < 0))
	result = (result ? result : avm_copied(lapack_error));
    }
  if (a)
    free (a);
  if (vl)
    free (vl);
  if (scale)
    free (scale);
  if (rconde)
    free (rconde);
  if (rcondv)
    free (rcondv);
  if (work)
    free (work);
  if (rwork)
    free (rwork);
  result = ((*fault ? 1 : (info != 0)) ? result : avm_list_of_matrix((void *) vr,n,ldvr,sizeof(complex),fault));
  if (vr)
    free (vr);
  if (*fault ? 0 : (info == 0))
    result = avm_recoverable_join(result,avm_list_of_vector((void *) w,n,sizeof(complex),fault));
  if (w)
    free (w);
  *fault = (*fault ? 1 : ((info == 0) ? !result : 0));
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}











static list
dpptrf_caller (operand, fault)
     list operand;
     int *fault;

     /* returns either the upper or lower factor of the Cholesky
	decomposition, depending on whether the upper or lower half of
	the matrix is given; if the whole matrix is given the upper
	factor is returned; if the matrix isn't positive definite, nil
	is returned but it's not an exception */
{
  char uplo;
  int n;
  double *ap;
  int info;
  list result;

  if (*fault)
    return NULL;
  result = NULL;
  uplo = (operand->head->tail ? 'U' : 'L');
  n = (int) avm_length(operand);
  ap = (double *) avm_packed_matrix_of_list(uplo == 'U',operand,n,sizeof(double),&result,fault);
  if (!*fault)
    dpptrf_(&uplo,&n,ap,&info);
  if (*fault ? 0 : info < 0)
    avm_internal_error (87);
  if (*fault ? 0 : (info == 0))
    result = avm_list_of_packed_matrix(uplo == 'U',(void *) ap,n,sizeof(double),fault);
  if (ap)
    free (ap);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}










static list
zpptrf_caller (operand, fault)
     list operand;
     int *fault;

     /* same as above but for complex numbers */
{
  char uplo;
  int n;
  complex *ap;
  int info;
  list result;

  if (*fault)
    return NULL;
  result = NULL;
  uplo = (operand->head->tail ? 'U' : 'L');
  n = (int) avm_length(operand);
  ap = (complex *) avm_packed_matrix_of_list(uplo == 'U',operand,n,sizeof(complex),&result,fault);
  if (!*fault)
    zpptrf_(&uplo,&n,ap,&info);
  if (*fault ? 0 : info < 0)
    avm_internal_error (92);
  if (*fault ? 0 : (info == 0))
    result = avm_list_of_packed_matrix(uplo == 'U',(void *) ap,n,sizeof(complex),fault);
  if (ap)
    free (ap);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}








static list
dgglse_caller (operand, fault)
     list operand;
     int *fault;

     /* The operand represents ((A,c),(B,d)) where A and B are
        matrices and c and d are vectors.  A and c are of length m and
        B and d are of length p. Both A and B are of width n. The
        result is a vector x of length n to minimize |Ax-c| subject to
        the constraint that Bx=d. */

{
  int m;
  int n;
  int p;
  double *a;
  int lda;
  double *b;
  int ldb;
  double *c;
  double *d;
  double *x;
  double *work;
  int lwork;
  int info;
  list result,ac,bd;
  int opt_lwork;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(operand ? ((ac = operand->head) ? ((bd = operand->tail) ? (ac->head ? !!(bd->head) : 0) : 0) : 0) : 0))
    return avm_copied(bad_lapack_spec);
  m = (int) avm_length(ac->head);
  n = (int) avm_length(ac->head->head);
  p = (int) avm_length(bd->head);
  if (*fault = !((p <= n) ? ((n <= m + p) ? (n == (int) avm_length(bd->head->head)) : 0): 0))
    return avm_copied(bad_lapack_spec);
  if (*fault = ((m != (int) avm_length(ac->tail)) ? 1 : (p != (int) avm_length(bd->tail))))
    return avm_copied(bad_lapack_spec);
  a = (double *) avm_matrix_of_list(0,0,0,1,ac->head,sizeof(double),&result,fault);
  lda = m;
  b = (*fault ? NULL : (double *) avm_matrix_of_list(0,0,0,1,bd->head,sizeof(double),&result,fault));
  ldb = p;
  c = (*fault ? NULL : (double *) avm_vector_of_list(ac->tail,sizeof(double),&result,fault));
  d = (*fault ? NULL : (double *) avm_vector_of_list(bd->tail,sizeof(double),&result,fault));
  x = (double *) malloc(sizeof(double) * p);
  work = (double *) malloc(sizeof(double));
  lwork = m + n + p;
  if (!(*fault = (*fault ? 1 : !(a ? (b ? (c ? (d ? (x ? !!work : 0) : 0) : 0) : 0) : 0))))
    {
      opt_lwork = -1;
      dgglse_(&m,&n,&p,a,&lda,b,&ldb,c,d,x,work,&opt_lwork,&info);
      opt_lwork = work[0];
      free (work);
      if (work = (double *) malloc(sizeof(double) * opt_lwork))
	lwork = opt_lwork;
      else
	*fault = !(work = (double *) malloc(sizeof(double) * lwork));
    }
  if (!*fault)
    {
      dgglse_(&m,&n,&p,a,&lda,b,&ldb,c,d,x,work,&lwork,&info);
      if ((info < 0) ? (info >= -13) : 0)
	avm_internal_error (98);
      if (*fault = (info != 0))
	result = (result ? result : avm_copied(lapack_error));
    }
  if (a)
    free (a);
  if (b)
    free (b);
  if (c)
    free (c);
  if (d)
    free (d);
  if (work)
    free (work);
  result = (*fault ? result : avm_list_of_vector((void *) x,n,sizeof(double),fault));
  if (x)
    free (x);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}









static list
zgglse_caller (operand, fault)
     list operand;
     int *fault;

     /* same as above for complex numbers */
{
  int m;
  int n;
  int p;
  complex *a;
  int lda;
  complex *b;
  int ldb;
  complex *c;
  complex *d;
  complex *x;
  complex *work;
  int lwork;
  int info;
  list result,ac,bd;
  int opt_lwork;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(operand ? ((ac = operand->head) ? ((bd = operand->tail) ? (ac->head ? !!(bd->head) : 0) : 0) : 0) : 0))
    return avm_copied(bad_lapack_spec);
  m = (int) avm_length(ac->head);
  n = (int) avm_length(ac->head->head);
  p = (int) avm_length(bd->head);
  if (*fault = !((p <= n) ? ((n <= m + p) ? (n == (int) avm_length(bd->head->head)) : 0): 0))
    return avm_copied(bad_lapack_spec);
  if (*fault = ((m != (int) avm_length(ac->tail)) ? 1 : (p != (int) avm_length(bd->tail))))
    return avm_copied(bad_lapack_spec);
  a = (complex *) avm_matrix_of_list(0,0,0,1,ac->head,sizeof(complex),&result,fault);
  lda = m;
  b = (*fault ? NULL : (complex *) avm_matrix_of_list(0,0,0,1,bd->head,sizeof(complex),&result,fault));
  ldb = p;
  c = (*fault ? NULL : (complex *) avm_vector_of_list(ac->tail,sizeof(complex),&result,fault));
  d = (*fault ? NULL : (complex *) avm_vector_of_list(bd->tail,sizeof(complex),&result,fault));
  x = (complex *) malloc(sizeof(complex) * p);
  work = (complex *) malloc(sizeof(complex));
  lwork = m + n + p;
  if (!(*fault = (*fault ? 1 : !(a ? (b ? (c ? (d ? (x ? !!work : 0) : 0) : 0) : 0) : 0))))
    {
      opt_lwork = -1;
      zgglse_(&m,&n,&p,a,&lda,b,&ldb,c,d,x,work,&opt_lwork,&info);
      opt_lwork = work[0][RE];
      free (work);
      if (work = (complex *) malloc(sizeof(complex) * opt_lwork))
	lwork = opt_lwork;
      else
	*fault = !(work = (complex *) malloc(sizeof(complex) * lwork));
    }
  if (!*fault)
    {
      zgglse_(&m,&n,&p,a,&lda,b,&ldb,c,d,x,work,&lwork,&info);
      if ((info < 0) ? (info >= -13) : 0)
	avm_internal_error (95);
      if (*fault = (info != 0))
	result = (result ? result : avm_copied(lapack_error));
    }
  if (a)
    free (a);
  if (b)
    free (b);
  if (c)
    free (c);
  if (d)
    free (d);
  if (work)
    free (work);
  result = (*fault ? result : avm_list_of_vector((void *) x,n,sizeof(complex),fault));
  if (x)
    free (x);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}









static list
dggglm_caller (operand, fault)
     list operand;
     int *fault;

     /* The operand represents a pair of matrices and a vector
	((A,B),d). The result is a pair of vectors (x,y) satisfying
        Ax + By = d for which |y| is minimal */
{
  int n;
  int m;
  int p;
  double *a;
  int lda;
  double *b;
  int ldb;
  double *d;
  double *x;
  double *y;
  double *work;
  int lwork;
  int info;
  list result,ab;
  int opt_lwork;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(operand ? ((ab = operand->head) ? (ab->head ? !!(ab->tail) : 0) : 0) : 0))
    return avm_copied(bad_lapack_spec);
  n = (int) avm_length(ab->head);
  m = (int) avm_length(ab->head->head);
  p = (int) avm_length(ab->tail->head);
  if (*fault = !(p ? (n ? (m ? ((m <= n) ? ((n <= m+p) ? (n == (int) avm_length(ab->tail)) : 0) : 0) : 0) : 0) : 0))
    return avm_copied(bad_lapack_spec);
  if (*fault = (n != (int) avm_length(operand->tail)))
    return avm_copied(bad_lapack_spec);
  lda = n;
  a = (double *) avm_matrix_of_list(0,0,0,1,ab->head,sizeof(double),&result,fault);
  ldb = n;
  b = (*fault ? NULL : (double *) avm_matrix_of_list(0,0,0,1,ab->tail,sizeof(double),&result,fault));
  d = (*fault ? NULL : (double *) avm_vector_of_list(operand->tail,sizeof(double),&result,fault));
  x = (double *) malloc(sizeof(double) * m);
  y = (double *) malloc(sizeof(double) * p);
  work = (double *) malloc(sizeof(double));
  lwork = n + m + p;
  if (!(*fault = !(a ? (b ? (d ? (x ? (y ? !!work : 0) : 0) : 0) : 0) : 0)))
    {
      opt_lwork = -1;
      dggglm_(&n,&m,&p,a,&lda,b,&ldb,d,x,y,work,&opt_lwork,&info);
      opt_lwork = work[0];
      free(work);
      if (work = (double *) malloc(sizeof(double) * opt_lwork))
	lwork = opt_lwork;
      else
	*fault = !(work = (double *) malloc(sizeof(double) * lwork));
    }
  if (!*fault)
    {
      dggglm_(&n,&m,&p,a,&lda,b,&ldb,d,x,y,work,&lwork,&info);
      if ((info < 0) ? (info >= -13) : 0)
	avm_internal_error (96);
      if (*fault = (info != 0))
	result = (result ? result : avm_copied(lapack_error));
    }
  if (a)
    free (a);
  if (b)
    free (b);
  if (d)
    free (d);
  if (work)
    free (work);
  result = (*fault ? result : avm_list_of_vector((void *) x,m,sizeof(double),fault));
  if (x)
    free (x);
  if (!*fault)
    result = avm_recoverable_join(result,avm_list_of_vector((void *) y,p,sizeof(double),fault));
  if (y)
    free (y);
  *fault = (*fault ? 1 : !result);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}







static list
zggglm_caller (operand, fault)
     list operand;
     int *fault;

     /* same as above with complex numbers */
{
  int n;
  int m;
  int p;
  complex *a;
  int lda;
  complex *b;
  int ldb;
  complex *d;
  complex *x;
  complex *y;
  complex *work;
  int lwork;
  int info;
  list result,ab;
  int opt_lwork;

  if (*fault)
    return NULL;
  result = NULL;
  if (*fault = !(operand ? ((ab = operand->head) ? (ab->head ? !!(ab->tail) : 0) : 0) : 0))
    return avm_copied(bad_lapack_spec);
  n = (int) avm_length(ab->head);
  m = (int) avm_length(ab->head->head);
  p = (int) avm_length(ab->tail->head);
  if (*fault = !(p ? (n ? (m ? ((m <= n) ? ((n <= m+p) ? (n == (int) avm_length(ab->tail)) : 0) : 0) : 0) : 0) : 0))
    return avm_copied(bad_lapack_spec);
  if (*fault = (n != (int) avm_length(operand->tail)))
    return avm_copied(bad_lapack_spec);
  lda = n;
  a = (complex *) avm_matrix_of_list(0,0,0,1,ab->head,sizeof(complex),&result,fault);
  ldb = n;
  b = (*fault ? NULL : (complex *) avm_matrix_of_list(0,0,0,1,ab->tail,sizeof(complex),&result,fault));
  d = (*fault ? NULL : (complex *) avm_vector_of_list(operand->tail,sizeof(complex),&result,fault));
  x = (complex *) malloc(sizeof(complex) * m);
  y = (complex *) malloc(sizeof(complex) * p);
  work = (complex *) malloc(sizeof(complex));
  lwork = n + m + p;
  if (!(*fault = !(a ? (b ? (d ? (x ? (y ? !!work : 0) : 0) : 0) : 0) : 0)))
    {
      opt_lwork = -1;
      zggglm_(&n,&m,&p,a,&lda,b,&ldb,d,x,y,work,&opt_lwork,&info);
      opt_lwork = work[0][RE];
      free(work);
      if (work = (complex *) malloc(sizeof(complex) * opt_lwork))
	lwork = opt_lwork;
      else
	*fault = !(work = (complex *) malloc(sizeof(complex) * lwork));
    }
  if (!*fault)
    {
      zggglm_(&n,&m,&p,a,&lda,b,&ldb,d,x,y,work,&lwork,&info);
      if ((info < 0) ? (info >= -13) : 0)
	avm_internal_error (97);
      if (*fault = (info != 0))
	result = (result ? result : avm_copied(lapack_error));
    }
  if (a)
    free (a);
  if (b)
    free (b);
  if (d)
    free (d);
  if (work)
    free (work);
  result = (*fault ? result : avm_list_of_vector((void *) x,m,sizeof(complex),fault));
  if (x)
    free (x);
  if (!*fault)
    result = avm_recoverable_join(result,avm_list_of_vector((void *) y,p,sizeof(complex),fault));
  if (y)
    free (y);
  *fault = (*fault ? 1 : !result);
  return (*fault ? (result ? result : avm_copied(memory_overflow)) : result);
}







static list
dgeesx_caller (operand, fault)
     list operand;
     int *fault;
{
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}







static list
zgeesx_caller (operand, fault)
     list operand;
     int *fault;
{
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}






#endif



list
avm_have_lapack_call (function_name, fault)
     list function_name;
     int *fault;

/* this reports the availability of a function */

{
#if HAVE_LAPACK
  list membership;
  list comparison;
  list result;

  if (!initialized)
    avm_initialize_lapack ();
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
avm_lapack_call (function_name, argument, fault)
     list function_name;
     list argument;
     int *fault;
{
#if HAVE_LAPACK
  list message;
  int function_number;

  if (*fault)
    return NULL;
  if (! initialized)
    avm_initialize_lapack ();
  if (!(function_number = 0xff & (function_name ? function_name->characterization : 0)))
    {
      message = avm_position (function_name, funs, fault);
      if (*fault)
	return (message);
      if (*fault = !message)
	return(avm_copied (unrecognized_function_name));
      function_number = message->characterization;
      function_name->characterization = function_number;
      avm_dispose (message);
    }
  switch (function_number)
    {
    case 1: return dgesvx_caller (argument, fault);
    case 2: return zgesvx_caller (argument, fault);
    case 3: return dgesdd_caller (argument, fault);
    case 4: return zgesdd_caller (argument, fault);
    case 5: return dgelsd_caller (argument, fault);
    case 6: return zgelsd_caller (argument, fault);
    case 7: return dsyevr_caller (argument, fault);
    case 8: return zheevr_caller (argument, fault);
    case 9: return dgeevx_caller (argument, fault);
    case 10: return zgeevx_caller (argument, fault);
    case 11: return dpptrf_caller (argument, fault);
    case 12: return zpptrf_caller (argument, fault);
    case 13: return dspev_caller (argument, fault);
    case 14: return zhpev_caller (argument, fault);
    case 15: return dgglse_caller (argument, fault);
    case 16: return zgglse_caller (argument, fault);
    case 17: return dggglm_caller (argument, fault);
    case 18: return zggglm_caller (argument, fault);
    case 19: return dgeesx_caller (argument, fault);
    case 20: return zgeesx_caller (argument, fault);
    }
#endif /* HAVE_LAPACK */
  *fault = 1;
  return avm_copied (unrecognized_function_name);
}







void
avm_initialize_lapack ()

     /* This initializes some static data structures. */
{
  char *funames[] = {
    "dgesvx",
    "zgesvx",
    "dgesdd",
    "zgesdd",
    "dgelsd",
    "zgelsd",
    "dsyevr",
    "zheevr",
    "dgeevx",
    "zgeevx",
    "dpptrf",
    "zpptrf",
    "dspev",
    "zhpev",
    "dgglse",
    "zgglse",
    "dggglm",
    "zggglm",
    "dgeesx",
    "zgeesx",
    NULL};            /* add more function names here up to a total of 255 */
  list back;
  int string_number;
  char S;

  S = 'S';
  if (initialized)
    return;
  initialized = 1;
  avm_initialize_lists ();
  avm_initialize_matcon ();
  avm_initialize_chrcodes ();
  wild = avm_strung("*");
  memory_overflow = avm_join (avm_strung ("memory overflow"), NULL);
  lapack_error = avm_join (avm_strung ("lapack error"), NULL);
  bad_lapack_spec = avm_join (avm_strung ("bad lapack specification"), NULL);
  unrecognized_function_name = avm_join (avm_strung ("unrecognized lapack function name"), NULL);
#if HAVE_LAPACK
  safemin = dlamch_(&S);
#endif
  string_number = 0;
  funs = back = NULL;
  while (funames[string_number])
    avm_enqueue (&funs, &back, avm_standard_strung (funames[string_number++]));
}





void
avm_count_lapack ()

     /* This frees some static data structures as an aid to the
	detection of memory leaks. */
{

  if (!initialized)
    return;
  initialized = 0;
  avm_dispose (funs);
  avm_dispose (wild);
  avm_dispose (lapack_error);
  avm_dispose (bad_lapack_spec);
  avm_dispose (memory_overflow);
  avm_dispose (unrecognized_function_name);
  funs = NULL;
  wild = NULL;
  lapack_error = NULL;
  memory_overflow = NULL;
  unrecognized_function_name = NULL;
}
