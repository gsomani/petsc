#ifndef lint
static char vcid[] = "$Id: direct.c,v 1.18 1995/04/17 19:53:06 curfman Exp bsmith $";
#endif
/*
   Defines a direct factorization preconditioner for any Mat implementation
   Note: this need not be consided a preconditioner since it supplies
         a direct solver.
*/
#include "pcimpl.h"
#include "options.h"

typedef struct {
  Mat fact;
  int ordering,inplace;
} PC_LU;

/*@
   PCLUSetOrdering - Sets the ordering to use for a direct 
   factorization.

   Input Parameters:
.  pc - the preconditioner context
.  ordering - the type of ordering to use, one of the following:
$    ORDER_NATURAL - Natural 
$    ORDER_ND - Nested Dissection
$    ORDER_1WD - One-way Dissection
$    ORDER_RCM - Reverse Cuthill-McGee
$    ORDER_QMD - Quotient Minimum Degree

   Options Database Key:
$  -pc_lu_ordering  <name>,  where <name> is one of:
$      natural, nd, 1wd, rcm, qmd

.keywords: PC, set, ordering, factorization, direct, LU, Cholesky,
           fill, natural, Nested Dissection, One-way Dissection,
           Reverse Cuthill-McGee, Quotient Minimum Degree

.seealso: PCSetLUUseInplace()
@*/
int PCLUSetOrdering(PC pc,int ordering)
{
  PC_LU *dir;
  VALIDHEADER(pc,PC_COOKIE);
  dir = (PC_LU *) pc->data;
  if (pc->type != PCLU) return 0;
  dir->ordering = ordering;
  return 0;
}
/*@
   PCLUSetUseInplace - Tells the system to do an in-place factorization.
   For some implementations, for instance, dense matrices, this enables the 
   solution of much larger problems. 

   Input Parameters:
.  pc - the preconditioner context

   Options Database Key:
$  -pc_lu_in_place

   Note:
   PCLUSetUseInplace() can only be used with the KSP method KSPPREONLY.
   This is because the Krylov space methods require an application of the 
   matrix multiplication, which is not possible here because the matrix has 
   been factored in-place, replacing the original matrix.

.keywords: PC, set, factorization, direct, inplace, in-place, LU, Cholesky

.seealso: PCLUSetOrdering()
@*/
int PCLUSetUseInplace(PC pc)
{
  PC_LU *dir;
  VALIDHEADER(pc,PC_COOKIE);
  dir = (PC_LU *) pc->data;
  if (pc->type != PCLU) return 0;
  dir->inplace = 1;
  return 0;
}
static int PCSetFromOptions_LU(PC pc)
{
  char      name[10];
  int       ordering = ORDER_ND;
  if (OptionsHasName(0,pc->prefix,"-pc_lu_in_place")) {
    PCLUSetUseInplace(pc);
  }
  if (OptionsGetString(0,pc->prefix,"-pc_lu_ordering",name,10)) {
    if (!strcmp(name,"nd")) ordering = ORDER_ND;
    else if (!strcmp(name,"natural")) ordering = ORDER_NATURAL;
    else if (!strcmp(name,"1wd")) ordering = ORDER_1WD;
    else if (!strcmp(name,"rcm")) ordering = ORDER_RCM;
    else if (!strcmp(name,"qmd")) ordering = ORDER_QMD;
    else fprintf(stderr,"Unknown order: %s\n",name);
    PCLUSetOrdering(pc,ordering);
  }
  return 0;
}

static int PCPrintHelp_LU(PC pc)
{
  char *p;
  if (pc->prefix) p = pc->prefix; else p = "-";
  fprintf(stderr," %spc_lu_in_place: do factorization in place\n",p);
  fprintf(stderr," %spc_lu_ordering name: ordering to reduce fill",p);
  fprintf(stderr," (nd,natural,1wd,rcm,qmd)\n");
  return 0;
}

static int PCSetUp_LU(PC pc)
{
  IS        row,col;
  int       ierr;
  PC_LU *dir = (PC_LU *) pc->data;
  if (dir->inplace) {
    ierr = MatGetReordering(pc->pmat,dir->ordering,&row,&col); CHKERR(ierr);
    if (row) {PLogObjectParent(pc,row);PLogObjectParent(pc,col);}
    if ((ierr = MatLUFactor(pc->pmat,row,col))) SETERR(ierr,0);
  }
  else {
    if (!pc->setupcalled) {
      ierr = MatGetReordering(pc->pmat,dir->ordering,&row,&col); CHKERR(ierr);
      if (row) {PLogObjectParent(pc,row);PLogObjectParent(pc,col);}
      ierr = MatLUFactorSymbolic(pc->pmat,row,col,&dir->fact); CHKERR(ierr);
      PLogObjectParent(pc,dir->fact);
    }
    else if (!(pc->flag & MAT_SAME_NONZERO_PATTERN)) { 
      ierr = MatDestroy(dir->fact); CHKERR(ierr);
      ierr = MatGetReordering(pc->pmat,dir->ordering,&row,&col); CHKERR(ierr);
      if (row) {PLogObjectParent(pc,row);PLogObjectParent(pc,col);}
      ierr = MatLUFactorSymbolic(pc->pmat,row,col,&dir->fact); CHKERR(ierr);
      PLogObjectParent(pc,dir->fact);
    }
    ierr = MatLUFactorNumeric(pc->pmat,&dir->fact); CHKERR(ierr);
  }
  return 0;
}

static int PCDestroy_LU(PetscObject obj)
{
  PC        pc   = (PC) obj;
  PC_LU *dir = (PC_LU*) pc->data;

  if (!dir->inplace) MatDestroy(dir->fact);
  FREE(dir); 
  PLogObjectDestroy(pc);
  PETSCHEADERDESTROY(pc);
  return 0;
}

static int PCApply_LU(PC pc,Vec x,Vec y)
{
  PC_LU *dir = (PC_LU *) pc->data;
  if (dir->inplace) return MatSolve(pc->pmat,x,y);
  else  return MatSolve(dir->fact,x,y);
}

int PCCreate_LU(PC pc)
{
  PC_LU *dir = NEW(PC_LU); CHKPTR(dir);
  dir->fact     = 0;
  dir->ordering = ORDER_ND;
  dir->inplace  = 0;
  pc->destroy   = PCDestroy_LU;
  pc->apply     = PCApply_LU;
  pc->setup     = PCSetUp_LU;
  pc->type      = PCLU;
  pc->data      = (void *) dir;
  pc->setfrom   = PCSetFromOptions_LU;
  pc->printhelp = PCPrintHelp_LU;
  return 0;
}
