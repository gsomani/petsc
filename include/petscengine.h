/* $Id: petscengine.h,v 1.3 2000/08/01 20:58:40 bsmith Exp bsmith $ */

/*
    Defines an interface to the Matlab Engine from PETSc
*/

#if !defined(__PETSCENGINE_H)
#define __PETSCENGINE_H

#define MATLABENGINE_COOKIE PETSC_COOKIE+12

typedef struct _p_PetscMatlabEngine* PetscMatlabEngine;

EXTERN int PetscMatlabEngineCreate(MPI_Comm,char*,PetscMatlabEngine*);
EXTERN int PetscMatlabEngineDestroy(PetscMatlabEngine);
EXTERN int PetscMatlabEngineEvaluate(PetscMatlabEngine,char*,...);
EXTERN int PetscMatlabEngineGetOutput(PetscMatlabEngine,char **);
EXTERN int PetscMatlabEnginePrintOutput(PetscMatlabEngine,FILE*);
EXTERN int PetscMatlabEnginePut(PetscMatlabEngine,PetscObject);
EXTERN int PetscMatlabEngineGet(PetscMatlabEngine,PetscObject);
EXTERN int PetscMatlabEnginePutArray(PetscMatlabEngine,int,int,PetscScalar*,char*);
EXTERN int PetscMatlabEngineGetArray(PetscMatlabEngine,int,int,PetscScalar*,char*);

EXTERN PetscMatlabEngine MATLAB_ENGINE_(MPI_Comm);
#define MATLAB_ENGINE_SELF  MATLAB_ENGINE_(PETSC_COMM_SELF)
#define MATLAB_ENGINE_WORLD MATLAB_ENGINE_(PETSC_COMM_WORLD)

#endif






