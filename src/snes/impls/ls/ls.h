/* $Id: ls.h,v 1.11 1999/10/23 00:00:57 bsmith Exp bsmith $ */

/* 
   Private context for a Newton line search method for solving
   systems of nonlinear equations
 */

#ifndef __SNES_EQLS_H
#define __SNES_EQLS_H
#include "src/snes/snesimpl.h"

typedef struct {
  int       (*LineSearch)(SNES,void*,Vec,Vec,Vec,Vec,Vec,PetscReal,PetscReal*,PetscReal*,int*);
  void      *lsP;                              /* user-defined line-search context (optional) */
  /* --------------- Parameters used by line search method ----------------- */
  PetscReal alpha;		                    /* used to determine sufficient reduction */
  PetscReal maxstep;                           /* maximum step size */
  PetscReal steptol;                           /* step convergence tolerance */
  int       (*CheckStep)(SNES,void*,Vec,PetscTruth*); /* step-checking routine (optional) */
  void      *checkP;                           /* user-defined step-checking context (optional) */
} SNES_EQ_LS;

#endif

