/* $Id: dvec2.c,v 1.27 1996/03/19 21:23:01 bsmith Exp balay $ */

/* 
   Defines some vector operation functions that are shared by 
  sequential and parallel vectors.
*/
#include "src/inline/dot.h"
#include "src/inline/setval.h"
#include "src/inline/axpy.h"
#include "vecimpl.h"             
#include "dvecimpl.h"   
#include "draw.h"          
#include "pinclude/pviewer.h"

static int VecMDot_Seq(int nv,Vec xin,Vec *y, Scalar *z )
{
  Vec_Seq *x = (Vec_Seq *)xin->data;
  register int n = x->n, i;
  Scalar   sum,*xx = x->array, *yy;

  /* This could be unrolled to reuse x[j] values */
  for (i=0; i<nv; i++) {
    sum = 0.0;
    yy = ((Vec_Seq *)(y[i]->data))->array;
    DOT(sum,xx,yy,n);
    z[i] = sum;
  }
  PLogFlops(nv*(2*x->n-1));
  return 0;
}

static int VecMax_Seq(Vec xin,int* idx,double * z )
{
  Vec_Seq         *x = (Vec_Seq *) xin->data;
  register int    i, j=0, n = x->n;
  register double max = -1.e40, tmp;
  Scalar          *xx = x->array;

  for (i=0; i<n; i++) {
#if defined(PETSC_COMPLEX)
    if ((tmp = real(*xx++)) > max) { j = i; max = tmp;}
#else
    if ((tmp = *xx++) > max) { j = i; max = tmp; } 
#endif
  }
  *z   = max;
  if (idx) *idx = j;
  return 0;
}

static int VecMin_Seq(Vec xin,int* idx,double * z )
{
  Vec_Seq         *x = (Vec_Seq *) xin->data;
  register int    i, j=0, n = x->n;
  register double min = 1.e40, tmp;
  Scalar          *xx = x->array;

  for ( i=0; i<n; i++ ) {
#if defined(PETSC_COMPLEX)
    if ((tmp = real(*xx++)) < min) { j = i; min = tmp;}
#else
    if ((tmp = *xx++) < min) { j = i; min = tmp; } 
#endif
  }
  *z   = min;
  if (idx) *idx = j;
  return 0;
}

static int VecSet_Seq(Scalar* alpha,Vec xin)
{
  Vec_Seq      *x = (Vec_Seq *)xin->data;
  register int n = x->n;
  Scalar       *xx = x->array, oalpha = *alpha;

  if (oalpha == 0.0) {
    PetscMemzero(xx,n*sizeof(Scalar));
  }
  else {
    SET(xx,n,oalpha);
  }
  return 0;
}

static int VecSetRandom_Seq(PetscRandom r,Vec xin)
{
  Vec_Seq      *x = (Vec_Seq *)xin->data;
  register int n = x->n;
  int          i, ierr;
  Scalar       *xx = x->array;

  for (i=0; i<n; i++) {ierr = PetscRandomGetValue(r,&xx[i]); CHKERRQ(ierr);}
  return 0;
}

static int VecMAXPY_Seq( int nv, Scalar *alpha, Vec yin, Vec *x )
{
  Vec_Seq      *y = (Vec_Seq *) yin->data;
  register int n = y->n;
  int          j;
  Scalar       *yy = y->array, *xx,oalpha;

  PLogFlops(nv*2*n);
  for (j=0; j<nv; j++) {
    xx     = ((Vec_Seq *)(x[j]->data))->array;
    oalpha = alpha[j];
    if (oalpha == -1.0) {
      YMX(yy,xx,n);
    }
    else if (oalpha == 1.0) {
      YPX(yy,xx,n);
    }
    else if (oalpha != 0.0) {
      APXY(yy,oalpha,xx,n);
    }
  }
  return 0;
}

static int VecAYPX_Seq(Scalar *alpha, Vec xin, Vec yin )
{
  Vec_Seq      *x = (Vec_Seq *)xin->data, *y = (Vec_Seq *)yin->data;
  register int i,n = x->n;
  Scalar       *xx = x->array, *yy = y->array, oalpha = *alpha;

  PLogFlops(2*n);
  for ( i=0; i<n; i++ ) {
    yy[i] = xx[i] + oalpha*yy[i];
  }
  return 0;
}

/*
   IBM ESSL contains a routine dzaxpy() that is our WAXPY() but it appears
  to be slower then a regular C loop. Hence we do not include it.
  void ?zaxpy(int*,Scalar*,Scalar*,int*,Scalar*,int*,Scalar*,int*);
*/

static int VecWAXPY_Seq(Scalar* alpha,Vec xin,Vec yin,Vec win )
{
  Vec_Seq      *w = (Vec_Seq *)win->data, *x = (Vec_Seq *)xin->data;
  Vec_Seq      *y = (Vec_Seq *)yin->data;
  register int i, n = x->n;
  Scalar       *xx = x->array, *yy = y->array, *ww = w->array, oalpha = *alpha;

  if (oalpha == 1.0) {
    PLogFlops(n);
    /* could call BLAS axpy after call to memcopy, but may be slower */
    for (i=0; i<n; i++) ww[i] = yy[i] + xx[i];
  }
  else if (oalpha == -1.0) {
    PLogFlops(n);
    for (i=0; i<n; i++) ww[i] = yy[i] - xx[i];
  }
  else if (oalpha == 0.0) {
    PetscMemcpy(ww,yy,n*sizeof(Scalar));
  }
  else {
    for (i=0; i<n; i++) ww[i] = yy[i] + oalpha * xx[i];
    PLogFlops(2*n);
  }
  return 0;
}

static int VecPointwiseMult_Seq( Vec xin, Vec yin, Vec win )
{
  Vec_Seq      *w = (Vec_Seq *)win->data, *x = (Vec_Seq *)xin->data;
  Vec_Seq      *y = (Vec_Seq *)yin->data;
  register int n = x->n, i;
  Scalar       *xx = x->array, *yy = y->array, *ww = w->array;

  PLogFlops(n);
  for (i=0; i<n; i++) ww[i] = xx[i] * yy[i];
  return 0;
}

static int VecPointwiseDivide_Seq(Vec xin,Vec yin,Vec win )
{
  Vec_Seq      *w = (Vec_Seq *)win->data, *x = (Vec_Seq *)xin->data;
  Vec_Seq      *y = (Vec_Seq *)yin->data;
  register int n = x->n, i;
  Scalar       *xx = x->array, *yy = y->array, *ww = w->array;

  PLogFlops(n);
  for (i=0; i<n; i++) ww[i] = xx[i] / yy[i];
  return 0;
}


static int VecGetArray_Seq(Vec vin,Scalar **a)
{
  Vec_Seq *v = (Vec_Seq *)vin->data;
  *a =  v->array; return 0;
}

static int VecGetSize_Seq(Vec vin,int *size)
{
  Vec_Seq *v = (Vec_Seq *)vin->data;
  *size = v->n; return 0;
}


 



