/*$Id: ex51.c,v 1.19 2001/04/10 19:35:44 bsmith Exp bsmith $*/

static char help[] = "Tests MatIncreaseOverlap(), MatGetSubMatrices() for MatBAIJ format.\n";

#include "petscmat.h"

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  Mat         A,B,*submatA,*submatB;
  int         bs=1,m=43,ov=1,i,j,k,*rows,*cols,ierr,M,nd=5,*idx,size,mm,nn;
  Scalar      *vals,rval;
  IS          *is1,*is2;
  PetscRandom rand;
  Vec         xx,s1,s2;
  double      s1norm,s2norm,rnorm,tol = 1.e-10;
  PetscTruth  flg;

  PetscInitialize(&argc,&args,(char *)0,help);
 

  ierr = PetscOptionsGetInt(PETSC_NULL,"-mat_block_size",&bs,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-mat_size",&m,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-ov",&ov,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(PETSC_NULL,"-nd",&nd,PETSC_NULL);CHKERRQ(ierr);
  M    = m*bs;

  ierr = MatCreateSeqBAIJ(PETSC_COMM_SELF,bs,M,M,1,PETSC_NULL,&A);CHKERRQ(ierr);
  ierr = MatCreateSeqAIJ(PETSC_COMM_SELF,M,M,15,PETSC_NULL,&B);CHKERRQ(ierr);
  ierr = PetscRandomCreate(PETSC_COMM_SELF,RANDOM_DEFAULT,&rand);CHKERRQ(ierr);

  ierr = PetscMalloc(bs*sizeof(int),&rows);CHKERRQ(ierr);
  ierr = PetscMalloc(bs*sizeof(int),&cols);CHKERRQ(ierr);
  ierr = PetscMalloc(bs*bs*sizeof(PetscScalar),&vals);CHKERRQ(ierr);
  ierr = PetscMalloc(M*sizeof(PetscScalar),&idx);CHKERRQ(ierr);
  
  /* Now set blocks of values */
  for (i=0; i<20*bs; i++) {
      ierr = PetscRandomGetValue(rand,&rval);CHKERRQ(ierr);
      cols[0] = bs*(int)(PetscRealPart(rval)*m);
      ierr = PetscRandomGetValue(rand,&rval);CHKERRQ(ierr);
      rows[0] = bs*(int)(PetscRealPart(rval)*m);
      for (j=1; j<bs; j++) {
        rows[j] = rows[j-1]+1;
        cols[j] = cols[j-1]+1;
      }

      for (j=0; j<bs*bs; j++) {
        ierr = PetscRandomGetValue(rand,&rval);CHKERRQ(ierr);
        vals[j] = rval;
      }
      ierr = MatSetValues(A,bs,rows,bs,cols,vals,ADD_VALUES);CHKERRQ(ierr);
      ierr = MatSetValues(B,bs,rows,bs,cols,vals,ADD_VALUES);CHKERRQ(ierr);
  }

  ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyBegin(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(B,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

    /* Test MatIncreaseOverlap() */
  ierr = PetscMalloc(nd*sizeof(IS **),&is1);CHKERRQ(ierr);
  ierr = PetscMalloc(nd*sizeof(IS **),&is2);CHKERRQ(ierr);

  
  for (i=0; i<nd; i++) {
    ierr = PetscRandomGetValue(rand,&rval);CHKERRQ(ierr);
    size = (int)(PetscRealPart(rval)*m);
    for (j=0; j<size; j++) {
      ierr = PetscRandomGetValue(rand,&rval);CHKERRQ(ierr);
      idx[j*bs] = bs*(int)(PetscRealPart(rval)*m);
      for (k=1; k<bs; k++) idx[j*bs+k] = idx[j*bs]+k;
    }
    ierr = ISCreateGeneral(PETSC_COMM_SELF,size*bs,idx,is1+i);CHKERRQ(ierr);
    ierr = ISCreateGeneral(PETSC_COMM_SELF,size*bs,idx,is2+i);CHKERRQ(ierr);
  }
  ierr = MatIncreaseOverlap(A,nd,is1,ov);CHKERRQ(ierr);
  ierr = MatIncreaseOverlap(B,nd,is2,ov);CHKERRQ(ierr);

  for (i=0; i<nd; ++i) { 
    ierr = ISEqual(is1[i],is2[i],&flg);CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_SELF,"i=%d, flg =%d\n",i,flg);CHKERRQ(ierr);
  }

  for (i=0; i<nd; ++i) { 
    ierr = ISSort(is1[i]);CHKERRQ(ierr);
    ierr = ISSort(is2[i]);CHKERRQ(ierr);
  }
  
  ierr = MatGetSubMatrices(A,nd,is1,is1,MAT_INITIAL_MATRIX,&submatA);CHKERRQ(ierr);
  ierr = MatGetSubMatrices(B,nd,is2,is2,MAT_INITIAL_MATRIX,&submatB);CHKERRQ(ierr);

  /* Test MatMult() */
  for (i=0; i<nd; i++) {
    ierr = MatGetSize(submatA[i],&mm,&nn);CHKERRQ(ierr);
    ierr = VecCreateSeq(PETSC_COMM_SELF,mm,&xx);CHKERRQ(ierr);
    ierr = VecDuplicate(xx,&s1);CHKERRQ(ierr);
    ierr = VecDuplicate(xx,&s2);CHKERRQ(ierr);
    for (j=0; j<3; j++) {
      ierr = VecSetRandom(rand,xx);CHKERRQ(ierr);
      ierr = MatMult(submatA[i],xx,s1);CHKERRQ(ierr);
      ierr = MatMult(submatB[i],xx,s2);CHKERRQ(ierr);
      ierr = VecNorm(s1,NORM_2,&s1norm);CHKERRQ(ierr);
      ierr = VecNorm(s2,NORM_2,&s2norm);CHKERRQ(ierr);
      rnorm = s2norm-s1norm;
      if (rnorm<-tol || rnorm>tol) { 
        ierr = PetscPrintf(PETSC_COMM_SELF,"Error:MatMult - Norm1=%16.14e Norm2=%16.14e\n",s1norm,s2norm);  CHKERRQ(ierr);
      }
    }
    ierr = VecDestroy(xx);CHKERRQ(ierr);
    ierr = VecDestroy(s1);CHKERRQ(ierr);
    ierr = VecDestroy(s2);CHKERRQ(ierr);
  } 
  /* Now test MatGetSubmatrices with MAT_REUSE_MATRIX option */
  ierr = MatGetSubMatrices(A,nd,is1,is1,MAT_REUSE_MATRIX,&submatA);CHKERRQ(ierr);
  ierr = MatGetSubMatrices(B,nd,is2,is2,MAT_REUSE_MATRIX,&submatB);CHKERRQ(ierr);
  
  /* Test MatMult() */
  for (i=0; i<nd; i++) {
    ierr = MatGetSize(submatA[i],&mm,&nn);CHKERRQ(ierr);
    ierr = VecCreateSeq(PETSC_COMM_SELF,mm,&xx);CHKERRQ(ierr);
    ierr = VecDuplicate(xx,&s1);CHKERRQ(ierr);
    ierr = VecDuplicate(xx,&s2);CHKERRQ(ierr);
    for (j=0; j<3; j++) {
      ierr = VecSetRandom(rand,xx);CHKERRQ(ierr);
      ierr = MatMult(submatA[i],xx,s1);CHKERRQ(ierr);
      ierr = MatMult(submatB[i],xx,s2);CHKERRQ(ierr);
      ierr = VecNorm(s1,NORM_2,&s1norm);CHKERRQ(ierr);
      ierr = VecNorm(s2,NORM_2,&s2norm);CHKERRQ(ierr);
      rnorm = s2norm-s1norm;
      if (rnorm<-tol || rnorm>tol) { 
        ierr = PetscPrintf(PETSC_COMM_SELF,"Error:MatMult - Norm1=%16.14e Norm2=%16.14e\n",s1norm,s2norm);CHKERRQ(ierr);
      }
    }
    ierr = VecDestroy(xx);CHKERRQ(ierr);
    ierr = VecDestroy(s1);CHKERRQ(ierr);
    ierr = VecDestroy(s2);CHKERRQ(ierr);
  } 
     
  /* Free allocated memory */
  for (i=0; i<nd; ++i) { 
    ierr = ISDestroy(is1[i]);CHKERRQ(ierr);
    ierr = ISDestroy(is2[i]);CHKERRQ(ierr);
    ierr = MatDestroy(submatA[i]);CHKERRQ(ierr);
    ierr = MatDestroy(submatB[i]);CHKERRQ(ierr);
 }
  ierr = PetscFree(is1);CHKERRQ(ierr);
  ierr = PetscFree(is2);CHKERRQ(ierr);
  ierr = PetscFree(idx);CHKERRQ(ierr);
  ierr = PetscFree(rows);CHKERRQ(ierr);
  ierr = PetscFree(cols);CHKERRQ(ierr);
  ierr = PetscFree(vals);CHKERRQ(ierr);
  ierr = MatDestroy(A);CHKERRQ(ierr);
  ierr = MatDestroy(B);CHKERRQ(ierr);
  ierr = PetscFree(submatA);CHKERRQ(ierr);
  ierr = PetscFree(submatB);CHKERRQ(ierr);
  ierr = PetscRandomDestroy(rand);CHKERRQ(ierr);
  ierr = PetscFinalize();CHKERRQ(ierr);
  return 0;
}
