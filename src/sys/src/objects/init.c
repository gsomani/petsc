/*$Id: init.c,v 1.62 2000/05/05 22:14:00 balay Exp balay $*/
/*

   This file defines part of the initialization of PETSc

  This file uses regular malloc and free because it cannot know 
  what malloc is being used until it has already processed the input.
*/

#include "petsc.h"        /*I  "petsc.h"   I*/
#include "petscsys.h"
#if defined(PETSC_HAVE_STDLIB_H)
#include <stdlib.h>
#endif
#if defined(PETSC_HAVE_MALLOC_H) && !defined(__cplusplus)
#include <malloc.h>
#endif
#include "petscfix.h"

/* ------------------------Nasty global variables -------------------------------*/
/*
     Indicates if PETSc started up MPI, or it was 
   already started before PETSc was initialized.
*/
PetscTruth PetscBeganMPI         = PETSC_FALSE;
PetscTruth PetscInitializeCalled = PETSC_FALSE;
int        PetscGlobalRank = -1,PetscGlobalSize = -1;
MPI_Comm   PETSC_COMM_WORLD = 0;
MPI_Comm   PETSC_COMM_SELF  = 0;

#if defined(PETSC_USE_COMPLEX)
MPI_Datatype  MPIU_COMPLEX;
Scalar        PETSC_i; 
#else
Scalar        PETSC_i = 0.0; 
#endif

/*
     These are needed by petscta.h
*/
char _BT_mask,_BT_c;
int  _BT_idx;

/*
     Determines if all PETSc objects are published to the AMS
*/
#if defined(PETSC_HAVE_AMS)
PetscTruth PetscAMSPublishAll = PETSC_FALSE;
#endif

/*
       Function that is called to display all error messages
*/
extern int  PetscErrorPrintfDefault(const char [],...);
extern int  PetscHelpPrintfDefault(MPI_Comm,const char [],...);
int (*PetscErrorPrintf)(const char [],...)          = PetscErrorPrintfDefault;
int (*PetscHelpPrintf)(MPI_Comm,const char [],...)  = PetscHelpPrintfDefault;

/* ------------------------------------------------------------------------------*/
/* 
   Optional file where all PETSc output from various prints is saved
*/
FILE *petsc_history = PETSC_NULL;

#undef __FUNC__  
#define __FUNC__ /*<a name="PLogOpenHistoryFile"></a>*/"PLogOpenHistoryFile"
int PLogOpenHistoryFile(const char filename[],FILE **fd)
{
  int  ierr,rank,size;
  char pfile[256],pname[256],fname[256],date[64];

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  if (!rank) {
    char arch[10];
    ierr = PetscGetArchType(arch,10);CHKERRQ(ierr);
    ierr = PetscGetDate(date,64);CHKERRQ(ierr);
    ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
    if (filename) {
      ierr = PetscFixFilename(filename,fname);CHKERRQ(ierr);
    } else {
      ierr = PetscGetHomeDirectory(pfile,240);CHKERRQ(ierr);
      ierr = PetscStrcat(pfile,"/.petschistory");CHKERRQ(ierr);
      ierr = PetscFixFilename(pfile,fname);CHKERRQ(ierr);
    }

    *fd = fopen(fname,"a"); if (!fd) SETERRQ1(PETSC_ERR_FILE_OPEN,0,"Cannot open file: %s",fname);
    fprintf(*fd,"---------------------------------------------------------\n");
    fprintf(*fd,"%s %s\n",PETSC_VERSION_NUMBER,date);
    ierr = PetscGetProgramName(pname,256);CHKERRQ(ierr);
    fprintf(*fd,"%s on a %s, %d proc. with options:\n",pname,arch,size);
    OptionsPrint(*fd);
    fprintf(*fd,"---------------------------------------------------------\n");
    fflush(*fd);
  }
  PetscFunctionReturn(0); 
}

#undef __FUNC__  
#define __FUNC__ /*<a name="PLogCloseHistoryFile"></a>*/"PLogCloseHistoryFile"
int PLogCloseHistoryFile(FILE **fd)
{
  int  rank,ierr;
  char date[64];

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);
  if (rank) PetscFunctionReturn(0);
  ierr = PetscGetDate(date,64);CHKERRQ(ierr);
  fprintf(*fd,"---------------------------------------------------------\n");
  fprintf(*fd,"Finished at %s\n",date);
  fprintf(*fd,"---------------------------------------------------------\n");
  fflush(*fd);
  fclose(*fd);
  PetscFunctionReturn(0); 
}

/* ------------------------------------------------------------------------------*/

PetscTruth PetscCompare          = PETSC_FALSE;
PetscReal  PetscCompareTolerance = 1.e-10;

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"PetscCompareInt"
/*@C
   PetscCompareInt - Compares integers while running with PETScs incremental
   debugger.

   Collective on PETSC_COMM_WORLD

   Input Parameter:
.  d - integer to compare

   Options Database Key:
.  -compare - Activates PetscCompareDouble(), PetscCompareInt(), and PetscCompareScalar()

   Level: advanced

.seealso: PetscCompareDouble(), PetscCompareScalar()
@*/
int PetscCompareInt(int d)
{
  int work = d,ierr;

  PetscFunctionBegin;
  ierr = MPI_Bcast(&work,1,MPI_INT,0,MPI_COMM_WORLD);CHKERRQ(ierr);
  if (d != work) {
    SETERRQ(PETSC_ERR_PLIB,0,"Inconsistent integer");
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"PetscCompareDouble"
/*@C
   PetscCompareDouble - Compares doubles while running with PETScs incremental
   debugger.

   Collective on PETSC_COMM_WORLD

   Input Parameter:
.  d - double precision number to compare

   Options Database Key:
.  -compare - Activates PetscCompareDouble(), PetscCompareInt(), and PetscCompareScalar()

   Level: advanced

.seealso: PetscCompareInt(), PetscComparseScalar()
@*/
int PetscCompareDouble(double d)
{
  double work = d;
  int    ierr;

  PetscFunctionBegin;
  ierr = MPI_Bcast(&work,1,MPI_DOUBLE,0,MPI_COMM_WORLD);CHKERRQ(ierr);
  if (!d && !work) PetscFunctionReturn(0);
  if (PetscAbsDouble(work - d)/PetscMax(PetscAbsDouble(d),PetscAbsDouble(work)) > PetscCompareTolerance) {
    SETERRQ(PETSC_ERR_PLIB,0,"Inconsistent double");
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"PetscCompareScalar"
/*@C
   PetscCompareScalar - Compares scalars while running with PETScs incremental
   debugger.

   Collective on PETSC_COMM_WORLD

   Input Parameter:
.  d - scalar to compare

   Options Database Key:
.  -compare - Activates PetscCompareDouble(), PetscCompareInt(), and PetscCompareScalar()

   Level: advanced

.seealso: PetscCompareInt(), PetscComparseDouble()
@*/
int PetscCompareScalar(Scalar d)
{
  Scalar work = d;
  int    ierr;

  PetscFunctionBegin;
  ierr = MPI_Bcast(&work,2,MPI_DOUBLE,0,MPI_COMM_WORLD);CHKERRQ(ierr);
  if (!PetscAbsScalar(d) && !PetscAbsScalar(work)) PetscFunctionReturn(0);
  if (PetscAbsScalar(work - d)/PetscMax(PetscAbsScalar(d),PetscAbsScalar(work)) >= PetscCompareTolerance) {
    SETERRQ(PETSC_ERR_PLIB,0,"Inconsistent scalar");
  }
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"PetscCompareInitialize"
/*
    PetscCompareInitialize - If there is a command line option -compare then
    this routine calls MPI_Init() and sets up two PETSC_COMM_WORLD, one for 
    each program being compared.

    Note: 
    Only works with C programs.
*/
int PetscCompareInitialize(double tol)
{
  int        ierr,i,len,rank,*gflag,size,mysize;
  char       pname[256],basename[256];
  MPI_Group  group_all,group_sub;
  PetscTruth work;

  PetscFunctionBegin;
  ierr = PetscGetProgramName(pname,256);CHKERRQ(ierr);
  PetscCompareTolerance = tol;

  ierr = MPI_Comm_rank(MPI_COMM_WORLD,&rank);CHKERRQ(ierr);
  ierr = MPI_Comm_size(MPI_COMM_WORLD,&size);CHKERRQ(ierr);
  if (!rank) {
    ierr = PetscStrcpy(basename,pname);CHKERRQ(ierr);
    ierr = PetscStrlen(basename,&len);CHKERRQ(ierr);
  }

  /* broadcase name from first processor to all processors */
  ierr = MPI_Bcast(&len,1,MPI_INT,0,MPI_COMM_WORLD);CHKERRQ(ierr);
  ierr = MPI_Bcast(basename,len+1,MPI_CHAR,0,MPI_COMM_WORLD);CHKERRQ(ierr);

  /* determine what processors belong to my group */
  ierr = PetscStrcmp(pname,basename,&work);CHKERRQ(ierr);

  gflag = (int*)malloc(size*sizeof(int));CHKPTRQ(gflag);
  ierr = MPI_Allgather(&work,1,MPI_INT,gflag,1,MPI_INT,MPI_COMM_WORLD);CHKERRQ(ierr);
  mysize = 0;
  for (i=0; i<size; i++) {
    if (work == gflag[i]) gflag[mysize++] = i;
  }
  /* printf("[%d] my name %s basename %s mysize %d\n",rank,programname,basename,mysize); */

  if (!mysize || mysize == size) {
    SETERRQ(PETSC_ERR_ARG_IDN,0,"Need two different programs to compare");
  }

  /* create a new communicator for each program */
  ierr = MPI_Comm_group(MPI_COMM_WORLD,&group_all);CHKERRQ(ierr);
  ierr = MPI_Group_incl(group_all,mysize,gflag,&group_sub);CHKERRQ(ierr);
  ierr = MPI_Comm_create(MPI_COMM_WORLD,group_sub,&PETSC_COMM_WORLD);CHKERRQ(ierr);
  ierr = MPI_Group_free(&group_all);CHKERRQ(ierr);
  ierr = MPI_Group_free(&group_sub);CHKERRQ(ierr);
  free(gflag);

  PetscCompare = PETSC_TRUE;
  PLogInfo(0,"PetscCompareInitialize:Configured to compare two programs\n",rank);
  PetscFunctionReturn(0);
}
/* ------------------------------------------------------------------------------------*/

 
/* 
   This is ugly and probably belongs somewhere else, but I want to 
  be able to put a true MPI abort error handler with command line args.

    This is so MPI errors in the debugger will leave all the stack 
  frames. The default abort cleans up and exits.
*/

#undef __FUNC__  
#define __FUNC__ /*<a name="Petsc_MPI_AbortOnError"></a>*/"Petsc_MPI_AbortOnError"
void Petsc_MPI_AbortOnError(MPI_Comm *comm,int *flag) 
{
  PetscFunctionBegin;
  (*PetscErrorPrintf)("MPI error %d\n",*flag);
  abort();
}

#undef __FUNC__  
#define __FUNC__ /*<a name="Petsc_MPI_DebuggerOnError"></a>*/"Petsc_MPI_DebuggerOnError"
void Petsc_MPI_DebuggerOnError(MPI_Comm *comm,int *flag) 
{
  int ierr;

  PetscFunctionBegin;
  (*PetscErrorPrintf)("MPI error %d\n",*flag);
  ierr = PetscAttachDebugger();
  if (ierr) { /* hopeless so get out */
    MPI_Finalize();
    exit(*flag);
  }
}

#if defined(PETSC_HAVE_MALLOC_VERIFY)
EXTERN_C_BEGIN
extern int malloc_debug(int);
EXTERN_C_END
#endif

PetscTruth        PetscPublishOptions = PETSC_FALSE;
extern int        PLogInfoAllow(PetscTruth,char *);
extern int        PetscSetUseTrMalloc_Private(void);
extern PetscTruth petscsetmallocvisited;
static char       emacsmachinename[128];

int (*PetscExternalVersionFunction)(MPI_Comm) = 0;
int (*PetscExternalHelpFunction)(MPI_Comm)    = 0;

#undef __FUNC__  
#define __FUNC__ /*<a name="PetscSetHelpVersionFunctions"></a>*/"PetscSetHelpVersionFunctions"
/*@C 
   PetscSetHelpVersionFunctions - Sets functions that print help and version information
   before the PETSc help and version information is printed. Must call BEFORE PetscInitialize().
   This routine enables a "higher-level" package that uses PETSc to print its messages first.

   Input Parameter:
+  help - the help function (may be PETSC_NULL)
-  version - the version function (may be PETSc null)

   Level: developer

.keywords: version information, help information
@*/
int PetscSetHelpVersionFunctions(int (*help)(MPI_Comm),int (*version)(MPI_Comm))
{
  PetscFunctionBegin;
  PetscExternalHelpFunction    = help;
  PetscExternalVersionFunction = version;
  PetscFunctionReturn(0);
}

#undef __FUNC__  
#define __FUNC__ /*<a name=""></a>*/"OptionsCheckInitial"
int OptionsCheckInitial(void)
{
  char       string[64],mname[256],*f;
  MPI_Comm   comm = PETSC_COMM_WORLD;
  PetscTruth flg1,flg2,flg3,flag;
  int        ierr,*nodes,i,rank;

  PetscFunctionBegin;
  ierr = MPI_Comm_rank(PETSC_COMM_WORLD,&rank);CHKERRQ(ierr);

#if defined(PETSC_HAVE_AMS)
  ierr = OptionsHasName(PETSC_NULL,"-ams_publish_options",&flg3);CHKERRQ(ierr);
  if (flg3) PetscPublishOptions = PETSC_TRUE;
#endif

  /*
      Setup the memory management; support for tracing malloc() usage 
  */
  ierr = OptionsHasName(PETSC_NULL,"-trmalloc_log",&flg3);CHKERRQ(ierr);
#if defined(PETSC_USE_BOPT_g)
  /* always does trmalloc with BOPT=g, just check so does not reported never checked */
  ierr = OptionsHasName(PETSC_NULL,"-trmalloc",&flg1);CHKERRQ(ierr);
  ierr = OptionsHasName(PETSC_NULL,"-trmalloc_off",&flg1);CHKERRQ(ierr);
  if (!flg1 && !petscsetmallocvisited) {
    ierr = PetscSetUseTrMalloc_Private();CHKERRQ(ierr); 
  }
#else
  ierr = OptionsHasName(PETSC_NULL,"-trdump",&flg1);CHKERRQ(ierr);
  ierr = OptionsHasName(PETSC_NULL,"-trmalloc",&flg2);CHKERRQ(ierr);
  if (flg1 || flg2 || flg3) {ierr = PetscSetUseTrMalloc_Private();CHKERRQ(ierr);}
#endif
  if (flg3) {
    ierr = PetscTrLog();CHKERRQ(ierr); 
  }
  ierr = OptionsHasName(PETSC_NULL,"-trdebug",&flg1);CHKERRQ(ierr);
  if (flg1) { 
    ierr = PetscTrDebugLevel(1);CHKERRQ(ierr);
#if defined(PETSC_HAVE_MALLOC_VERIFY) && defined(PETSC_USE_BOPT_g)
    malloc_debug(2);
#endif
  }

  /*
      Set the display variable for graphics
  */
  ierr = PetscSetDisplay();CHKERRQ(ierr);

  /*
      Print the PETSc version information
  */
  ierr = OptionsHasName(PETSC_NULL,"-v",&flg1);CHKERRQ(ierr);
  ierr = OptionsHasName(PETSC_NULL,"-version",&flg2);CHKERRQ(ierr);
  ierr = OptionsHasName(PETSC_NULL,"-help",&flg3);CHKERRQ(ierr);
  if (flg1 || flg2 || flg3){

    /*
       Print "higher-level" package version message 
    */
    if (PetscExternalVersionFunction) {
      ierr = (*PetscExternalVersionFunction)(comm);CHKERRQ(ierr);
    }

    ierr = (*PetscHelpPrintf)(comm,"--------------------------------------------\
------------------------------\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"\t   %s\n",PETSC_VERSION_NUMBER);CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"%s",PETSC_AUTHOR_INFO);CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"See docs/copyright.html for copyright information\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"See docs/changes/index.html for recent updates.\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"See docs/troubleshooting.html for problems.\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"See docs/manualpages/index.html for help. \n");CHKERRQ(ierr);
#if !defined(PARCH_win32)
    ierr = (*PetscHelpPrintf)(comm,"Libraries linked from %s\n",PETSC_LDIR);CHKERRQ(ierr);
#endif
    ierr = (*PetscHelpPrintf)(comm,"--------------------------------------------\
------------------------------\n");CHKERRQ(ierr);
  }

  /*
       Print "higher-level" package help message 
  */
  if (flg3){
    if (PetscExternalHelpFunction) {
      ierr = (*PetscExternalHelpFunction)(comm);CHKERRQ(ierr);
    }
  }

  /*
      Setup the error handling
  */
  ierr = OptionsHasName(PETSC_NULL,"-fp_trap",&flg1);CHKERRQ(ierr);
  if (flg1) { ierr = PetscSetFPTrap(PETSC_FP_TRAP_ON);CHKERRQ(ierr); }
  ierr = OptionsHasName(PETSC_NULL,"-on_error_abort",&flg1);CHKERRQ(ierr);
  if (flg1) { ierr = PetscPushErrorHandler(PetscAbortErrorHandler,0);CHKERRQ(ierr)} 
  ierr = OptionsHasName(PETSC_NULL,"-on_error_stop",&flg1);CHKERRQ(ierr);
  if (flg1) { ierr = PetscPushErrorHandler(PetscStopErrorHandler,0);CHKERRQ(ierr)}
  ierr = OptionsHasName(PETSC_NULL,"-mpi_return_on_error",&flg1);CHKERRQ(ierr);
  if (flg1) {
    ierr = MPI_Errhandler_set(comm,MPI_ERRORS_RETURN);CHKERRQ(ierr);
  }
  ierr = OptionsHasName(PETSC_NULL,"-no_signal_handler",&flg1);CHKERRQ(ierr);
  if (!flg1) { ierr = PetscPushSignalHandler(PetscDefaultSignalHandler,(void*)0);CHKERRQ(ierr) }

  /*
      Setup debugger information
  */
#if defined(PETSC_USE_DBX_DEBUGGER)
  ierr = PetscSetDebugger("dbx",PETSC_TRUE);CHKERRQ(ierr);
#elif defined(PETSC_USE_XDB_DEBUGGER) 
  ierr = PetscSetDebugger("xdb",PETSC_TRUE);CHKERRQ(ierr);
#else  /* Default is gdb */
  ierr = PetscSetDebugger("gdb",PETSC_TRUE);CHKERRQ(ierr);
#endif
  ierr = OptionsGetString(PETSC_NULL,"-on_error_attach_debugger",string,64,&flg1);CHKERRQ(ierr);
  if (flg1) {
    char           *debugger = 0;
    PetscTruth     xterm     = PETSC_TRUE;
    MPI_Errhandler err_handler;

    ierr = PetscStrstr(string,"noxterm",&f);CHKERRQ(ierr);
    if (f) xterm = PETSC_FALSE;
    ierr = PetscStrstr(string,"xdb",&f);CHKERRQ(ierr);
    if (f)     debugger = "xdb";
    ierr = PetscStrstr(string,"dbx",&f);CHKERRQ(ierr);
    if (f)     debugger = "dbx";
    ierr = PetscStrstr(string,"xldb",&f);CHKERRQ(ierr);
    if (f)     debugger = "xldb";
    ierr = PetscStrstr(string,"gdb",&f);CHKERRQ(ierr);
    if (f)     debugger = "gdb";
    ierr = PetscStrstr(string,"xxgdb",&f);CHKERRQ(ierr);
    if (f)     debugger = "xxgdb";
    ierr = PetscStrstr(string,"ups",&f);CHKERRQ(ierr);
    if (f)     debugger = "ups";

    ierr = PetscSetDebugger(debugger,xterm);CHKERRQ(ierr);
    ierr = MPI_Errhandler_create((MPI_Handler_function*)Petsc_MPI_DebuggerOnError,&err_handler);CHKERRQ(ierr);
    ierr = MPI_Errhandler_set(comm,err_handler);CHKERRQ(ierr);
    ierr = PetscPushErrorHandler(PetscAttachDebuggerErrorHandler,0);CHKERRQ(ierr);
  }
  ierr = OptionsGetString(PETSC_NULL,"-start_in_debugger",string,64,&flg1);CHKERRQ(ierr);
  ierr = OptionsGetString(PETSC_NULL,"-stop_for_debugger",string,64,&flg2);CHKERRQ(ierr);
  if (flg1 || flg2) {
    char           *debugger = 0;
    PetscTruth     xterm     = PETSC_TRUE;
    int            size;
    MPI_Errhandler err_handler;
    /*
       we have to make sure that all processors have opened 
       connections to all other processors, otherwise once the 
       debugger has stated it is likely to receive a SIGUSR1
       and kill the program. 
    */
    ierr = MPI_Comm_size(PETSC_COMM_WORLD,&size);CHKERRQ(ierr);
    if (size > 2) {
      int        dummy;
      MPI_Status status;
      for (i=0; i<size; i++) {
        ierr = MPI_Send(&dummy,1,MPI_INT,i,109,PETSC_COMM_WORLD);CHKERRQ(ierr);
      }
      for (i=0; i<size; i++) {
        ierr = MPI_Recv(&dummy,1,MPI_INT,i,109,PETSC_COMM_WORLD,&status);CHKERRQ(ierr);
      }
    }
    /* check if this processor node should be in debugger */
    nodes = (int*)PetscMalloc(size*sizeof(int));CHKPTRQ(nodes);
    ierr  = OptionsGetIntArray(PETSC_NULL,"-debugger_nodes",nodes,&size,&flag);CHKERRQ(ierr);
    if (flag) {
      for (i=0; i<size; i++) {
        if (nodes[i] == rank) { flag = PETSC_FALSE; break; }
      }
    }
    if (!flag) {        
      ierr = PetscStrstr(string,"noxterm",&f);CHKERRQ(ierr);
      if (f) xterm = PETSC_FALSE;
      ierr = PetscStrstr(string,"xdb",&f);CHKERRQ(ierr);
      if (f)     debugger = "xdb";
      ierr = PetscStrstr(string,"dbx",&f);CHKERRQ(ierr);
      if (f)     debugger = "dbx";
      ierr = PetscStrstr(string,"xldb",&f);CHKERRQ(ierr);
      if (f)     debugger = "xldb";
      ierr = PetscStrstr(string,"gdb",&f);CHKERRQ(ierr);
      if (f)     debugger = "gdb";
      ierr = PetscStrstr(string,"xxgdb",&f);CHKERRQ(ierr);
      if (f)     debugger = "xxgdb";
      ierr = PetscStrstr(string,"ups",&f);CHKERRQ(ierr);
      if (f)     debugger = "ups";

      ierr = PetscSetDebugger(debugger,xterm);CHKERRQ(ierr);
      ierr = PetscPushErrorHandler(PetscAbortErrorHandler,0);CHKERRQ(ierr);
      if (flg1) {
        ierr = PetscAttachDebugger();CHKERRQ(ierr);
      } else {
        ierr = PetscStopForDebugger();CHKERRQ(ierr);
      }
      ierr = MPI_Errhandler_create((MPI_Handler_function*)Petsc_MPI_AbortOnError,&err_handler);CHKERRQ(ierr);
      ierr = MPI_Errhandler_set(comm,err_handler);CHKERRQ(ierr);
    }
    ierr = PetscFree(nodes);CHKERRQ(ierr);
  }

  ierr = OptionsGetString(PETSC_NULL,"-on_error_emacs",emacsmachinename,128,&flg1);CHKERRQ(ierr);
  if (flg1 && !rank) {ierr = PetscPushErrorHandler(PetscEmacsClientErrorHandler,emacsmachinename);CHKERRQ(ierr)}

  /*
        Setup profiling and logging
  */
#if defined(PETSC_USE_LOG)
  mname[0] = 0;
  ierr = OptionsGetString(PETSC_NULL,"-log_history",mname,256,&flg1);CHKERRQ(ierr);
  if (flg1) {
    if (mname[0]) {
      ierr = PLogOpenHistoryFile(mname,&petsc_history);CHKERRQ(ierr);
    } else {
      ierr = PLogOpenHistoryFile(0,&petsc_history);CHKERRQ(ierr);
    }
  }
  ierr = OptionsHasName(PETSC_NULL,"-log_info",&flg1);CHKERRQ(ierr);
  if (flg1) { 
    char logname[256]; logname[0] = 0;
    ierr = OptionsGetString(PETSC_NULL,"-log_info",logname,250,&flg1);CHKERRQ(ierr);
    if (logname[0]) {
      PLogInfoAllow(PETSC_TRUE,logname); 
    } else {
      PLogInfoAllow(PETSC_TRUE,PETSC_NULL); 
    }
  }
#if defined(PETSC_HAVE_MPE)
  ierr = OptionsHasName(PETSC_NULL,"-log_mpe",&flg1);CHKERRQ(ierr);
  if (flg1) PLogMPEBegin();
#endif
  ierr = OptionsHasName(PETSC_NULL,"-log_all",&flg1);CHKERRQ(ierr);
  ierr = OptionsHasName(PETSC_NULL,"-log",&flg2);CHKERRQ(ierr);
  ierr = OptionsHasName(PETSC_NULL,"-log_summary",&flg3);CHKERRQ(ierr);
  if (flg1)              {  ierr = PLogAllBegin();CHKERRQ(ierr); }
  else if (flg2 || flg3) {  ierr = PLogBegin();CHKERRQ(ierr);}
    
  ierr = OptionsGetString(PETSC_NULL,"-log_trace",mname,250,&flg1);CHKERRQ(ierr);
  if (flg1) { 
    char name[256],fname[256];
    FILE *file;
    if (mname[0]) {
      sprintf(name,"%s.%d",mname,rank);
      ierr = PetscFixFilename(name,fname);CHKERRQ(ierr);
      file = fopen(fname,"w"); 
      if (!file) {
        SETERRQ1(PETSC_ERR_FILE_OPEN,0,"Unable to open trace file: %s",fname);
      }
    } else {
      file = stdout;
    }
    ierr = PLogTraceBegin(file);CHKERRQ(ierr);
  }
#endif

  /*
      Setup building of stack frames for all function calls
  */
#if defined(PETSC_USE_STACK)
#if defined(PETSC_USE_BOPT_g)
  ierr = PetscStackCreate();CHKERRQ(ierr);
#else
  ierr = OptionsHasName(PETSC_NULL,"-log_stack",&flg1);CHKERRQ(ierr);
  if (flg1) {
    ierr = PetscStackCreate();CHKERRQ(ierr);
  }
#endif
#endif


  /*
       Print basic help message
  */
  ierr = OptionsHasName(PETSC_NULL,"-help",&flg1);CHKERRQ(ierr);
  if (flg1) {
    ierr = (*PetscHelpPrintf)(comm,"Options for all PETSc programs:\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -on_error_abort: cause an abort when an error is");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," detected. Useful \n       only when run in the debugger\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -on_error_attach_debugger [gdb,dbx,xxgdb,ups,noxterm]\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"       start the debugger in new xterm\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"       unless noxterm is given\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -start_in_debugger [gdb,dbx,xxgdb,ups,noxterm]\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"       start all processes in the debugger\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -on_error_emacs <machinename>\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"    emacs jumps to error file\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -debugger_nodes [n1,n2,..] Nodes to start in debugger\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -debugger_pause [m] : delay (in seconds) to attach debugger\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -stop_for_debugger : prints message on how to attach debugger manually\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"                      waits the delay for you to attach\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -display display: Location where graphics and debuggers are displayed\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -no_signal_handler: do not trap error signals\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -mpi_return_on_error: MPI returns error code, rather than abort on internal error\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -fp_trap: stop on floating point exceptions\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm,"           note on IBM RS6000 this slows run greatly\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -trdump <optional filename>: dump list of unfreed memory at conclusion\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -trmalloc: use our error checking malloc\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -trmalloc_off: don't use error checking malloc\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -trinfo: prints total memory usage\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -trdebug: enables extended checking for memory corruption\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -options_table: dump list of options inputted\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -options_left: dump list of unused options\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -options_left_off: don't dump list of unused options\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -tmp tmpdir: alternative /tmp directory\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -shared_tmp: tmp directory is shared by all processors\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -not_shared_tmp: each processor has seperate tmp directory\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -get_resident_set_size: print memory usage at end of run\n");CHKERRQ(ierr);
#if defined(PETSC_USE_LOG)
    ierr = (*PetscHelpPrintf)(comm," -log[_all _summary]: logging objects and events\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -log_trace [filename]: prints trace of all PETSc calls\n");CHKERRQ(ierr);
#if defined(PETSC_HAVE_MPE)
    ierr = (*PetscHelpPrintf)(comm," -log_mpe: Also create logfile viewable through upshot\n");CHKERRQ(ierr);
#endif
    ierr = (*PetscHelpPrintf)(comm," -log_info <optional filename>: print informative messages about the calculations\n");CHKERRQ(ierr);
#endif
    ierr = (*PetscHelpPrintf)(comm," -v: prints PETSc version number and release date\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -options_file <file>: reads options from file\n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -petsc_sleep n: sleeps n seconds before running program\n");CHKERRQ(ierr);
#if defined(PETSC_HAVE_AMS)
    ierr = (*PetscHelpPrintf)(comm," -ams_publish_objects: \n");CHKERRQ(ierr);
    ierr = (*PetscHelpPrintf)(comm," -ams_publish_stack: \n");CHKERRQ(ierr);
#endif
    ierr = (*PetscHelpPrintf)(comm,"-----------------------------------------------\n");CHKERRQ(ierr);
  }

  /*
      Setup advanced compare feature for allowing comparison to two running PETSc programs
  */
  ierr = OptionsHasName(PETSC_NULL,"-compare",&flg1);CHKERRQ(ierr);
  if (flg1) {
     double tol = 1.e-12;
     ierr = OptionsGetDouble(PETSC_NULL,"-compare",&tol,&flg1);CHKERRQ(ierr); 
     ierr = PetscCompareInitialize(tol);CHKERRQ(ierr);
  }
  ierr = OptionsGetInt(PETSC_NULL,"-petsc_sleep",&i,&flg1);CHKERRQ(ierr);
  if (flg1) {
    ierr = PetscSleep(i);CHKERRQ(ierr);
  }

  ierr = OptionsGetString(PETSC_NULL,"-log_info_exclude",mname,256,&flg1);CHKERRQ(ierr);
  ierr = PetscStrstr(mname,"null",&f);CHKERRQ(ierr);
  if (f) {
    ierr = PLogInfoDeactivateClass(PETSC_NULL);CHKERRQ(ierr);
  }

  PetscFunctionReturn(0);
}


