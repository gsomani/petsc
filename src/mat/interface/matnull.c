/*$Id: matnull.c,v 1.37 2001/03/23 23:21:44 balay Exp bsmith $*/
/*
    Routines to project vectors out of null spaces.
*/

#include "src/mat/matimpl.h"      /*I "petscmat.h" I*/
#include "petscsys.h"

#undef __FUNCT__  
#define __FUNCT__ "MatNullSpaceCreate"
/*@C
   MatNullSpaceCreate - Creates a data structure used to project vectors 
   out of null spaces.

   Collective on MPI_Comm

   Input Parameters:
+  comm - the MPI communicator associated with the object
.  has_cnst - PETSC_TRUE if the null space contains the constant vector; otherwise PETSC_FALSE
.  n - number of vectors (excluding constant vector) in null space
-  vecs - the vectors that span the null space (excluding the constant vector);
          these vectors must be orthonormal

   Output Parameter:
.  SP - the null space context

   Level: advanced

.keywords: PC, null space, create

.seealso: MatNullSpaceDestroy(), MatNullSpaceRemove(), PCNullSpaceAttach()
@*/
int MatNullSpaceCreate(MPI_Comm comm,int has_cnst,int n,Vec *vecs,MatNullSpace *SP)
{
  MatNullSpace sp;

  PetscFunctionBegin;
  PetscHeaderCreate(sp,_p_MatNullSpace,int,MATNULLSPACE_COOKIE,0,"MatNullSpace",comm,MatNullSpaceDestroy,0);
  PetscLogObjectCreate(sp);
  PetscLogObjectMemory(sp,sizeof(struct _p_MatNullSpace));

  sp->has_cnst = has_cnst; 
  sp->n        = n;
  sp->vecs     = vecs;
  sp->vec      = PETSC_NULL;

  *SP          = sp;
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatNullSpaceDestroy"
/*@
   MatNullSpaceDestroy - Destroys a data structure used to project vectors 
   out of null spaces.

   Collective on MatNullSpace

   Input Parameter:
.  sp - the null space context to be destroyed

   Level: advanced

.keywords: PC, null space, destroy

.seealso: MatNullSpaceCreate(), MatNullSpaceRemove()
@*/
int MatNullSpaceDestroy(MatNullSpace sp)
{
  int ierr;

  PetscFunctionBegin;
  if (--sp->refct > 0) PetscFunctionReturn(0);

  if (sp->vec) {ierr = VecDestroy(sp->vec);CHKERRQ(ierr);}

  PetscLogObjectDestroy(sp);
  PetscHeaderDestroy(sp);
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatNullSpaceRemove"
/*@
   MatNullSpaceRemove - Removes all the components of a null space from a vector.

   Collective on MatNullSpace

   Input Parameters:
+  sp - the null space context
-  vec - the vector from which the null space is to be removed 

   Level: advanced

.keywords: PC, null space, remove

.seealso: MatNullSpaceCreate(), MatNullSpaceDestroy()
@*/
int MatNullSpaceRemove(MatNullSpace sp,Vec vec,Vec *out)
{
  PetscScalar sum;
  int    j,n = sp->n,N,ierr;
  Vec    l = vec;

  PetscFunctionBegin;
  if (out) {
    if (!sp->vec) {
      ierr = VecDuplicate(vec,&sp->vec);CHKERRQ(ierr);
    }
    *out = sp->vec;
    ierr = VecCopy(vec,*out);CHKERRQ(ierr);
    l    = *out;
  }

  if (sp->has_cnst) {
    ierr = VecSum(l,&sum);CHKERRQ(ierr);
    ierr = VecGetSize(l,&N);CHKERRQ(ierr);
    sum  = sum/(-1.0*N);
    ierr = VecShift(&sum,l);CHKERRQ(ierr);
  }

  for (j=0; j<n; j++) {
    ierr = VecDot(l,sp->vecs[j],&sum);CHKERRQ(ierr);
    sum  = -sum;
    ierr = VecAXPY(&sum,sp->vecs[j],l);CHKERRQ(ierr);
  }
  
  PetscFunctionReturn(0);
}

#undef __FUNCT__  
#define __FUNCT__ "MatNullSpaceTest"
/*@
   MatNullSpaceTest  - Tests if the claimed null space is really a
     null space of a matrix

   Collective on MatNullSpace

   Input Parameters:
+  sp - the null space context
-  mat - the matrix

   Level: advanced

.keywords: PC, null space, remove

.seealso: MatNullSpaceCreate(), MatNullSpaceDestroy()
@*/
int MatNullSpaceTest(MatNullSpace sp,Mat mat)
{
  PetscScalar     sum;
  PetscReal  nrm;
  int        j,n = sp->n,N,ierr,m;
  Vec        l,r;
  MPI_Comm   comm = sp->comm;
  PetscTruth flg1,flg2;

  PetscFunctionBegin;
  ierr = PetscOptionsHasName(PETSC_NULL,"-mat_null_space_test_view",&flg1);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-mat_null_space_test_view_draw",&flg2);CHKERRQ(ierr);

  if (!sp->vec) {
    if (n) {
      ierr = VecDuplicate(sp->vecs[0],&sp->vec);CHKERRQ(ierr);
    } else {
      ierr = MatGetLocalSize(mat,&m,PETSC_NULL);CHKERRQ(ierr);
      ierr = VecCreateMPI(sp->comm,m,PETSC_DETERMINE,&sp->vec);CHKERRQ(ierr);
    }
  }
  l    = sp->vec;

  if (sp->has_cnst) {
    ierr = VecDuplicate(l,&r);CHKERRQ(ierr);
    ierr = VecGetSize(l,&N);CHKERRQ(ierr);
    sum  = 1.0/N;
    ierr = VecSet(&sum,l);CHKERRQ(ierr);
    ierr = MatMult(mat,l,r);CHKERRQ(ierr);
    ierr = VecNorm(r,NORM_2,&nrm);CHKERRQ(ierr);
    if (nrm < 1.e-7) {ierr = PetscPrintf(comm,"Constants are likely null vector");CHKERRQ(ierr);}
    else {ierr = PetscPrintf(comm,"Constants are unlikely null vector ");CHKERRQ(ierr);}
    ierr = PetscPrintf(comm,"|| A * 1 || = %g\n",nrm);CHKERRQ(ierr);
    if (nrm > 1.e-7 && flg1) {ierr = VecView(r,PETSC_VIEWER_STDOUT_(comm));CHKERRQ(ierr);}
    if (nrm > 1.e-7 && flg2) {ierr = VecView(r,PETSC_VIEWER_DRAW_(comm));CHKERRQ(ierr);}
    ierr = VecDestroy(r);CHKERRQ(ierr);
  }

  for (j=0; j<n; j++) {
    ierr = (*mat->ops->mult)(mat,sp->vecs[j],l);CHKERRQ(ierr);
    ierr = VecNorm(l,NORM_2,&nrm);CHKERRQ(ierr);
    if (nrm < 1.e-7) {ierr = PetscPrintf(comm,"Null vector %d is likely null vector",j);CHKERRQ(ierr);}
    else {ierr = PetscPrintf(comm,"Null vector %d unlikely null vector ",j);CHKERRQ(ierr);}
    ierr = PetscPrintf(comm,"|| A * v[%d] || = %g\n",j,nrm);CHKERRQ(ierr);
    if (nrm > 1.e-7 && flg1) {ierr = VecView(l,PETSC_VIEWER_STDOUT_(comm));CHKERRQ(ierr);}
    if (nrm > 1.e-7 && flg2) {ierr = VecView(l,PETSC_VIEWER_DRAW_(comm));CHKERRQ(ierr);}
  }
  
  PetscFunctionReturn(0);
}

