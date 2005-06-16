#include "zpetsc.h"
#include "petscmat.h"

#if defined(PETSC_HAVE_FORTRAN_CAPS)
#define matgetrowij_                     MATGETROWIJ
#define matrestorerowij_                 MATRESTOREROWIJ
#define matgetrow_                       MATGETROW
#define matrestorerow_                   MATRESTOREROW
#define matview_                         MATVIEW
#define matgetarray_                     MATGETARRAY
#define matrestorearray_                 MATRESTOREARRAY
#define mattranspose_                    MATTRANSPOSE
#define matconvert_                      MATCONVERT
#define matgetsubmatrices_               MATGETSUBMATRICES
#define matzerorows_                     MATZEROROWS
#define matzerorowsis_                   MATZEROROWSIS
#define matzerorowslocal_                MATZEROROWSLOCAL
#define matzerorowslocalis_              MATZEROROWSLOCALIS
#elif !defined(PETSC_HAVE_FORTRAN_UNDERSCORE)
#define matgetrowij_                     matgetrowij
#define matrestorerowij_                 matrestorerowij
#define matgetrow_                       matgetrow
#define matrestorerow_                   matrestorerow
#define matview_                         matview
#define matgetarray_                     matgetarray
#define matrestorearray_                 matrestorearray
#define mattranspose_                    mattranspose
#define matconvert_                      matconvert
#define matgetsubmatrices_               matgetsubmatrices
#define matzerorows_                     matzerorows
#define matzerorowsis_                   matzerorowsis
#define matzerorowslocal_                matzerorowslocal
#define matzerorowslocalis_              matzerorowslocalis
#endif

EXTERN_C_BEGIN

void PETSC_STDCALL matgetrowij_(Mat *B,PetscInt *shift,PetscTruth *sym,PetscInt *n,PetscInt *ia,size_t *iia,PetscInt *ja,size_t *jja,
                                PetscTruth *done,PetscErrorCode *ierr)
{
  PetscInt *IA,*JA;
  *ierr = MatGetRowIJ(*B,*shift,*sym,n,&IA,&JA,done);if (*ierr) return;
  *iia  = PetscIntAddressToFortran(ia,IA);
  *jja  = PetscIntAddressToFortran(ja,JA);
}

void PETSC_STDCALL matrestorerowij_(Mat *B,PetscInt *shift,PetscTruth *sym,PetscInt *n,PetscInt *ia,size_t *iia,PetscInt *ja,size_t *jja,
                                    PetscTruth *done,PetscErrorCode *ierr)
{
  PetscInt *IA = PetscIntAddressFromFortran(ia,*iia),*JA = PetscIntAddressFromFortran(ja,*jja);
  *ierr = MatRestoreRowIJ(*B,*shift,*sym,n,&IA,&JA,done);
}

/*
   This is a poor way of storing the column and value pointers 
  generated by MatGetRow() to be returned with MatRestoreRow()
  but there is not natural,good place else to store them. Hence
  Fortran programmers can only have one outstanding MatGetRows()
  at a time.
*/
static PetscErrorCode    matgetrowactive = 0;
static const PetscInt    *my_ocols = 0;
static const PetscScalar *my_ovals = 0;

void PETSC_STDCALL matgetrow_(Mat *mat,PetscInt *row,PetscInt *ncols,PetscInt *cols,PetscScalar *vals,PetscErrorCode *ierr)
{
  const PetscInt    **oocols = &my_ocols;
  const PetscScalar **oovals = &my_ovals;

  if (matgetrowactive) {
     PetscError(__LINE__,"MatGetRow_Fortran",__FILE__,__SDIR__,1,0,
               "Cannot have two MatGetRow() active simultaneously\n\
               call MatRestoreRow() before calling MatGetRow() a second time");
     *ierr = 1;
     return;
  }

  CHKFORTRANNULLINTEGER(cols); if (!cols) oocols = PETSC_NULL;
  CHKFORTRANNULLSCALAR(vals);  if (!vals) oovals = PETSC_NULL;

  *ierr = MatGetRow(*mat,*row,ncols,oocols,oovals); 
  if (*ierr) return;

  if (oocols) { *ierr = PetscMemcpy(cols,my_ocols,(*ncols)*sizeof(PetscInt)); if (*ierr) return;}
  if (oovals) { *ierr = PetscMemcpy(vals,my_ovals,(*ncols)*sizeof(PetscScalar)); if (*ierr) return; }
  matgetrowactive = 1;
}

void PETSC_STDCALL matrestorerow_(Mat *mat,PetscInt *row,PetscInt *ncols,PetscInt *cols,PetscScalar *vals,PetscErrorCode *ierr)
{
  const PetscInt         **oocols = &my_ocols;
  const PetscScalar **oovals = &my_ovals;
  if (!matgetrowactive) {
     PetscError(__LINE__,"MatRestoreRow_Fortran",__FILE__,__SDIR__,1,0,
               "Must call MatGetRow() first");
     *ierr = 1;
     return;
  }
  CHKFORTRANNULLINTEGER(cols); if (!cols) oocols = PETSC_NULL;
  CHKFORTRANNULLSCALAR(vals);  if (!vals) oovals = PETSC_NULL;

  *ierr = MatRestoreRow(*mat,*row,ncols,oocols,oovals); 
  matgetrowactive = 0;
}

void PETSC_STDCALL matview_(Mat *mat,PetscViewer *vin,PetscErrorCode *ierr)
{
  PetscViewer v;
  PetscPatchDefaultViewers_Fortran(vin,v);
  *ierr = MatView(*mat,v);
}

void PETSC_STDCALL matgetarray_(Mat *mat,PetscScalar *fa,size_t *ia,PetscErrorCode *ierr)
{
  PetscScalar *mm;
  PetscInt    m,n;

  *ierr = MatGetArray(*mat,&mm); if (*ierr) return;
  *ierr = MatGetSize(*mat,&m,&n);  if (*ierr) return;
  *ierr = PetscScalarAddressToFortran((PetscObject)*mat,fa,mm,m*n,ia); if (*ierr) return;
}

void PETSC_STDCALL matrestorearray_(Mat *mat,PetscScalar *fa,size_t *ia,PetscErrorCode *ierr)
{
  PetscScalar          *lx;
  PetscInt                  m,n;

  *ierr = MatGetSize(*mat,&m,&n); if (*ierr) return;
  *ierr = PetscScalarAddressFromFortran((PetscObject)*mat,fa,*ia,m*n,&lx);if (*ierr) return;
  *ierr = MatRestoreArray(*mat,&lx);if (*ierr) return;
}

void PETSC_STDCALL mattranspose_(Mat *mat,Mat *B,PetscErrorCode *ierr)
{
  CHKFORTRANNULLOBJECT(B);
  *ierr = MatTranspose(*mat,B);
}

void PETSC_STDCALL matconvert_(Mat *mat,CHAR outtype PETSC_MIXED_LEN(len),MatReuse *reuse,Mat *M,PetscErrorCode *ierr PETSC_END_LEN(len))
{
  char *t;
  FIXCHAR(outtype,len,t);
  *ierr = MatConvert(*mat,t,*reuse,M);
  FREECHAR(outtype,t);
}

/*
    MatGetSubmatrices() is slightly different from C since the 
    Fortran provides the array to hold the submatrix objects,while in C that 
    array is allocated by the MatGetSubmatrices()
*/
void PETSC_STDCALL matgetsubmatrices_(Mat *mat,PetscInt *n,IS *isrow,IS *iscol,MatReuse *scall,Mat *smat,PetscErrorCode *ierr)
{
  Mat *lsmat;
  PetscInt i;

  if (*scall == MAT_INITIAL_MATRIX) {
    *ierr = MatGetSubMatrices(*mat,*n,isrow,iscol,*scall,&lsmat);
    for (i=0; i<*n; i++) {
      smat[i] = lsmat[i];
    }
    *ierr = PetscFree(lsmat); 
  } else {
    *ierr = MatGetSubMatrices(*mat,*n,isrow,iscol,*scall,&smat);
  }
}

void PETSC_STDCALL matzerorows_(Mat *mat,PetscInt *numRows,PetscInt *rows,PetscScalar *diag,PetscErrorCode *ierr)
{
  CHKFORTRANNULLSCALAR(diag);
  *ierr = MatZeroRows(*mat,*numRows,rows,*diag);
}

void PETSC_STDCALL matzerorowsis_(Mat *mat,IS *is,PetscScalar *diag,PetscErrorCode *ierr)
{
  CHKFORTRANNULLSCALAR(diag);
  *ierr = MatZeroRowsIS(*mat,*is,*diag);
}

void PETSC_STDCALL matzerorowslocal_(Mat *mat,PetscInt *numRows,PetscInt *rows,PetscScalar *diag,PetscErrorCode *ierr)
{
  CHKFORTRANNULLSCALAR(diag);
  *ierr = MatZeroRowsLocal(*mat,*numRows,rows,*diag);
}

void PETSC_STDCALL matzerorowslocalis_(Mat *mat,IS *is,PetscScalar *diag,PetscErrorCode *ierr)
{
  CHKFORTRANNULLSCALAR(diag);
  *ierr = MatZeroRowsLocalIS(*mat,*is,*diag);
}


EXTERN_C_END
