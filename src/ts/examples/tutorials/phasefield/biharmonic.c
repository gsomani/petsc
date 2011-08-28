
static char help[] = "Solves biharmonic equation in 1d.\n";

/*
  Solves the equation

    u_t = - kappa  \Delta \Delta u 
    Periodic boundary conditions

Evolve the biharmonic heat equation: 
---------------
./biharmonic -ts_monitor -snes_monitor   -pc_type lu  -draw_pause .1 -snes_converged_reason  -wait   -ts_type beuler  -da_refine 5

Evolve with the restriction that -1 <= u <= 1; i.e. as a variational inequality
---------------
./biharmonic -ts_monitor -snes_monitor   -pc_type lu  -draw_pause .1 -snes_converged_reason  -wait   -ts_type beuler   -da_refine 5 -vi 

Add a simple term to the right hand side so that the bulge grows/expands with time
---------------
./biharmonic -ts_monitor -snes_monitor  -pc_type lu  -draw_pause .1  -snes_converged_reason  -wait   -ts_type beuler   -da_refine 5 -vi -growth

   u_t =  kappa \Delta \Delta  6.*u*(u_x)^2 + (3*u^2 - 12) \Delta u
    -1 <= u <= 1 
    Periodic boundary conditions

Evolve the Cahn-Hillard equations: double well
---------------
./biharmonic -ts_monitor -snes_monitor   -pc_type lu  -draw_pause .1 -snes_converged_reason   -wait   -ts_type beuler    -da_refine 6 -vi  -kappa .00001 -ts_dt 5.96046e-06 -cahn-hillard

Evolve the Cahn-Hillard equations: double obstacle
---------------
./biharmonic -ts_monitor -snes_monitor  -pc_type lu  -draw_pause .1 -snes_converged_reason   -wait   -ts_type beuler    -da_refine 5 -vi  -kappa .00001 -ts_dt 5.96046e-06 -cahn-hillard -energy 2 -snes_ls_monitor   -vi 

Evolve the Cahn-Hillard equations: logarithmic
---------------
./biharmonic -ts_monitor -snes_monitor  -pc_type lu  --snes_converged_reason  -wait   -ts_type beuler    -da_refine 5 -vi  -kappa .00001 -ts_dt 5.96046e-06 -cahn-hillard -energy 3 -snes_ls_monitor -theta .00000001  -vi 


*/
#include <petscdmda.h>
#include <petscts.h>

extern PetscErrorCode FormFunction(TS,PetscReal,Vec,Vec,void*),FormInitialSolution(DM,Vec),MyMonitor(TS,PetscInt,PetscReal,Vec,void*);
typedef struct {PetscBool growth;PetscBool cahnhillard;PetscBool degenerate;PetscReal kappa;PetscInt energy;PetscReal tol;PetscReal theta,theta_c;} UserCtx;

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **argv)
{
  TS                     ts;                 /* nonlinear solver */
  Vec                    x,r;                  /* solution, residual vectors */
  Mat                    J;                    /* Jacobian matrix */
  PetscInt               steps,Mx,maxsteps = 10000000;
  PetscErrorCode         ierr;
  DM                     da;
  MatFDColoring          matfdcoloring;
  ISColoring             iscoloring;
  PetscReal              ftime,dt;
  PetscReal              vbounds[] = {-1.1,1.1};
  PetscBool              wait,vi = PETSC_FALSE;
  Vec                    ul,uh;
  UserCtx                ctx;

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Initialize program
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  PetscInitialize(&argc,&argv,(char *)0,help);
  ctx.kappa = 1.0;
  ierr = PetscOptionsGetReal(PETSC_NULL,"-kappa",&ctx.kappa,PETSC_NULL);CHKERRQ(ierr);
  ctx.growth = PETSC_FALSE;
  ierr = PetscOptionsGetBool(PETSC_NULL,"-growth",&ctx.growth,PETSC_NULL);CHKERRQ(ierr);
  ctx.degenerate = PETSC_FALSE;
  ierr = PetscOptionsGetBool(PETSC_NULL,"-degenerate",&ctx.degenerate,PETSC_NULL);CHKERRQ(ierr);
  ctx.cahnhillard = PETSC_FALSE;
  ierr = PetscOptionsGetBool(PETSC_NULL,"-cahn-hillard",&ctx.cahnhillard,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetBool(PETSC_NULL,"-vi",&vi,PETSC_NULL);CHKERRQ(ierr);
  ctx.energy = 1;
  ierr = PetscOptionsInt("-energy","type of energy (1=double well, 2=double obstacle, 3=logarithmic)","",ctx.energy,&ctx.energy,PETSC_NULL);CHKERRQ(ierr);
  ctx.tol = 1.0e-8;
  ierr = PetscOptionsGetReal(PETSC_NULL,"-tol",&ctx.tol,PETSC_NULL);CHKERRQ(ierr);
  ctx.theta = .001;
  ctx.theta_c = 1.0;
  ierr = PetscOptionsGetReal(PETSC_NULL,"-theta",&ctx.theta,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(PETSC_NULL,"-theta_c",&ctx.theta_c,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscViewerDrawSetBounds(PETSC_VIEWER_DRAW_(PETSC_COMM_WORLD),1,vbounds);CHKERRQ(ierr); 
  ierr = PetscViewerDrawResize(PETSC_VIEWER_DRAW_(PETSC_COMM_WORLD),1200,1000);CHKERRQ(ierr); 

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create distributed array (DMDA) to manage parallel grid and vectors
  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = DMDACreate1d(PETSC_COMM_WORLD, DMDA_BOUNDARY_PERIODIC, -10 ,1,2,PETSC_NULL,&da);CHKERRQ(ierr);
  ierr = DMDASetFieldName(da,0,"Biharmonic heat equation: u");CHKERRQ(ierr);
  ierr = DMDAGetInfo(da,0,&Mx,0,0,0,0,0,0,0,0,0,0,0);CHKERRQ(ierr);
  dt   = 1.0/(10.*ctx.kappa*Mx*Mx*Mx*Mx);

  /*  - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Extract global vectors from DMDA; then duplicate for remaining
     vectors that are the same types
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = DMCreateGlobalVector(da,&x);CHKERRQ(ierr);
  ierr = VecDuplicate(x,&r);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create timestepping solver context
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = TSCreate(PETSC_COMM_WORLD,&ts);CHKERRQ(ierr);
  ierr = TSSetDM(ts,da);CHKERRQ(ierr);
  ierr = TSSetProblemType(ts,TS_NONLINEAR);CHKERRQ(ierr);
  ierr = TSSetRHSFunction(ts,PETSC_NULL,FormFunction,&ctx);CHKERRQ(ierr);
  ierr = TSSetDuration(ts,maxsteps,.02);CHKERRQ(ierr);
  ierr = TSSetExactFinalTime(ts,PETSC_TRUE);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Create matrix data structure; set Jacobian evaluation routine

     Set Jacobian matrix data structure and default Jacobian evaluation
     routine. User can override with:
     -snes_mf : matrix-free Newton-Krylov method with no preconditioning
                (unless user explicitly sets preconditioner) 
     -snes_mf_operator : form preconditioning matrix as set by the user,
                         but use matrix-free approx for Jacobian-vector
                         products within Newton-Krylov method

     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = DMGetColoring(da,IS_COLORING_GLOBAL,MATAIJ,&iscoloring);CHKERRQ(ierr);
  ierr = DMGetMatrix(da,MATAIJ,&J);CHKERRQ(ierr);
  ierr = MatFDColoringCreate(J,iscoloring,&matfdcoloring);CHKERRQ(ierr);
  ierr = ISColoringDestroy(&iscoloring);CHKERRQ(ierr);
  ierr = MatFDColoringSetFunction(matfdcoloring,(PetscErrorCode (*)(void))FormFunction,&ctx);CHKERRQ(ierr);
  ierr = MatFDColoringSetFromOptions(matfdcoloring);CHKERRQ(ierr);
  ierr = TSSetRHSJacobian(ts,J,J,TSDefaultComputeJacobianColor,matfdcoloring);CHKERRQ(ierr);

  if (vi) {
    ierr = VecDuplicate(x,&ul);CHKERRQ(ierr);
    ierr = VecDuplicate(x,&uh);CHKERRQ(ierr);
    ierr = VecSet(ul,-1.0);CHKERRQ(ierr);
    ierr = VecSet(uh,1.0);CHKERRQ(ierr);
    ierr = TSVISetVariableBounds(ts,ul,uh);CHKERRQ(ierr);
  }

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Customize nonlinear solver
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = TSSetType(ts,TSBEULER);CHKERRQ(ierr);
 
  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set initial conditions
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = FormInitialSolution(da,x);CHKERRQ(ierr);
  ierr = TSSetInitialTimeStep(ts,0.0,dt);CHKERRQ(ierr);
  ierr = TSSetSolution(ts,x);CHKERRQ(ierr);

  ierr = TSMonitorSet(ts,MyMonitor,&ctx,PETSC_NULL);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Set runtime options
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = TSSetFromOptions(ts);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Solve nonlinear system
     - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  ierr = TSSolve(ts,x,&ftime);CHKERRQ(ierr);
  wait  = PETSC_FALSE;
  ierr = PetscOptionsGetBool(PETSC_NULL,"-wait",&wait,PETSC_NULL);CHKERRQ(ierr);
  if (wait) {
    ierr = PetscSleep(-1);CHKERRQ(ierr);
  }
  ierr = TSGetTimeStepNumber(ts,&steps);CHKERRQ(ierr);

  /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
     Free work space.  All PETSc objects should be destroyed when they
     are no longer needed.
   - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
  if (vi) {
    ierr = VecDestroy(&ul);CHKERRQ(ierr);
    ierr = VecDestroy(&uh);CHKERRQ(ierr);
  }
  ierr = MatDestroy(&J);CHKERRQ(ierr);
  ierr = MatFDColoringDestroy(&matfdcoloring);CHKERRQ(ierr);
  ierr = VecDestroy(&x);CHKERRQ(ierr);
  ierr = VecDestroy(&r);CHKERRQ(ierr);      
  ierr = TSDestroy(&ts);CHKERRQ(ierr);
  ierr = DMDestroy(&da);CHKERRQ(ierr);

  ierr = PetscFinalize();
  PetscFunctionReturn(0);
}
/* ------------------------------------------------------------------- */
#undef __FUNCT__
#define __FUNCT__ "FormFunction"
/* 
   FormFunction - Evaluates nonlinear function, F(x).

   Input Parameters:
.  ts - the TS context
.  X - input vector
.  ptr - optional user-defined context, as set by SNESSetFunction()

   Output Parameter:
.  F - function vector
 */
PetscErrorCode FormFunction(TS ts,PetscReal ftime,Vec X,Vec F,void *ptr)
{
  DM             da;
  PetscErrorCode ierr;
  PetscInt       i,Mx,xs,xm;
  PetscReal      hx,sx;
  PetscScalar    *x,*f,c,r,l;
  Vec            localX;
  UserCtx        *ctx = (UserCtx*)ptr;
  PetscReal      tol = ctx->tol, theta=ctx->theta,theta_c=ctx->theta_c;

  PetscFunctionBegin;
  ierr = TSGetDM(ts,&da);CHKERRQ(ierr);
  ierr = DMGetLocalVector(da,&localX);CHKERRQ(ierr);
  ierr = DMDAGetInfo(da,PETSC_IGNORE,&Mx,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,
                   PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);

  hx     = 1.0/(PetscReal)Mx; sx = 1.0/(hx*hx);

  /*
     Scatter ghost points to local vector,using the 2-step process
        DMGlobalToLocalBegin(),DMGlobalToLocalEnd().
     By placing code between these two statements, computations can be
     done while messages are in transition.
  */
  ierr = DMGlobalToLocalBegin(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);
  ierr = DMGlobalToLocalEnd(da,X,INSERT_VALUES,localX);CHKERRQ(ierr);

  /*
     Get pointers to vector data
  */
  ierr = DMDAVecGetArray(da,localX,&x);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(da,F,&f);CHKERRQ(ierr);

  /*
     Get local grid boundaries
  */
  ierr = DMDAGetCorners(da,&xs,PETSC_NULL,PETSC_NULL,&xm,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);

  /*
     Compute function over the locally owned part of the grid
  */
  for (i=xs; i<xs+xm; i++) {
    if (ctx->degenerate) {
      c = (1. - x[i]*x[i])*(x[i-1] + x[i+1] - 2.0*x[i])*sx;
      r = (1. - x[i+1]*x[i+1])*(x[i] + x[i+2] - 2.0*x[i+1])*sx;
      l = (1. - x[i-1]*x[i-1])*(x[i-2] + x[i] - 2.0*x[i-1])*sx; 
    } else {
      c = (x[i-1] + x[i+1] - 2.0*x[i])*sx;
      r = (x[i] + x[i+2] - 2.0*x[i+1])*sx;
      l = (x[i-2] + x[i] - 2.0*x[i-1])*sx;
    }
    f[i] = -ctx->kappa*(l + r - 2.0*c)*sx; 
    if (ctx->growth) {
      f[i] += 100000*((x[i] < .2) ?  0: sx*x[i]);
    }
    if (ctx->cahnhillard) {
      switch (ctx->energy) {
      case 1: // double well
      f[i] += 6.*.25*x[i]*(x[i+1] - x[i-1])*(x[i+1] - x[i-1])*sx + (3.*x[i]*x[i] - 1.)*(x[i-1] + x[i+1] - 2.0*x[i])*sx;
      break;
      case 2: // double obstacle
        f[i] += -(x[i-1] + x[i+1] - 2.0*x[i])*sx;
          break;
      case 3: // logarithmic
        f[i] += - theta_c*(x[i-1] + x[i+1] - 2.0*x[i])*sx;
        if (x[i] < -1.0 + 2.0*tol) {
          f[i] += 2.0*theta*(2.0*tol-1.0)/(16.0*(tol-tol*tol)*(tol-tol*tol))*.25*(x[i+1] - x[i-1])*(x[i+1] - x[i-1])*sx + (.25*theta/(tol-tol*tol))*(x[i-1] + x[i+1] - 2.0*x[i])*sx;
        } else if (x[i] > 1.0 - 2.0*tol) {
          f[i] += 2.0*theta*(-2.0*tol+1.0)/(16.0*(tol-tol*tol)*(tol-tol*tol))*.25*(x[i+1] - x[i-1])*(x[i+1] - x[i-1])*sx + (.25*theta/(tol-tol*tol))*(x[i-1] + x[i+1] - 2.0*x[i])*sx;
        } else {
          f[i] += 2.0*theta*x[i]/((1.0-x[i]*x[i])*(1.0-x[i]*x[i]))*.25*(x[i+1] - x[i-1])*(x[i+1] - x[i-1])*sx + (theta/(1.0-x[i]*x[i]))*(x[i-1] + x[i+1] - 2.0*x[i])*sx;
        }
        break;
      }
    }

  }

  /*
     Restore vectors
  */
  ierr = DMDAVecRestoreArray(da,localX,&x);CHKERRQ(ierr);
  ierr = DMDAVecRestoreArray(da,F,&f);CHKERRQ(ierr);
  ierr = DMRestoreLocalVector(da,&localX);CHKERRQ(ierr);
  PetscFunctionReturn(0); 
} 

/* ------------------------------------------------------------------- */
#undef __FUNCT__
#define __FUNCT__ "FormInitialSolution"
PetscErrorCode FormInitialSolution(DM da,Vec U)
{
  PetscErrorCode ierr;
  PetscInt       i,xs,xm,Mx;
  PetscScalar    *u,r;
  PetscReal      hx,x;

  PetscFunctionBegin;
  ierr = DMDAGetInfo(da,PETSC_IGNORE,&Mx,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,
                   PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);

  hx     = 1.0/(PetscReal)Mx;

  /*
     Get pointers to vector data
  */
  ierr = DMDAVecGetArray(da,U,&u);CHKERRQ(ierr);

  /*
     Get local grid boundaries
  */
  ierr = DMDAGetCorners(da,&xs,PETSC_NULL,PETSC_NULL,&xm,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);

  /*
     Compute function over the locally owned part of the grid
  */
  for (i=xs; i<xs+xm; i++) {
    x = i*hx;
    r = PetscSqrtScalar((x-.5)*(x-.5));
    if (r < .125) {
      u[i] = 1.0;
    } else {
      u[i] = -.5;
    }
  }

  /*
     Restore vectors
  */
  ierr = DMDAVecRestoreArray(da,U,&u);CHKERRQ(ierr);
  PetscFunctionReturn(0); 
} 

#undef __FUNCT__  
#define __FUNCT__ "MyMonitor"
/*
    This routine is not parallel
*/
PetscErrorCode  MyMonitor(TS ts,PetscInt step,PetscReal time,Vec U,void *ptr)
{
  UserCtx                   *ctx = (UserCtx*)ptr;
  PetscDrawLG               lg;
  PetscErrorCode            ierr;
  PetscScalar               *u,l,r,c;
  PetscInt                  Mx,i,xs,xm,cnt;
  PetscReal                 x,y,hx,pause,sx,len,max,xx[4],yy[4],yup,ydown,y2,len2;
  PetscDraw                 draw;
  Vec                       localU;
  DM                        da;
  int                       colors[] = {PETSC_DRAW_YELLOW,PETSC_DRAW_RED,PETSC_DRAW_BLUE,PETSC_DRAW_PLUM,PETSC_DRAW_BLACK};
  const char *const         legend[3][3] = {{"-kappa (\\grad u,\\grad u)","(1 - u^2)^2"},{"-kappa (\\grad u,\\grad u)","(1 - u^2)"},{"-kappa (\\grad u,\\grad u)","logarithmic"}};
  PetscDrawAxis             axis;
  static PetscDrawViewPorts *ports = 0;
  PetscReal                 tol = ctx->tol, theta=ctx->theta,theta_c=ctx->theta_c;


  PetscFunctionBegin;
  ierr = TSGetDM(ts,&da);CHKERRQ(ierr);
  ierr = DMGetLocalVector(da,&localU);CHKERRQ(ierr);
  ierr = DMDAGetInfo(da,PETSC_IGNORE,&Mx,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,
                        PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE,PETSC_IGNORE);CHKERRQ(ierr);
  ierr = DMDAGetCorners(da,&xs,PETSC_NULL,PETSC_NULL,&xm,PETSC_NULL,PETSC_NULL);CHKERRQ(ierr);
  hx     = 1.0/(PetscReal)Mx; sx = 1.0/(hx*hx); 
  ierr = DMGlobalToLocalBegin(da,U,INSERT_VALUES,localU);CHKERRQ(ierr);
  ierr = DMGlobalToLocalEnd(da,U,INSERT_VALUES,localU);CHKERRQ(ierr);
  ierr = DMDAVecGetArray(da,localU,&u);CHKERRQ(ierr);

  ierr = PetscViewerDrawGetDrawLG(PETSC_VIEWER_DRAW_(PETSC_COMM_WORLD),1,&lg);CHKERRQ(ierr);
  ierr = PetscDrawLGGetDraw(lg,&draw);CHKERRQ(ierr);
  ierr = PetscDrawCheckResizedWindow(draw);CHKERRQ(ierr);
  if (!ports) {
    ierr = PetscDrawViewPortsCreateRect(draw,1,3,&ports);CHKERRQ(ierr);
  }
  ierr = PetscDrawLGGetAxis(lg,&axis);CHKERRQ(ierr);
  ierr = PetscDrawLGReset(lg);CHKERRQ(ierr);

  xx[0] = 0.0; xx[1] = 1.0; cnt = 2;
  ierr = PetscOptionsGetRealArray(PETSC_NULL,"-zoom",xx,&cnt,PETSC_NULL);CHKERRQ(ierr);
  xs = xx[0]/hx; xm = (xx[1] - xx[0])/hx;

  /* 
      Plot the  energies 
  */
  ierr = PetscDrawLGSetDimension(lg,1 + (ctx->cahnhillard ? 1 : 0) + (ctx->energy == 3));CHKERRQ(ierr);
  ierr = PetscDrawLGSetColors(lg,colors+1);CHKERRQ(ierr);
  ierr = PetscDrawViewPortsSet(ports,2);CHKERRQ(ierr);
  x   = hx*xs;
  for (i=xs; i<xs+xm; i++) {
    xx[0] = xx[1]  = xx[2] = x;
    if (ctx->degenerate) {
      yy[0] = PetscRealPart(.25*(1. - u[i]*u[i])*ctx->kappa*(u[i-1] - u[i+1])*(u[i-1] - u[i+1])*sx);
    } else {
      yy[0] = PetscRealPart(.25*ctx->kappa*(u[i-1] - u[i+1])*(u[i-1] - u[i+1])*sx);
    }
    if (ctx->cahnhillard) {
      switch (ctx->energy) {
      case 1: // double well
        yy[1]   = .25*PetscRealPart((1. - u[i]*u[i])*(1. - u[i]*u[i]));
        break;
      case 2: // double obstacle 
        yy[1] = .5*PetscRealPart(1. - u[i]*u[i]);
        break;
      case 3:
        yy[1] = .5*theta_c*(1.0-u[i]*u[i]);
        if (u[i] < -1.0 + 2.0*tol) {
          yy[2] = -.5*theta*(2.0*tol*log(tol) + (1.0-u[i])*log((1-u[i])/2.0));
        } else if (u[i] > 1.0 - 2.0*tol) {
          yy[2] = -.5*theta*((1.0+u[i])*log((1.0+u[i])/2.0) + 2.0*tol*log(tol));
        } else {
          yy[2] = -.5*theta*((1.0+u[i])*log((1.0+u[i])/2.0) + (1.0-u[i])*log((1.0-u[i])/2.0));
        }
        break;
      }
    }
    ierr = PetscDrawLGAddPoint(lg,xx,yy);CHKERRQ(ierr);
    x   += hx;
  }
  ierr = PetscDrawGetPause(draw,&pause);CHKERRQ(ierr);
  ierr = PetscDrawSetPause(draw,0.0);CHKERRQ(ierr);
  ierr = PetscDrawAxisSetLabels(axis,"Energy","","");CHKERRQ(ierr);
  /*  ierr = PetscDrawLGSetLegend(lg,legend[ctx->energy-1]);CHKERRQ(ierr); */
  ierr = PetscDrawLGDraw(lg);CHKERRQ(ierr);

  /* 
      Plot the  forces
  */
  ierr = PetscDrawLGSetDimension(lg,1 + (ctx->cahnhillard ? 2 : 0) + (ctx->energy == 3));CHKERRQ(ierr);
  ierr = PetscDrawLGSetColors(lg,colors+1);CHKERRQ(ierr);
  ierr = PetscDrawViewPortsSet(ports,1);CHKERRQ(ierr);
  ierr = PetscDrawLGReset(lg);CHKERRQ(ierr);
  x   = xs*hx;;
  max = 0.;
  for (i=xs; i<xs+xm; i++) {
    xx[0] = xx[1] = xx[2] = xx[3] = x;
    if (ctx->degenerate) {
      c = (1. - u[i]*u[i])*(u[i-1] + u[i+1] - 2.0*u[i])*sx;
      r = (1. - u[i+1]*u[i+1])*(u[i] + u[i+2] - 2.0*u[i+1])*sx;
      l = (1. - u[i-1]*u[i-1])*(u[i-2] + u[i] - 2.0*u[i-1])*sx;
    } else {
      c = (u[i-1] + u[i+1] - 2.0*u[i])*sx;
      r = (u[i] + u[i+2] - 2.0*u[i+1])*sx;
      l = (u[i-2] + u[i] - 2.0*u[i-1])*sx;
    }
    yy[0] = PetscRealPart(-ctx->kappa*(l + r - 2.0*c)*sx);
    max   = PetscMax(max,PetscAbs(yy[0]));
    if (ctx->cahnhillard) {
      switch (ctx->energy) {
      case 1: // double well
        yy[1] = PetscRealPart(6.*.25*u[i]*(u[i+1] - u[i-1])*(u[i+1] - u[i-1])*sx + (3.*u[i]*u[i] - 1.)*(u[i-1] + u[i+1] - 2.0*u[i])*sx);
        break;
      case 2: // double obstacle
        yy[1] = -(u[i-1] + u[i+1] - 2.0*u[i])*sx;
        break;
      case 3: // logarithmic
        yy[1] = theta_c*(-(u[i-1] + u[i+1] - 2.0*u[i]))*sx;
        if (u[i] < -1.0 + 2.0*tol) {
          yy[2] = (2.0*theta*(2.0*tol-1.0)/(16.0*(tol-tol*tol)*(tol-tol*tol))*.25*(u[i+1] - u[i-1])*(u[i+1] - u[i-1])*sx + (.25*theta/(tol-tol*tol))*(u[i-1] + u[i+1] - 2.0*u[i])*sx);
        } else if (u[i] > 1.0 - 2.0*tol) {
          yy[2] = (2.0*theta*(-2.0*tol+1.0)/(16.0*(tol-tol*tol)*(tol-tol*tol))*.25*(u[i+1] - u[i-1])*(u[i+1] - u[i-1])*sx + ( .25*theta/(tol-tol*tol))*(u[i-1] + u[i+1] - 2.0*u[i])*sx);
        } else {
          yy[2] = (2.0*theta*u[i]/((1.0-u[i]*u[i])*(1.0-u[i]*u[i]))*.25*(u[i+1] - u[i-1])*(u[i+1] - u[i-1])*sx + (theta/(1.0-u[i]*u[i]))*(u[i-1] + u[i+1] - 2.0*u[i])*sx);
        }
        break;
      }
      if (ctx->energy != 3) {
        max   = PetscMax(max,PetscAbs(yy[1]));
        yy[2] = yy[0]+yy[1];
      } else {
        max   = PetscMax(max,PetscAbs(yy[1]+yy[2]));
        yy[3] = yy[0]+yy[1]+yy[2];
      }
    }
    ierr = PetscDrawLGAddPoint(lg,xx,yy);CHKERRQ(ierr);
    x   += hx;
    //if (max > 7200150000.0)
    //printf("max very big when i = %d\n",i);
  }
  ierr = PetscDrawAxisSetLabels(axis,"Right hand side","","");CHKERRQ(ierr);
  ierr = PetscDrawLGSetLegend(lg,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscDrawLGDraw(lg);CHKERRQ(ierr);

  /*
        Plot the solution
  */
  ierr = PetscDrawLGSetDimension(lg,1);CHKERRQ(ierr);
  ierr = PetscDrawViewPortsSet(ports,0);CHKERRQ(ierr);
  ierr = PetscDrawLGReset(lg);
  x = hx*xs;
  ierr = PetscDrawLGSetLimits(lg,x,x+(xm-1)*hx,-1.1,1.1);CHKERRQ(ierr);
  ierr = PetscDrawLGSetColors(lg,colors);CHKERRQ(ierr);
  for (i=xs; i<xs+xm; i++) {
    xx[0] = x;
    yy[0] = PetscRealPart(u[i]);
    ierr = PetscDrawLGAddPoint(lg,xx,yy);CHKERRQ(ierr);
    x   += hx;
  }
  ierr = PetscDrawAxisSetLabels(axis,"Solution","","");CHKERRQ(ierr);
  ierr = PetscDrawLGDraw(lg);CHKERRQ(ierr);

  /*
      Print the  forces as arrows on the solution
  */
  x = hx*xs;
  cnt = xm/60;
  cnt = (!cnt) ? 1 : cnt;

  for (i=xs; i<xs+xm; i += cnt) {
    y  = yup = ydown = PetscRealPart(u[i]);
    c = (u[i-1] + u[i+1] - 2.0*u[i])*sx;
    r = (u[i] + u[i+2] - 2.0*u[i+1])*sx;
    l = (u[i-2] + u[i] - 2.0*u[i-1])*sx;
    len  = -.5*PetscRealPart(ctx->kappa*(l + r - 2.0*c)*sx)/max;
    ierr = PetscDrawArrow(draw,x,y,x,y+len,PETSC_DRAW_RED);CHKERRQ(ierr);
    if (ctx->cahnhillard) {
      if (len < 0.) {
        ydown += len;
      } else {
        yup += len;
      }
      switch (ctx->energy) {
      case 1: // double well
        len = .5*PetscRealPart(6.*.25*u[i]*(u[i+1] - u[i-1])*(u[i+1] - u[i-1])*sx + (3.*u[i]*u[i] - 1.)*(u[i-1] + u[i+1] - 2.0*u[i])*sx)/max;
        break;
      case 2: // double obstacle
        len = -.5*(u[i-1] + u[i+1] - 2.0*u[i])*sx/max;
        break;
      case 3: // logarithmic
         len   = .5*theta_c*(-(u[i-1] + u[i+1] - 2.0*u[i])*sx/max);
         if (len < 0.) {
           ydown += len;
         } else {
           yup += len;
        }
        if (u[i] < -1.0 + 2.0*tol) {
          len2 = .5*(2.0*theta*(2.0*tol-1.0)/(16.0*(tol-tol*tol)*(tol-tol*tol))*.25*(u[i+1] - u[i-1])*(u[i+1] - u[i-1])*sx + (.25*theta/(tol-tol*tol))*(u[i-1] + u[i+1] - 2.0*u[i])*sx)/max;
        } else if (u[i] > 1.0 - 2.0*tol) {
          len2 = .5*(2.0*theta*(-2.0*tol+1.0)/(16.0*(tol-tol*tol)*(tol-tol*tol))*.25*(u[i+1] - u[i-1])*(u[i+1] - u[i-1])*sx + (.25*theta/(tol-tol*tol))*(u[i-1] + u[i+1] - 2.0*u[i])*sx)/max;
        } else {
          len2 = .5*(2.0*theta*u[i]/((1.0-u[i]*u[i])*(1.0-u[i]*u[i]))*.25*(u[i+1] - u[i-1])*(u[i+1] - u[i-1])*sx + (theta/(1.0-u[i]*u[i]))*(u[i-1] + u[i+1] - 2.0*u[i])*sx)/max;
        }
        y2 = len < 0 ? ydown : yup;
        ierr = PetscDrawArrow(draw,x,y2,x,y2+len2,PETSC_DRAW_PLUM);CHKERRQ(ierr);
        break;
      }
      ierr = PetscDrawArrow(draw,x,y,x,y+len,PETSC_DRAW_BLUE);CHKERRQ(ierr);
    }
    x   += cnt*hx; 
  }
  ierr = DMDAVecRestoreArray(da,localU,&x);CHKERRQ(ierr);
  ierr = DMRestoreLocalVector(da,&localU);CHKERRQ(ierr);
  ierr = PetscDrawStringSetSize(draw,.2,.2);CHKERRQ(ierr);
  ierr = PetscDrawFlush(draw);CHKERRQ(ierr);
  ierr = PetscDrawSetPause(draw,pause);CHKERRQ(ierr);
  ierr = PetscDrawPause(draw);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

