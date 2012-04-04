
static char help[] = "Solves PDE problem from ex22.c\n\n";

#include <petscdmda.h>
#include <petscpf.h>
#include <petscsnes.h>

/*

       In this example the PDE is 
                             Uxx + U^2 = 2, 
                            u(0) = .25
                            u(1) = 0

       The exact solution for u is given by u(x) = x*x - 1.25*x + .25

       Use the usual centered finite differences.


       See ex22.c for a design optimization problem

*/

typedef struct {
  PetscViewer  u_viewer;
  PetscViewer  fu_viewer;
} UserCtx;

extern PetscErrorCode FormFunction(DM,Vec,Vec);

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  PetscErrorCode ierr;
  UserCtx        user;
  DM             da;
  SNES           snes;

  PetscInitialize(&argc,&argv,PETSC_NULL,help);

  /* Hardwire several options; can be changed at command line */
  ierr = PetscOptionsSetValue("-snes_grid_sequence","1");CHKERRQ(ierr);
  ierr = PetscOptionsSetValue("-ksp_type","fgmres");CHKERRQ(ierr);
  ierr = PetscOptionsSetValue("-ksp_max_it","5");CHKERRQ(ierr);
  ierr = PetscOptionsSetValue("-pc_type","mg");CHKERRQ(ierr);
  ierr = PetscOptionsSetValue("-pc_mg_type","full");CHKERRQ(ierr);
  ierr = PetscOptionsSetValue("-mg_coarse_ksp_type","gmres");CHKERRQ(ierr);
  ierr = PetscOptionsSetValue("-mg_levels_ksp_type","gmres");CHKERRQ(ierr);
  ierr = PetscOptionsSetValue("-mg_coarse_ksp_max_it","3");CHKERRQ(ierr);
  ierr = PetscOptionsSetValue("-mg_levels_ksp_max_it","3");CHKERRQ(ierr);
  ierr = PetscOptionsInsert(&argc,&argv,PETSC_NULL);CHKERRQ(ierr); 
  
  /* Create a global vector from a da arrays */
  ierr = DMDACreate1d(PETSC_COMM_WORLD,DMDA_BOUNDARY_NONE,-5,1,1,PETSC_NULL,&da);CHKERRQ(ierr);
  ierr = DMSetFunction(da,FormFunction);CHKERRQ(ierr);

  /* create graphics windows */
  ierr = PetscViewerDrawOpen(PETSC_COMM_WORLD,0,"u - state variables",-1,-1,-1,-1,&user.u_viewer);CHKERRQ(ierr);
  ierr = PetscViewerDrawOpen(PETSC_COMM_WORLD,0,"fu - discretization of function",-1,-1,-1,-1,&user.fu_viewer);CHKERRQ(ierr);

  /* create nonlinear multi-level solver */
  ierr = SNESCreate(PETSC_COMM_WORLD,&snes);CHKERRQ(ierr);
  ierr = SNESSetDM(snes,da);CHKERRQ(ierr);
  ierr = SNESSetFromOptions(snes);CHKERRQ(ierr);
  ierr = SNESSolve(snes,0,0);CHKERRQ(ierr);
  ierr = SNESDestroy(&snes);CHKERRQ(ierr);

  ierr = DMDestroy(&da);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&user.u_viewer);CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&user.fu_viewer);CHKERRQ(ierr);

  ierr = PetscFinalize();
  return 0;
}

#undef __FUNCT__  
#define __FUNCT__ "FormFunction"
/*
     This local function acts on the ghosted version of U (accessed via DMGetLocalVector())
     BUT the global, nonghosted version of FU

*/
PetscErrorCode FormFunction(DM da,Vec U,Vec FU)
{
  PetscErrorCode ierr;
  PetscInt       xs,xm,i,N;
  PetscScalar    *u,*fu,d,h;
  Vec            vu;

  PetscFunctionBegin;
  ierr = DMGetLocalVector(da,&vu);CHKERRQ(ierr);
  ierr = DMGlobalToLocalBegin(da,U,INSERT_VALUES,vu);CHKERRQ(ierr);
  ierr = DMGlobalToLocalEnd(da,U,INSERT_VALUES,vu);CHKERRQ(ierr);

  ierr = DMDAGetCorners(da,&xs,PETSC_NULL,PETSC_NULL,&xm,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
  ierr = DMDAGetInfo(da,0,&N,0,0,0,0,0,0,0,0,0,0,0);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(da,vu,&u);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(da,FU,&fu);CHKERRQ(ierr);
  d    = N-1.0;
  h    = 1.0/d;

  for (i=xs; i<xs+xm; i++) {
    if      (i == 0)   fu[0]   = 2.0*d*(u[0] - .25) + h*u[0]*u[0];
    else if (i == N-1) fu[N-1] = 2.0*d*u[N-1] + h*u[N-1]*u[N-1];
    else               fu[i]   = -(d*(u[i+1] - 2.0*u[i] + u[i-1]) - 2.0*h) + h*u[i]*u[i];
  } 

  ierr = DMDAVecRestoreArray(da,vu,&u);CHKERRQ(ierr);
  ierr = DMDAVecRestoreArray(da,FU,&fu);CHKERRQ(ierr);
  ierr = DMRestoreLocalVector(da,&vu);CHKERRQ(ierr);
  ierr = PetscLogFlops(9.0*N);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

