static char help[] = "Linear elasticity in 2d and 3d with finite elements.\n\
We solve the elasticity problem in a rectangular\n\
domain, using a parallel unstructured mesh (DMPLEX) to discretize it.\n\
This example supports automatic convergence estimation\n\
and eventually adaptivity.\n\n\n";

/*
  https://en.wikipedia.org/wiki/Linear_elasticity

  Converting elastic constants:
    lambda = E nu / ((1 + nu) (1 - 2 nu))
    mu     = E / (2 (1 + nu))
*/

#include <petscdmplex.h>
#include <petscsnes.h>
#include <petscds.h>
#include <petscbag.h>
#include <petscconvest.h>

typedef enum {SOL_VLAP_QUADRATIC, SOL_ELAS_QUADRATIC, SOL_VLAP_TRIG, SOL_ELAS_TRIG, SOL_ELAS_AXIAL_DISP, SOL_ELAS_UNIFORM_STRAIN, SOL_ELAS_GE, SOL_MASS_QUADRATIC, NUM_SOLUTION_TYPES} SolutionType;
const char *solutionTypes[NUM_SOLUTION_TYPES+1] = {"vlap_quad", "elas_quad", "vlap_trig", "elas_trig", "elas_axial_disp", "elas_uniform_strain", "elas_ge", "mass_quad", "unknown"};

typedef enum {DEFORM_NONE, DEFORM_SHEAR, DEFORM_STEP, NUM_DEFORM_TYPES} DeformType;
const char *deformTypes[NUM_DEFORM_TYPES+1] = {"none", "shear", "step", "unknown"};

typedef struct {
  PetscScalar mu;     /* shear modulus */
  PetscScalar lambda; /* Lame's first parameter */
} Parameter;

typedef struct {
  /* Domain and mesh definition */
  char         dmType[256]; /* DM type for the solve */
  DeformType   deform;      /* Domain deformation type */
  /* Problem definition */
  SolutionType solType;     /* Type of exact solution */
  PetscBag     bag;         /* Problem parameters */
  /* Solver definition */
  PetscBool    useNearNullspace; /* Use the rigid body modes as a near nullspace for AMG */
} AppCtx;

static PetscErrorCode zero(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscInt d;
  for (d = 0; d < dim; ++d) u[d] = 0.0;
  return 0;
}

static PetscErrorCode ge_shift(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  PetscInt d;
  u[0] = 0.1;
  for (d = 1; d < dim; ++d) u[d] = 0.0;
  return 0;
}

static PetscErrorCode quadratic_2d_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  u[0] = x[0]*x[0];
  u[1] = x[1]*x[1] - 2.0*x[0]*x[1];
  return 0;
}

static PetscErrorCode quadratic_3d_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  u[0] = x[0]*x[0];
  u[1] = x[1]*x[1] - 2.0*x[0]*x[1];
  u[2] = x[2]*x[2] - 2.0*x[1]*x[2];
  return 0;
}

/*
  u = x^2
  v = y^2 - 2xy
  Delta <u,v> - f = <2, 2> - <2, 2>

  u = x^2
  v = y^2 - 2xy
  w = z^2 - 2yz
  Delta <u,v,w> - f = <2, 2, 2> - <2, 2, 2>
*/
static void f0_vlap_quadratic_u(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                                const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                                const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                                PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  PetscInt d;
  for (d = 0; d < dim; ++d) f0[d] += 2.0;
}

/*
  u = x^2
  v = y^2 - 2xy
  \varepsilon = / 2x     -y    \
                \ -y   2y - 2x /
  Tr(\varepsilon) = div u = 2y
  div \sigma = \partial_i \lambda \delta_{ij} \varepsilon_{kk} + \partial_i 2\mu\varepsilon_{ij}
    = \lambda \partial_j (2y) + 2\mu < 2-1, 2 >
    = \lambda < 0, 2 > + \mu < 2, 4 >

  u = x^2
  v = y^2 - 2xy
  w = z^2 - 2yz
  \varepsilon = / 2x     -y       0   \
                | -y   2y - 2x   -z   |
                \  0     -z    2z - 2y/
  Tr(\varepsilon) = div u = 2z
  div \sigma = \partial_i \lambda \delta_{ij} \varepsilon_{kk} + \partial_i 2\mu\varepsilon_{ij}
    = \lambda \partial_j (2z) + 2\mu < 2-1, 2-1, 2 >
    = \lambda < 0, 0, 2 > + \mu < 2, 2, 4 >
*/
static void f0_elas_quadratic_u(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                                const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                                const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                                PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal mu     = 1.0;
  const PetscReal lambda = 1.0;
  PetscInt        d;

  for (d = 0; d < dim-1; ++d) f0[d] += 2.0*mu;
  f0[dim-1] += 2.0*lambda + 4.0*mu;
}

static void f0_mass_quadratic_u(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                                const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                                const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                                PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  if (dim == 2) {
    f0[0] -= x[0]*x[0];
    f0[1] -= x[1]*x[1] - 2.0*x[0]*x[1];
  } else {
    f0[0] -= x[0]*x[0];
    f0[1] -= x[1]*x[1] - 2.0*x[0]*x[1];
    f0[2] -= x[2]*x[2] - 2.0*x[1]*x[2];
  }
}

static PetscErrorCode trig_2d_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  u[0] = PetscSinReal(2.0*PETSC_PI*x[0]);
  u[1] = PetscSinReal(2.0*PETSC_PI*x[1]) - 2.0*x[0]*x[1];
  return 0;
}

static PetscErrorCode trig_3d_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  u[0] = PetscSinReal(2.0*PETSC_PI*x[0]);
  u[1] = PetscSinReal(2.0*PETSC_PI*x[1]) - 2.0*x[0]*x[1];
  u[2] = PetscSinReal(2.0*PETSC_PI*x[2]) - 2.0*x[1]*x[2];
  return 0;
}

/*
  u = sin(2 pi x)
  v = sin(2 pi y) - 2xy
  Delta <u,v> - f = <-4 pi^2 u, -4 pi^2 v> - <-4 pi^2 sin(2 pi x), -4 pi^2 sin(2 pi y)>

  u = sin(2 pi x)
  v = sin(2 pi y) - 2xy
  w = sin(2 pi z) - 2yz
  Delta <u,v,2> - f = <-4 pi^2 u, -4 pi^2 v, -4 pi^2 w> - <-4 pi^2 sin(2 pi x), -4 pi^2 sin(2 pi y), -4 pi^2 sin(2 pi z)>
*/
static void f0_vlap_trig_u(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                           const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                           const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                           PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  PetscInt d;
  for (d = 0; d < dim; ++d) f0[d] += -4.0*PetscSqr(PETSC_PI)*PetscSinReal(2.0*PETSC_PI*x[d]);
}

/*
  u = sin(2 pi x)
  v = sin(2 pi y) - 2xy
  \varepsilon = / 2 pi cos(2 pi x)             -y        \
                \      -y          2 pi cos(2 pi y) - 2x /
  Tr(\varepsilon) = div u = 2 pi (cos(2 pi x) + cos(2 pi y)) - 2 x
  div \sigma = \partial_i \lambda \delta_{ij} \varepsilon_{kk} + \partial_i 2\mu\varepsilon_{ij}
    = \lambda \partial_j 2 pi (cos(2 pi x) + cos(2 pi y)) + 2\mu < -4 pi^2 sin(2 pi x) - 1, -4 pi^2 sin(2 pi y) >
    = \lambda < -4 pi^2 sin(2 pi x) - 2, -4 pi^2 sin(2 pi y) > + \mu < -8 pi^2 sin(2 pi x) - 2, -8 pi^2 sin(2 pi y) >

  u = sin(2 pi x)
  v = sin(2 pi y) - 2xy
  w = sin(2 pi z) - 2yz
  \varepsilon = / 2 pi cos(2 pi x)            -y                     0         \
                |         -y       2 pi cos(2 pi y) - 2x            -z         |
                \          0                  -z         2 pi cos(2 pi z) - 2y /
  Tr(\varepsilon) = div u = 2 pi (cos(2 pi x) + cos(2 pi y) + cos(2 pi z)) - 2 x - 2 y
  div \sigma = \partial_i \lambda \delta_{ij} \varepsilon_{kk} + \partial_i 2\mu\varepsilon_{ij}
    = \lambda \partial_j (2 pi (cos(2 pi x) + cos(2 pi y) + cos(2 pi z)) - 2 x - 2 y) + 2\mu < -4 pi^2 sin(2 pi x) - 1, -4 pi^2 sin(2 pi y) - 1, -4 pi^2 sin(2 pi z) >
    = \lambda < -4 pi^2 sin(2 pi x) - 2, -4 pi^2 sin(2 pi y) - 2, -4 pi^2 sin(2 pi z) > + 2\mu < -4 pi^2 sin(2 pi x) - 1, -4 pi^2 sin(2 pi y) - 1, -4 pi^2 sin(2 pi z) >
*/
static void f0_elas_trig_u(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                           const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                           const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                           PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal mu     = 1.0;
  const PetscReal lambda = 1.0;
  const PetscReal fact   = 4.0*PetscSqr(PETSC_PI);
  PetscInt        d;

  for (d = 0; d < dim; ++d) f0[d] += -(2.0*mu + lambda) * fact*PetscSinReal(2.0*PETSC_PI*x[d]) - (d < dim-1 ? 2.0*(mu + lambda) : 0.0);
}

static PetscErrorCode axial_disp_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  const PetscReal mu     = 1.0;
  const PetscReal lambda = 1.0;
  const PetscReal N      = 1.0;
  PetscInt d;

  u[0] = (3.*lambda*lambda + 8.*lambda*mu + 4*mu*mu)/(4*mu*(3*lambda*lambda + 5.*lambda*mu + 2*mu*mu))*N*x[0];
  u[1] = -0.25*lambda/mu/(lambda+mu)*N*x[1];
  for (d = 2; d < dim; ++d) u[d] = 0.0;
  return 0;
}

/*
  We will pull/push on the right side of a block of linearly elastic material. The uniform traction conditions on the
  right side of the box will result in a uniform strain along x and y. The Neumann BC is given by

     n_i \sigma_{ij} = t_i

  u = (1/(2\mu) - 1) x
  v = -y
  f = 0
  t = <4\mu/\lambda (\lambda + \mu), 0>
  \varepsilon = / 1/(2\mu) - 1   0 \
                \ 0             -1 /
  Tr(\varepsilon) = div u = 1/(2\mu) - 2
  div \sigma = \partial_i \lambda \delta_{ij} \varepsilon_{kk} + \partial_i 2\mu\varepsilon_{ij}
    = \lambda \partial_j (1/(2\mu) - 2) + 2\mu < 0, 0 >
    = \lambda < 0, 0 > + \mu < 0, 0 > = 0
  NBC =  <1,0> . <4\mu/\lambda (\lambda + \mu), 0> = 4\mu/\lambda (\lambda + \mu)

  u = x - 1/2
  v = 0
  w = 0
  \varepsilon = / x  0  0 \
                | 0  0  0 |
                \ 0  0  0 /
  Tr(\varepsilon) = div u = x
  div \sigma = \partial_i \lambda \delta_{ij} \varepsilon_{kk} + \partial_i 2\mu\varepsilon_{ij}
    = \lambda \partial_j x + 2\mu < 1, 0, 0 >
    = \lambda < 1, 0, 0 > + \mu < 2, 0, 0 >
*/
static void f0_elas_axial_disp_bd_u(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                                    const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                                    const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                                    PetscReal t, const PetscReal x[], const PetscReal n[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscReal N = -1.0;

  f0[0] = N;
}

static PetscErrorCode uniform_strain_u(PetscInt dim, PetscReal time, const PetscReal x[], PetscInt Nc, PetscScalar *u, void *ctx)
{
  const PetscReal eps_xx = 0.1;
  const PetscReal eps_xy = 0.3;
  const PetscReal eps_yy = 0.25;
  PetscInt d;

  u[0] = eps_xx*x[0] + eps_xy*x[1];
  u[1] = eps_xy*x[0] + eps_yy*x[1];
  for (d = 2; d < dim; ++d) u[d] = 0.0;
  return 0;
}

static void f0_mass_u(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                      const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                      const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                      PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f0[])
{
  const PetscInt Nc = dim;
  PetscInt       c;

  for (c = 0; c < Nc; ++c) f0[c] = u[c];
}

static void f1_vlap_u(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                      const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                      const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                      PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f1[])
{
  const PetscInt Nc = dim;
  PetscInt       c, d;

  for (c = 0; c < Nc; ++c) for (d = 0; d < dim; ++d) f1[c*dim+d] += u_x[c*dim+d];
}

static void f1_elas_u(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                      const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                      const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                      PetscReal t, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar f1[])
{
  const PetscInt  Nc     = dim;
  const PetscReal mu     = 1.0;
  const PetscReal lambda = 1.0;
  PetscInt        c, d;

  for (c = 0; c < Nc; ++c) {
    for (d = 0; d < dim; ++d) {
      f1[c*dim+d] += mu*(u_x[c*dim+d] + u_x[d*dim+c]);
      f1[c*dim+c] += lambda*u_x[d*dim+d];
    }
  }
}

static void g0_mass_uu(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                       const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                       const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                       PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g0[])
{
  const PetscInt Nc = dim;
  PetscInt       c;

  for (c = 0; c < Nc; ++c) g0[c*Nc + c] = 1.0;
}

static void g3_vlap_uu(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                       const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                       const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                       PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g3[])
{
  const PetscInt Nc = dim;
  PetscInt       c, d;

  for (c = 0; c < Nc; ++c) {
    for (d = 0; d < dim; ++d) {
      g3[((c*Nc + c)*dim + d)*dim + d] = 1.0;
    }
  }
}

/*
  \partial_df \phi_fc g_{fc,gc,df,dg} \partial_dg \phi_gc

  \partial_df \phi_fc \lambda \delta_{fc,df} \sum_gc \partial_dg \phi_gc \delta_{gc,dg}
  = \partial_fc \phi_fc \sum_gc \partial_gc \phi_gc
*/
static void g3_elas_uu(PetscInt dim, PetscInt Nf, PetscInt NfAux,
                       const PetscInt uOff[], const PetscInt uOff_x[], const PetscScalar u[], const PetscScalar u_t[], const PetscScalar u_x[],
                       const PetscInt aOff[], const PetscInt aOff_x[], const PetscScalar a[], const PetscScalar a_t[], const PetscScalar a_x[],
                       PetscReal t, PetscReal u_tShift, const PetscReal x[], PetscInt numConstants, const PetscScalar constants[], PetscScalar g3[])
{
  const PetscInt  Nc     = dim;
  const PetscReal mu     = 1.0;
  const PetscReal lambda = 1.0;
  PetscInt        c, d;

  for (c = 0; c < Nc; ++c) {
    for (d = 0; d < dim; ++d) {
      g3[((c*Nc + c)*dim + d)*dim + d] += mu;
      g3[((c*Nc + d)*dim + d)*dim + c] += mu;
      g3[((c*Nc + d)*dim + c)*dim + d] += lambda;
    }
  }
}

static PetscErrorCode ProcessOptions(MPI_Comm comm, AppCtx *options)
{
  PetscInt       sol = 0, def = 0;
  PetscErrorCode ierr;

  PetscFunctionBeginUser;
  options->deform           = DEFORM_NONE;
  options->solType          = SOL_VLAP_QUADRATIC;
  options->useNearNullspace = PETSC_TRUE;
  ierr = PetscStrncpy(options->dmType, DMPLEX, 256);CHKERRQ(ierr);

  ierr = PetscOptionsBegin(comm, "", "Linear Elasticity Problem Options", "DMPLEX");CHKERRQ(ierr);
  ierr = PetscOptionsEList("-deform_type", "Type of domain deformation", "ex17.c", deformTypes, NUM_DEFORM_TYPES, deformTypes[options->deform], &def, NULL);CHKERRQ(ierr);
  options->deform = (DeformType) def;
  ierr = PetscOptionsEList("-sol_type", "Type of exact solution", "ex17.c", solutionTypes, NUM_SOLUTION_TYPES, solutionTypes[options->solType], &sol, NULL);CHKERRQ(ierr);
  options->solType = (SolutionType) sol;
  ierr = PetscOptionsBool("-near_nullspace", "Use the rigid body modes as an AMG near nullspace", "ex17.c", options->useNearNullspace, &options->useNearNullspace, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsFList("-dm_type", "Convert DMPlex to another format", "ex17.c", DMList, options->dmType, options->dmType, 256, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsEnd();CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode SetupParameters(MPI_Comm comm, AppCtx *ctx)
{
  PetscBag       bag;
  Parameter     *p;
  PetscErrorCode ierr;

  PetscFunctionBeginUser;
  /* setup PETSc parameter bag */
  ierr = PetscBagGetData(ctx->bag,(void**)&p);CHKERRQ(ierr);
  ierr = PetscBagSetName(ctx->bag,"par","Elastic Parameters");CHKERRQ(ierr);
  bag  = ctx->bag;
  ierr = PetscBagRegisterScalar(bag, &p->mu,     1.0, "mu",     "Shear Modulus, Pa");CHKERRQ(ierr);
  ierr = PetscBagRegisterScalar(bag, &p->lambda, 1.0, "lambda", "Lame's first parameter, Pa");CHKERRQ(ierr);
  ierr = PetscBagSetFromOptions(bag);CHKERRQ(ierr);
  {
    PetscViewer       viewer;
    PetscViewerFormat format;
    PetscBool         flg;

    ierr = PetscOptionsGetViewer(comm, NULL, NULL, "-param_view", &viewer, &format, &flg);CHKERRQ(ierr);
    if (flg) {
      ierr = PetscViewerPushFormat(viewer, format);CHKERRQ(ierr);
      ierr = PetscBagView(bag, viewer);CHKERRQ(ierr);
      ierr = PetscViewerFlush(viewer);CHKERRQ(ierr);
      ierr = PetscViewerPopFormat(viewer);CHKERRQ(ierr);
      ierr = PetscViewerDestroy(&viewer);CHKERRQ(ierr);
    }
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode DMPlexDistortGeometry(DM dm)
{
  DM             cdm;
  DMLabel        label;
  Vec            coordinates;
  PetscScalar   *coords;
  PetscReal      mid = 0.5;
  PetscInt       cdim, d, vStart, vEnd, v;
  PetscErrorCode ierr;

  PetscFunctionBeginUser;
  ierr = DMGetCoordinateDM(dm, &cdm);CHKERRQ(ierr);
  ierr = DMGetCoordinateDim(dm, &cdim);CHKERRQ(ierr);
  ierr = DMPlexGetDepthStratum(dm, 0, &vStart, &vEnd);CHKERRQ(ierr);
  ierr = DMGetLabel(dm, "marker", &label);CHKERRQ(ierr);
  ierr = DMGetCoordinatesLocal(dm, &coordinates);CHKERRQ(ierr);
  ierr = VecGetArrayWrite(coordinates, &coords);CHKERRQ(ierr);
  for (v = vStart; v < vEnd; ++v) {
    PetscScalar *pcoords, shift;
    PetscInt     val;

    ierr = DMLabelGetValue(label, v, &val);CHKERRQ(ierr);
    if (val >= 0) continue;
    ierr = DMPlexPointLocalRef(cdm, v, coords, &pcoords);CHKERRQ(ierr);
    shift = 0.2 * PetscAbsScalar(pcoords[0] - mid);
    shift = PetscRealPart(pcoords[0]) > mid ? shift : -shift;
    for (d = 1; d < cdim; ++d) pcoords[d] += shift;
  }
  ierr = VecRestoreArrayWrite(coordinates, &coords);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode CreateMesh(MPI_Comm comm, AppCtx *user, DM *dm)
{
  PetscErrorCode ierr;

  PetscFunctionBeginUser;
  ierr = DMCreate(comm, dm);CHKERRQ(ierr);
  ierr = DMSetType(*dm, DMPLEX);CHKERRQ(ierr);
  ierr = DMSetFromOptions(*dm);CHKERRQ(ierr);
  switch (user->deform) {
    case DEFORM_NONE:  break;
    case DEFORM_SHEAR: ierr = DMPlexShearGeometry(*dm, DM_X, NULL);CHKERRQ(ierr);break;
    case DEFORM_STEP:  ierr = DMPlexDistortGeometry(*dm);CHKERRQ(ierr);break;
    default: SETERRQ2(comm, PETSC_ERR_ARG_OUTOFRANGE, "Invalid deformation type: %s (%D)", deformTypes[PetscMin(user->deform, NUM_DEFORM_TYPES)], user->deform);
  }
  ierr = DMSetApplicationContext(*dm, user);CHKERRQ(ierr);
  ierr = DMViewFromOptions(*dm, NULL, "-dm_view");CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

static PetscErrorCode SetupPrimalProblem(DM dm, AppCtx *user)
{
  PetscErrorCode (*exact)(PetscInt, PetscReal, const PetscReal[], PetscInt, PetscScalar *, void *);
  Parameter       *param;
  PetscDS          ds;
  PetscWeakForm    wf;
  DMLabel          label;
  PetscInt         id, bd;
  PetscInt         dim;
  PetscErrorCode   ierr;

  PetscFunctionBeginUser;
  ierr = DMGetDS(dm, &ds);CHKERRQ(ierr);
  ierr = PetscDSGetWeakForm(ds, &wf);CHKERRQ(ierr);
  ierr = PetscDSGetSpatialDimension(ds, &dim);CHKERRQ(ierr);
  ierr = PetscBagGetData(user->bag, (void **) &param);CHKERRQ(ierr);
  switch (user->solType) {
  case SOL_MASS_QUADRATIC:
    ierr = PetscDSSetResidual(ds, 0, f0_mass_u, NULL);CHKERRQ(ierr);
    ierr = PetscDSSetJacobian(ds, 0, 0, g0_mass_uu, NULL, NULL, NULL);CHKERRQ(ierr);
    ierr = PetscWeakFormSetIndexResidual(wf, NULL, 0, 0,  0, 1, f0_mass_quadratic_u, 0, NULL);CHKERRQ(ierr);
    switch (dim) {
    case 2: exact = quadratic_2d_u;break;
    case 3: exact = quadratic_3d_u;break;
    default: SETERRQ1(PetscObjectComm((PetscObject) ds), PETSC_ERR_ARG_WRONG, "Invalid dimension: %D", dim);
    }
    break;
  case SOL_VLAP_QUADRATIC:
    ierr = PetscDSSetResidual(ds, 0, f0_vlap_quadratic_u, f1_vlap_u);CHKERRQ(ierr);
    ierr = PetscDSSetJacobian(ds, 0, 0, NULL, NULL, NULL, g3_vlap_uu);CHKERRQ(ierr);
    switch (dim) {
    case 2: exact = quadratic_2d_u;break;
    case 3: exact = quadratic_3d_u;break;
    default: SETERRQ1(PetscObjectComm((PetscObject) ds), PETSC_ERR_ARG_WRONG, "Invalid dimension: %D", dim);
    }
    break;
  case SOL_ELAS_QUADRATIC:
    ierr = PetscDSSetResidual(ds, 0, f0_elas_quadratic_u, f1_elas_u);CHKERRQ(ierr);
    ierr = PetscDSSetJacobian(ds, 0, 0, NULL, NULL, NULL, g3_elas_uu);CHKERRQ(ierr);
    switch (dim) {
    case 2: exact = quadratic_2d_u;break;
    case 3: exact = quadratic_3d_u;break;
    default: SETERRQ1(PetscObjectComm((PetscObject) ds), PETSC_ERR_ARG_WRONG, "Invalid dimension: %D", dim);
    }
    break;
  case SOL_VLAP_TRIG:
    ierr = PetscDSSetResidual(ds, 0, f0_vlap_trig_u, f1_vlap_u);CHKERRQ(ierr);
    ierr = PetscDSSetJacobian(ds, 0, 0, NULL, NULL, NULL, g3_vlap_uu);CHKERRQ(ierr);
    switch (dim) {
    case 2: exact = trig_2d_u;break;
    case 3: exact = trig_3d_u;break;
    default: SETERRQ1(PetscObjectComm((PetscObject) ds), PETSC_ERR_ARG_WRONG, "Invalid dimension: %D", dim);
    }
    break;
  case SOL_ELAS_TRIG:
    ierr = PetscDSSetResidual(ds, 0, f0_elas_trig_u, f1_elas_u);CHKERRQ(ierr);
    ierr = PetscDSSetJacobian(ds, 0, 0, NULL, NULL, NULL, g3_elas_uu);CHKERRQ(ierr);
    switch (dim) {
    case 2: exact = trig_2d_u;break;
    case 3: exact = trig_3d_u;break;
    default: SETERRQ1(PetscObjectComm((PetscObject) ds), PETSC_ERR_ARG_WRONG, "Invalid dimension: %D", dim);
    }
    break;
  case SOL_ELAS_AXIAL_DISP:
    ierr = PetscDSSetResidual(ds, 0, NULL, f1_elas_u);CHKERRQ(ierr);
    ierr = PetscDSSetJacobian(ds, 0, 0, NULL, NULL, NULL, g3_elas_uu);CHKERRQ(ierr);
    id   = dim == 3 ? 5 : 2;
    ierr = DMGetLabel(dm, "marker", &label);CHKERRQ(ierr);
    ierr = DMAddBoundary(dm, DM_BC_NATURAL, "right", label, 1, &id, 0, 0, NULL, (void (*)(void)) NULL, NULL, user, &bd);CHKERRQ(ierr);
    ierr = PetscDSGetBoundary(ds, bd, &wf, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);CHKERRQ(ierr);
    ierr = PetscWeakFormSetIndexBdResidual(wf, label, id, 0, 0, 0, f0_elas_axial_disp_bd_u, 0, NULL);CHKERRQ(ierr);
    exact = axial_disp_u;
    break;
  case SOL_ELAS_UNIFORM_STRAIN:
    ierr = PetscDSSetResidual(ds, 0, NULL, f1_elas_u);CHKERRQ(ierr);
    ierr = PetscDSSetJacobian(ds, 0, 0, NULL, NULL, NULL, g3_elas_uu);CHKERRQ(ierr);
    exact = uniform_strain_u;
    break;
  case SOL_ELAS_GE:
    ierr = PetscDSSetResidual(ds, 0, NULL, f1_elas_u);CHKERRQ(ierr);
    ierr = PetscDSSetJacobian(ds, 0, 0, NULL, NULL, NULL, g3_elas_uu);CHKERRQ(ierr);
    exact = zero; /* No exact solution available */
    break;
  default: SETERRQ2(PetscObjectComm((PetscObject) ds), PETSC_ERR_ARG_WRONG, "Invalid solution type: %s (%D)", solutionTypes[PetscMin(user->solType, NUM_SOLUTION_TYPES)], user->solType);
  }
  ierr = PetscDSSetExactSolution(ds, 0, exact, user);CHKERRQ(ierr);
  ierr = DMGetLabel(dm, "marker", &label);CHKERRQ(ierr);
  if (user->solType == SOL_ELAS_AXIAL_DISP) {
    PetscInt cmp;

    id   = dim == 3 ? 6 : 4;
    cmp  = 0;
    ierr = DMAddBoundary(dm,   DM_BC_ESSENTIAL, "left",   label, 1, &id, 0, 1, &cmp, (void (*)(void)) zero, NULL, user, NULL);CHKERRQ(ierr);
    cmp  = dim == 3 ? 2 : 1;
    id   = dim == 3 ? 1 : 1;
    ierr = DMAddBoundary(dm,   DM_BC_ESSENTIAL, "bottom", label, 1, &id, 0, 1, &cmp, (void (*)(void)) zero, NULL, user, NULL);CHKERRQ(ierr);
    if (dim == 3) {
      cmp  = 1;
      id   = 3;
      ierr = DMAddBoundary(dm, DM_BC_ESSENTIAL, "front",  label, 1, &id, 0, 1, &cmp, (void (*)(void)) zero, NULL, user, NULL);CHKERRQ(ierr);
    }
  } else if (user->solType == SOL_ELAS_GE) {
    PetscInt cmp;

    id   = dim == 3 ? 6 : 4;
    ierr = DMAddBoundary(dm,   DM_BC_ESSENTIAL, "left",   label, 1, &id, 0, 0, NULL, (void (*)(void)) zero, NULL, user, NULL);CHKERRQ(ierr);
    id   = dim == 3 ? 5 : 2;
    cmp  = 0;
    ierr = DMAddBoundary(dm,   DM_BC_ESSENTIAL, "right",  label, 1, &id, 0, 1, &cmp, (void (*)(void)) ge_shift, NULL, user, NULL);CHKERRQ(ierr);
  } else {
    id = 1;
    ierr = DMAddBoundary(dm, DM_BC_ESSENTIAL, "wall", label, 1, &id, 0, 0, NULL, (void (*)(void)) exact, NULL, user, NULL);CHKERRQ(ierr);
  }
  /* Setup constants */
  {
    PetscScalar constants[2];

    constants[0] = param->mu;     /* shear modulus, Pa */
    constants[1] = param->lambda; /* Lame's first parameter, Pa */
    ierr = PetscDSSetConstants(ds, 2, constants);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

static PetscErrorCode CreateElasticityNullSpace(DM dm, PetscInt origField, PetscInt field, MatNullSpace *nullspace)
{
  PetscErrorCode ierr;

  PetscFunctionBegin;
  ierr = DMPlexCreateRigidBody(dm, origField, nullspace);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

PetscErrorCode SetupFE(DM dm, const char name[], PetscErrorCode (*setup)(DM, AppCtx *), void *ctx)
{
  AppCtx        *user = (AppCtx *) ctx;
  DM             cdm  = dm;
  PetscFE        fe;
  char           prefix[PETSC_MAX_PATH_LEN];
  DMPolytopeType ct;
  PetscBool      simplex;
  PetscInt       dim, cStart;
  PetscErrorCode ierr;

  PetscFunctionBegin;
  /* Create finite element */
  ierr = DMGetDimension(dm, &dim);CHKERRQ(ierr);
  ierr = DMPlexGetHeightStratum(dm, 0, &cStart, NULL);CHKERRQ(ierr);
  ierr = DMPlexGetCellType(dm, cStart, &ct);CHKERRQ(ierr);
  simplex = DMPolytopeTypeGetNumVertices(ct) == DMPolytopeTypeGetDim(ct)+1 ? PETSC_TRUE : PETSC_FALSE;
  ierr = PetscSNPrintf(prefix, PETSC_MAX_PATH_LEN, "%s_", name);CHKERRQ(ierr);
  ierr = PetscFECreateDefault(PetscObjectComm((PetscObject) dm), dim, dim, simplex, name ? prefix : NULL, -1, &fe);CHKERRQ(ierr);
  ierr = PetscObjectSetName((PetscObject) fe, name);CHKERRQ(ierr);
  /* Set discretization and boundary conditions for each mesh */
  ierr = DMSetField(dm, 0, NULL, (PetscObject) fe);CHKERRQ(ierr);
  ierr = DMCreateDS(dm);CHKERRQ(ierr);
  ierr = (*setup)(dm, user);CHKERRQ(ierr);
  while (cdm) {
    ierr = DMCopyDisc(dm, cdm);CHKERRQ(ierr);
    if (user->useNearNullspace) {ierr = DMSetNearNullSpaceConstructor(cdm, 0, CreateElasticityNullSpace);CHKERRQ(ierr);}
    /* TODO: Check whether the boundary of coarse meshes is marked */
    ierr = DMGetCoarseDM(cdm, &cdm);CHKERRQ(ierr);
  }
  ierr = PetscFEDestroy(&fe);CHKERRQ(ierr);
  PetscFunctionReturn(0);
}

int main(int argc, char **argv)
{
  DM             dm;   /* Problem specification */
  SNES           snes; /* Nonlinear solver */
  Vec            u;    /* Solutions */
  AppCtx         user; /* User-defined work context */
  PetscErrorCode ierr;

  ierr = PetscInitialize(&argc, &argv, NULL,help);if (ierr) return ierr;
  ierr = ProcessOptions(PETSC_COMM_WORLD, &user);CHKERRQ(ierr);
  ierr = PetscBagCreate(PETSC_COMM_SELF, sizeof(Parameter), &user.bag);CHKERRQ(ierr);
  ierr = SetupParameters(PETSC_COMM_WORLD, &user);CHKERRQ(ierr);
  /* Primal system */
  ierr = SNESCreate(PETSC_COMM_WORLD, &snes);CHKERRQ(ierr);
  ierr = CreateMesh(PETSC_COMM_WORLD, &user, &dm);CHKERRQ(ierr);
  ierr = SNESSetDM(snes, dm);CHKERRQ(ierr);
  ierr = SetupFE(dm, "displacement", SetupPrimalProblem, &user);CHKERRQ(ierr);
  ierr = DMCreateGlobalVector(dm, &u);CHKERRQ(ierr);
  ierr = VecSet(u, 0.0);CHKERRQ(ierr);
  ierr = PetscObjectSetName((PetscObject) u, "displacement");CHKERRQ(ierr);
  ierr = DMPlexSetSNESLocalFEM(dm, &user, &user, &user);CHKERRQ(ierr);
  ierr = SNESSetFromOptions(snes);CHKERRQ(ierr);
  ierr = DMSNESCheckFromOptions(snes, u);CHKERRQ(ierr);
  ierr = SNESSolve(snes, NULL, u);CHKERRQ(ierr);
  ierr = SNESGetSolution(snes, &u);CHKERRQ(ierr);
  ierr = VecViewFromOptions(u, NULL, "-displacement_view");CHKERRQ(ierr);
  /* Cleanup */
  ierr = VecDestroy(&u);CHKERRQ(ierr);
  ierr = SNESDestroy(&snes);CHKERRQ(ierr);
  ierr = DMDestroy(&dm);CHKERRQ(ierr);
  ierr = PetscBagDestroy(&user.bag);CHKERRQ(ierr);
  ierr = PetscFinalize();
  return ierr;
}

/*TEST

  testset:
    args: -dm_plex_box_faces 1,1,1

    test:
      suffix: 2d_p1_quad_vlap
      requires: triangle
      args: -displacement_petscspace_degree 1 -dm_refine 2 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_p2_quad_vlap
      requires: triangle
      args: -displacement_petscspace_degree 2 -dm_refine 2 -dmsnes_check .0001
    test:
      suffix: 2d_p3_quad_vlap
      requires: triangle
      args: -displacement_petscspace_degree 3 -dm_refine 2 -dmsnes_check .0001
    test:
      suffix: 2d_q1_quad_vlap
      args: -dm_plex_simplex 0 -displacement_petscspace_degree 1 -dm_refine 2 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_q2_quad_vlap
      args: -dm_plex_simplex 0 -displacement_petscspace_degree 2 -dm_refine 2 -dmsnes_check .0001
    test:
      suffix: 2d_q3_quad_vlap
      requires: !single
      args: -dm_plex_simplex 0 -displacement_petscspace_degree 3 -dm_refine 2 -dmsnes_check .0001
    test:
      suffix: 2d_p1_quad_elas
      requires: triangle
      args: -sol_type elas_quad -displacement_petscspace_degree 1 -dm_refine 2 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_p2_quad_elas
      requires: triangle
      args: -sol_type elas_quad -displacement_petscspace_degree 2 -dmsnes_check .0001
    test:
      suffix: 2d_p3_quad_elas
      requires: triangle
      args: -sol_type elas_quad -displacement_petscspace_degree 3 -dmsnes_check .0001
    test:
      suffix: 2d_q1_quad_elas
      args: -sol_type elas_quad -dm_plex_simplex 0 -displacement_petscspace_degree 1 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_q1_quad_elas_shear
      args: -sol_type elas_quad -dm_plex_simplex 0 -deform_type shear -displacement_petscspace_degree 1 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_q2_quad_elas
      args: -sol_type elas_quad -dm_plex_simplex 0 -displacement_petscspace_degree 2 -dmsnes_check .0001
    test:
      suffix: 2d_q2_quad_elas_shear
      args: -sol_type elas_quad -dm_plex_simplex 0 -deform_type shear -displacement_petscspace_degree 2 -dmsnes_check
    test:
      suffix: 2d_q3_quad_elas
      args: -sol_type elas_quad -dm_plex_simplex 0 -displacement_petscspace_degree 3 -dmsnes_check .0001
    test:
      suffix: 2d_q3_quad_elas_shear
      requires: !single
      args: -sol_type elas_quad -dm_plex_simplex 0 -deform_type shear -displacement_petscspace_degree 3 -dmsnes_check

    test:
      suffix: 3d_p1_quad_vlap
      requires: ctetgen
      args: -dm_plex_dim 3 -dm_refine 1 -displacement_petscspace_degree 1 -convest_num_refine 2 -snes_convergence_estimate
    test:
      suffix: 3d_p2_quad_vlap
      requires: ctetgen
      args: -dm_plex_dim 3 -displacement_petscspace_degree 2 -dm_refine 1 -dmsnes_check .0001
    test:
      suffix: 3d_p3_quad_vlap
      requires: ctetgen
      args: -dm_plex_dim 3 -displacement_petscspace_degree 3 -dm_refine 0 -dmsnes_check .0001
    test:
      suffix: 3d_q1_quad_vlap
      args: -dm_plex_dim 3 -dm_plex_box_faces 2,2,2 -dm_plex_simplex 0 -displacement_petscspace_degree 1 -convest_num_refine 2 -snes_convergence_estimate
    test:
      suffix: 3d_q2_quad_vlap
      args: -dm_plex_dim 3 -dm_plex_simplex 0 -displacement_petscspace_degree 2 -dm_refine 1 -dmsnes_check .0001
    test:
      suffix: 3d_q3_quad_vlap
      args: -dm_plex_dim 3 -dm_plex_simplex 0 -displacement_petscspace_degree 3 -dm_refine 0 -dmsnes_check .0001
    test:
      suffix: 3d_p1_quad_elas
      requires: ctetgen
      args: -sol_type elas_quad -dm_plex_dim 3 -dm_refine 1 -displacement_petscspace_degree 1 -convest_num_refine 2 -snes_convergence_estimate
    test:
      suffix: 3d_p2_quad_elas
      requires: ctetgen
      args: -sol_type elas_quad -dm_plex_dim 3 -displacement_petscspace_degree 2 -dm_refine 1 -dmsnes_check .0001
    test:
      suffix: 3d_p3_quad_elas
      requires: ctetgen
      args: -sol_type elas_quad -dm_plex_dim 3 -displacement_petscspace_degree 3 -dm_refine 0 -dmsnes_check .0001
    test:
      suffix: 3d_q1_quad_elas
      args: -sol_type elas_quad -dm_plex_dim 3 -dm_plex_box_faces 2,2,2 -dm_plex_simplex 0 -displacement_petscspace_degree 1 -convest_num_refine 2 -snes_convergence_estimate
    test:
      suffix: 3d_q2_quad_elas
      args: -sol_type elas_quad -dm_plex_dim 3 -dm_plex_simplex 0 -displacement_petscspace_degree 2 -dm_refine 1 -dmsnes_check .0001
    test:
      suffix: 3d_q3_quad_elas
      requires: !single
      args: -sol_type elas_quad -dm_plex_dim 3 -dm_plex_simplex 0 -displacement_petscspace_degree 3 -dm_refine 0 -dmsnes_check .0001

    test:
      suffix: 2d_p1_trig_vlap
      requires: triangle
      args: -sol_type vlap_trig -displacement_petscspace_degree 1 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_p2_trig_vlap
      requires: triangle
      args: -sol_type vlap_trig -displacement_petscspace_degree 2 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_p3_trig_vlap
      requires: triangle
      args: -sol_type vlap_trig -displacement_petscspace_degree 3 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_q1_trig_vlap
      args: -sol_type vlap_trig -dm_plex_simplex 0 -displacement_petscspace_degree 1 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_q2_trig_vlap
      args: -sol_type vlap_trig -dm_plex_simplex 0 -displacement_petscspace_degree 2 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_q3_trig_vlap
      args: -sol_type vlap_trig -dm_plex_simplex 0 -displacement_petscspace_degree 3 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_p1_trig_elas
      requires: triangle
      args: -sol_type elas_trig -displacement_petscspace_degree 1 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_p2_trig_elas
      requires: triangle
      args: -sol_type elas_trig -displacement_petscspace_degree 2 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_p3_trig_elas
      requires: triangle
      args: -sol_type elas_trig -displacement_petscspace_degree 3 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_q1_trig_elas
      args: -sol_type elas_trig -dm_plex_simplex 0 -displacement_petscspace_degree 1 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_q1_trig_elas_shear
      args: -sol_type elas_trig -dm_plex_simplex 0 -deform_type shear -displacement_petscspace_degree 1 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_q2_trig_elas
      args: -sol_type elas_trig -dm_plex_simplex 0 -displacement_petscspace_degree 2 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_q2_trig_elas_shear
      args: -sol_type elas_trig -dm_plex_simplex 0 -deform_type shear -displacement_petscspace_degree 2 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_q3_trig_elas
      args: -sol_type elas_trig -dm_plex_simplex 0 -displacement_petscspace_degree 3 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate
    test:
      suffix: 2d_q3_trig_elas_shear
      args: -sol_type elas_trig -dm_plex_simplex 0 -deform_type shear -displacement_petscspace_degree 3 -dm_refine 1 -convest_num_refine 3 -snes_convergence_estimate

    test:
      suffix: 3d_p1_trig_vlap
      requires: ctetgen
      args: -sol_type vlap_trig -dm_plex_dim 3 -dm_refine 1 -displacement_petscspace_degree 1 -convest_num_refine 2 -snes_convergence_estimate
    test:
      suffix: 3d_p2_trig_vlap
      requires: ctetgen
      args: -sol_type vlap_trig -dm_plex_dim 3 -displacement_petscspace_degree 2 -dm_refine 0 -convest_num_refine 1 -snes_convergence_estimate
    test:
      suffix: 3d_p3_trig_vlap
      requires: ctetgen
      args: -sol_type vlap_trig -dm_plex_dim 3 -displacement_petscspace_degree 3 -dm_refine 0 -convest_num_refine 1 -snes_convergence_estimate
    test:
      suffix: 3d_q1_trig_vlap
      args: -sol_type vlap_trig -dm_plex_dim 3 -dm_plex_box_faces 2,2,2 -dm_plex_simplex 0 -displacement_petscspace_degree 1 -convest_num_refine 2 -snes_convergence_estimate
    test:
      suffix: 3d_q2_trig_vlap
      args: -sol_type vlap_trig -dm_plex_dim 3 -dm_plex_simplex 0 -displacement_petscspace_degree 2 -dm_refine 0 -convest_num_refine 1 -snes_convergence_estimate
    test:
      suffix: 3d_q3_trig_vlap
      requires: !__float128
      args: -sol_type vlap_trig -dm_plex_dim 3 -dm_plex_simplex 0 -displacement_petscspace_degree 3 -dm_refine 0 -convest_num_refine 1 -snes_convergence_estimate
    test:
      suffix: 3d_p1_trig_elas
      requires: ctetgen
      args: -sol_type elas_trig -dm_plex_dim 3 -dm_refine 1 -displacement_petscspace_degree 1 -convest_num_refine 2 -snes_convergence_estimate
    test:
      suffix: 3d_p2_trig_elas
      requires: ctetgen
      args: -sol_type elas_trig -dm_plex_dim 3 -displacement_petscspace_degree 2 -dm_refine 0 -convest_num_refine 1 -snes_convergence_estimate
    test:
      suffix: 3d_p3_trig_elas
      requires: ctetgen
      args: -sol_type elas_trig -dm_plex_dim 3 -displacement_petscspace_degree 3 -dm_refine 0 -convest_num_refine 1 -snes_convergence_estimate
    test:
      suffix: 3d_q1_trig_elas
      args: -sol_type elas_trig -dm_plex_dim 3 -dm_plex_box_faces 2,2,2 -dm_plex_simplex 0 -displacement_petscspace_degree 1 -convest_num_refine 2 -snes_convergence_estimate
    test:
      suffix: 3d_q2_trig_elas
      args: -sol_type elas_trig -dm_plex_dim 3 -dm_plex_simplex 0 -displacement_petscspace_degree 2 -dm_refine 0 -convest_num_refine 1 -snes_convergence_estimate
    test:
      suffix: 3d_q3_trig_elas
      requires: !__float128
      args: -sol_type elas_trig -dm_plex_dim 3 -dm_plex_simplex 0 -displacement_petscspace_degree 3 -dm_refine 0 -convest_num_refine 1 -snes_convergence_estimate

    test:
      suffix: 2d_p1_axial_elas
      requires: triangle
      args: -sol_type elas_axial_disp -displacement_petscspace_degree 1 -dm_plex_separate_marker -dm_refine 2 -dmsnes_check .0001 -pc_type lu
    test:
      suffix: 2d_p2_axial_elas
      requires: triangle
      args: -sol_type elas_axial_disp -displacement_petscspace_degree 2 -dm_plex_separate_marker -dmsnes_check .0001 -pc_type lu
    test:
      suffix: 2d_p3_axial_elas
      requires: triangle
      args: -sol_type elas_axial_disp -displacement_petscspace_degree 3 -dm_plex_separate_marker -dmsnes_check .0001 -pc_type lu
    test:
      suffix: 2d_q1_axial_elas
      args: -sol_type elas_axial_disp -dm_plex_simplex 0 -displacement_petscspace_degree 1 -dm_plex_separate_marker -dm_refine 1 -dmsnes_check   .0001 -pc_type lu
    test:
      suffix: 2d_q2_axial_elas
      args: -sol_type elas_axial_disp -dm_plex_simplex 0 -displacement_petscspace_degree 2 -dm_plex_separate_marker -dmsnes_check .0001 -pc_type   lu
    test:
      suffix: 2d_q3_axial_elas
      args: -sol_type elas_axial_disp -dm_plex_simplex 0 -displacement_petscspace_degree 3 -dm_plex_separate_marker -dmsnes_check .0001 -pc_type   lu

    test:
      suffix: 2d_p1_uniform_elas
      requires: triangle
      args: -sol_type elas_uniform_strain -displacement_petscspace_degree 1 -dm_refine 2 -dmsnes_check .0001 -pc_type lu
    test:
      suffix: 2d_p2_uniform_elas
      requires: triangle
      args: -sol_type elas_uniform_strain -displacement_petscspace_degree 2 -dm_refine 2 -dmsnes_check .0001 -pc_type lu
    test:
      suffix: 2d_p3_uniform_elas
      requires: triangle
      args: -sol_type elas_uniform_strain -displacement_petscspace_degree 3 -dm_refine 2 -dmsnes_check .0001 -pc_type lu
    test:
      suffix: 2d_q1_uniform_elas
      args: -sol_type elas_uniform_strain -dm_plex_simplex 0 -displacement_petscspace_degree 1 -dm_refine 2 -dmsnes_check .0001 -pc_type lu
    test:
      suffix: 2d_q2_uniform_elas
      requires: !single
      args: -sol_type elas_uniform_strain -dm_plex_simplex 0 -displacement_petscspace_degree 2 -dm_refine 2 -dmsnes_check .0001 -pc_type lu
    test:
      suffix: 2d_q3_uniform_elas
      requires: !single
      args: -sol_type elas_uniform_strain -dm_plex_simplex 0 -displacement_petscspace_degree 3 -dm_refine 2 -dmsnes_check .0001 -pc_type lu
    test:
      suffix: 2d_p1_uniform_elas_step
      requires: triangle
      args: -sol_type elas_uniform_strain -deform_type step -displacement_petscspace_degree 1 -dm_refine 2 -dmsnes_check .0001 -pc_type lu

  testset:
    args: -dm_plex_simplex 0 -dm_plex_box_faces 3,3 -deform_type step -displacement_petscspace_degree 1 -dmsnes_check .0001 -pc_type lu

    test:
      suffix: 2d_q1_uniform_elas_step
      args: -sol_type elas_uniform_strain -dm_refine 2
    test:
      suffix: 2d_q1_quad_vlap_step
      args:
    test:
      suffix: 2d_q2_quad_vlap_step
      args: -displacement_petscspace_degree 2
    test:
      suffix: 2d_q1_quad_mass_step
      args: -sol_type mass_quad

  testset:
    args: -dm_plex_dim 3 -dm_plex_simplex 0 -dm_plex_box_lower -5,-5,-0.25 -dm_plex_box_upper 5,5,0.25 \
          -dm_plex_box_faces 5,5,2 -dm_plex_separate_marker -dm_refine 0 -petscpartitioner_type simple \
          -sol_type elas_ge \
          -snes_max_it 2 -snes_rtol 1.e-10 \
          -ksp_type cg -ksp_rtol 1.e-10 -ksp_max_it 100 -ksp_norm_type unpreconditioned \
          -pc_type gamg -pc_gamg_type agg -pc_gamg_agg_nsmooths 1 \
            -pc_gamg_coarse_eq_limit 10 -pc_gamg_reuse_interpolation true \
            -pc_gamg_square_graph 1 -pc_gamg_threshold 0.05 -pc_gamg_threshold_scale .0 \
            -mg_levels_ksp_max_it 2 -mg_levels_ksp_type chebyshev -mg_levels_ksp_chebyshev_esteig 0,0.05,0,1.1 -mg_levels_pc_type jacobi \
            -matptap_via scalable
    test:
      suffix: ge_q1_0
      args: -displacement_petscspace_degree 1

TEST*/
