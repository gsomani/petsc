/*$Id: sregis.c,v 1.38 2001/04/10 19:36:03 bsmith Exp bsmith $*/

#include "src/mat/matimpl.h"     /*I       "petscmat.h"   I*/

EXTERN_C_BEGIN
EXTERN int MatOrdering_Natural(Mat,MatOrderingType,IS*,IS*);
EXTERN int MatOrdering_ND(Mat,MatOrderingType,IS*,IS*);
EXTERN int MatOrdering_1WD(Mat,MatOrderingType,IS*,IS*);
EXTERN int MatOrdering_QMD(Mat,MatOrderingType,IS*,IS*);
EXTERN int MatOrdering_RCM(Mat,MatOrderingType,IS*,IS*);
EXTERN int MatOrdering_RowLength(Mat,MatOrderingType,IS*,IS*);
EXTERN int MatOrdering_DSC(Mat,MatOrderingType,IS*,IS*);
EXTERN_C_END

#undef __FUNCT__  
#define __FUNCT__ "MatOrderingRegisterAll"
/*@C
  MatOrderingRegisterAll - Registers all of the matrix 
  reordering routines in PETSc.

  Not Collective

  Level: developer

  Adding new methods:
  To add a new method to the registry. Copy this routine and 
  modify it to incorporate a call to MatReorderRegister() for 
  the new method, after the current list.

  Restricting the choices: To prevent all of the methods from being
  registered and thus save memory, copy this routine and comment out
  those orderigs you do not wish to include.  Make sure that the
  replacement routine is linked before libpetscmat.a.

.keywords: matrix, reordering, register, all

.seealso: MatOrderingRegisterDynamic(), MatOrderingRegisterDestroy()
@*/
int MatOrderingRegisterAll(char *path)
{
  int           ierr;

  PetscFunctionBegin;
  MatOrderingRegisterAllCalled = PETSC_TRUE;

  ierr = MatOrderingRegisterDynamic(MATORDERING_NATURAL,  path,"MatOrdering_Natural"  ,MatOrdering_Natural);CHKERRQ(ierr);
  ierr = MatOrderingRegisterDynamic(MATORDERING_ND,       path,"MatOrdering_ND"       ,MatOrdering_ND);CHKERRQ(ierr);
  ierr = MatOrderingRegisterDynamic(MATORDERING_1WD,      path,"MatOrdering_1WD"      ,MatOrdering_1WD);CHKERRQ(ierr);
  ierr = MatOrderingRegisterDynamic(MATORDERING_RCM,      path,"MatOrdering_RCM"      ,MatOrdering_RCM);CHKERRQ(ierr);
  ierr = MatOrderingRegisterDynamic(MATORDERING_QMD,      path,"MatOrdering_QMD"      ,MatOrdering_QMD);CHKERRQ(ierr);
  ierr = MatOrderingRegisterDynamic(MATORDERING_ROWLENGTH,path,"MatOrdering_RowLength",MatOrdering_RowLength);CHKERRQ(ierr);
#if defined(PETSC_HAVE_DSCPACK) && !defined(PETSC_USE_COMPLEX)  && !defined(PETSC_USE_SINGLE)
  ierr = MatOrderingRegisterDynamic(MATORDERING_DSC_ND    ,path,"MatOrdering_DSC"     ,MatOrdering_DSC);CHKERRQ(ierr);
  ierr = MatOrderingRegisterDynamic(MATORDERING_DSC_MMD   ,path,"MatOrdering_DSC"     ,MatOrdering_DSC);CHKERRQ(ierr);
  ierr = MatOrderingRegisterDynamic(MATORDERING_DSC_MDF   ,path,"MatOrdering_DSC"     ,MatOrdering_DSC);CHKERRQ(ierr);
#endif
  PetscFunctionReturn(0);
}

