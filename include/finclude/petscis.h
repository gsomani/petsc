!
!  $Id: petscis.h,v 1.20 2000/01/11 21:04:11 bsmith Exp balay $;
!
!  Include file for Fortran use of the IS (index set) package in PETSc
!
#if !defined (__PETSCIS_H)
#define __PETSCIS_H

#define IS                         PetscFortranAddr
#define ISLocalToGlobalMapping     PetscFortranAddr
#define ISColoring                 PetscFortranAddr
#define ISType                     integer
#define ISGlobalToLocalMappingType integer

#endif


#if !defined (PETSC_AVOID_DECLARATIONS)

      integer IS_GENERAL,IS_STRIDE,IS_BLOCK
      parameter (IS_GENERAL = 0,IS_STRIDE = 1,IS_BLOCK = 2)

      integer IS_GTOLM_MASK,IS_GTOLM_DROP 
      parameter (IS_GTOLM_MASK =0,IS_GTOLM_DROP = 1)

!
!  End of Fortran include file for the IS package in PETSc

#endif
