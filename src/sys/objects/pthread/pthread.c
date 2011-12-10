/* The code is active only when the flag PETSC_USE_PTHREAD is set */


#include <petscsys.h>        /*I  "petscsys.h"   I*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#if defined(PETSC_HAVE_SCHED_H)
#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <sched.h>
#endif
#if defined(PETSC_HAVE_PTHREAD_H)
#include <pthread.h>
#endif

#if defined(PETSC_HAVE_SYS_SYSINFO_H)
#include <sys/sysinfo.h>
#endif
#if defined(PETSC_HAVE_UNISTD_H)
#include <unistd.h>
#endif
#if defined(PETSC_HAVE_STDLIB_H)
#include <stdlib.h>
#endif
#if defined(PETSC_HAVE_MALLOC_H)
#include <malloc.h>
#endif
#if defined(PETSC_HAVE_VALGRIND)
#include <valgrind/valgrind.h>
#endif

PetscBool    PetscCheckCoreAffinity    = PETSC_FALSE;
PetscBool    PetscThreadGo         = PETSC_TRUE;
PetscMPIInt  PetscMaxThreads = 2;
pthread_t*   PetscThreadPoint;
int*         ThreadCoreAffinity;

/* Function Pointers */
void*          (*PetscThreadFunc)(void*) = NULL;
PetscErrorCode (*PetscThreadInitialize)(PetscInt) = NULL;
PetscErrorCode (*PetscThreadFinalize)(void) = NULL;
void           (*MainWait)(void) = NULL;
PetscErrorCode (*MainJob)(void* (*pFunc)(void*),void**,PetscInt) = NULL;
/* Tree Thread Pool Functions */
extern void*          PetscThreadFunc_Tree(void*);
extern PetscErrorCode PetscThreadInitialize_Tree(PetscInt);
extern PetscErrorCode PetscThreadFinalize_Tree(void);
extern void           MainWait_Tree(void);
extern PetscErrorCode MainJob_Tree(void* (*pFunc)(void*),void**,PetscInt);
/* Main Thread Pool Functions */
extern void*          PetscThreadFunc_Main(void*);
extern PetscErrorCode PetscThreadInitialize_Main(PetscInt);
extern PetscErrorCode PetscThreadFinalize_Main(void);
extern void           MainWait_Main(void);
extern PetscErrorCode MainJob_Main(void* (*pFunc)(void*),void**,PetscInt);
/* Chain Thread Pool Functions */
extern void*          PetscThreadFunc_Chain(void*);
extern PetscErrorCode PetscThreadInitialize_Chain(PetscInt);
extern PetscErrorCode PetscThreadFinalize_Chain(void);
extern void           MainWait_Chain(void);
extern PetscErrorCode MainJob_Chain(void* (*pFunc)(void*),void**,PetscInt);

/* True Thread Pool Functions */
extern void*          PetscThreadFunc_True(void*);
extern PetscErrorCode PetscThreadInitialize_True(PetscInt);
extern PetscErrorCode PetscThreadFinalize_True(void);
extern void           MainWait_True(void);
extern PetscErrorCode MainJob_True(void* (*pFunc)(void*),void**,PetscInt);
/* NO Thread Pool Function */
PetscErrorCode MainJob_Spawn(void* (*pFunc)(void*),void**,PetscInt);

/* True Thread Pool Functions */
extern void*          PetscThreadFunc_LockFree(void*);
extern PetscErrorCode PetscThreadInitialize_LockFree(PetscInt);
extern PetscErrorCode PetscThreadFinalize_LockFree(void);
extern void           MainWait_LockFree(void);
extern PetscErrorCode MainJob_LockFree(void* (*pFunc)(void*),void**,PetscInt);



void* FuncFinish(void* arg) {
  PetscThreadGo = PETSC_FALSE;
  return(0);
}

PetscErrorCode PetscThreadRun(MPI_Comm Comm,void* (*funcp)(void*),int iTotThreads,pthread_t* ThreadId,void** data) 
{
  PetscErrorCode    ierr;
  PetscInt i;

  PetscFunctionBegin;
  for(i=0; i<iTotThreads; i++) {
    ierr = pthread_create(&ThreadId[i],NULL,funcp,data[i]);
  }
  PetscFunctionReturn(0);
}

PetscErrorCode PetscThreadStop(MPI_Comm Comm,int iTotThreads,pthread_t* ThreadId) 
{
  PetscErrorCode ierr;
  PetscInt i;

  PetscFunctionBegin;
  void* joinstatus;
  for (i=0; i<iTotThreads; i++) {
    ierr = pthread_join(ThreadId[i], &joinstatus);CHKERRQ(ierr);
  }
  PetscFunctionReturn(0);
}

#if defined(PETSC_HAVE_SCHED_CPU_SET_T)
/* Set CPU affinity for the main thread */
void PetscSetMainThreadAffinity(PetscInt icorr)
{
  cpu_set_t mset;
  int ncorr = get_nprocs();

  CPU_ZERO(&mset);
  CPU_SET(icorr%ncorr,&mset);
  sched_setaffinity(0,sizeof(cpu_set_t),&mset);
}

/* Set CPU affinity for individual threads */
void PetscPthreadSetAffinity(PetscInt icorr)
{
  cpu_set_t mset;
  int ncorr = get_nprocs();

  CPU_ZERO(&mset);
  CPU_SET(icorr%ncorr,&mset);
  pthread_setaffinity_np(pthread_self(),sizeof(cpu_set_t),&mset);
}
#endif

/* 
   -----------------------------
     'NO' THREAD POOL FUNCTION 
   -----------------------------
*/
#undef __FUNCT__
#define __FUNCT__ "MainJob_Spawn"
PetscErrorCode MainJob_Spawn(void* (*pFunc)(void*),void** data,PetscInt n) {
  PetscErrorCode ijoberr = 0;

  pthread_t* apThread = (pthread_t*)malloc(n*sizeof(pthread_t));
  PetscThreadPoint = apThread; /* point to same place */
  PetscThreadRun(MPI_COMM_WORLD,pFunc,n,apThread,data);
  PetscThreadStop(MPI_COMM_WORLD,n,apThread); /* ensures that all threads are finished with the job */
  free(apThread);

  return ijoberr;
}

#undef __FUNCT__
#define __FUNCT__ "PetscOptionsCheckInitial_Private_Pthread"
PetscErrorCode PetscOptionsCheckInitial_Private_Pthread(void)
{
  PetscErrorCode ierr;
  PetscBool flg1=PETSC_FALSE;

  PetscFunctionBegin;

  /*
      Determine whether user specified maximum number of threads
   */
  ierr = PetscOptionsGetInt(PETSC_NULL,"-thread_max",&PetscMaxThreads,PETSC_NULL);CHKERRQ(ierr);
  ierr = PetscOptionsHasName(PETSC_NULL,"-main",&flg1);CHKERRQ(ierr);
  if(flg1) {
    PetscInt icorr=0;
    ierr = PetscOptionsGetInt(PETSC_NULL,"-main",&icorr,PETSC_NULL);CHKERRQ(ierr);
#if defined(PETSC_HAVE_SCHED_CPU_SET_T)
    PetscSetMainThreadAffinity(icorr);
#endif
  }

#if defined(PETSC_HAVE_SCHED_CPU_SET_T)
  PetscInt N_CORES;
  N_CORES = get_nprocs();
  ThreadCoreAffinity = (int*)malloc(N_CORES*sizeof(int));
  char tstr[9];
  char tbuf[2];
  PetscInt i;
  strcpy(tstr,"-thread");
  for(i=0;i<PetscMaxThreads;i++) {
    ThreadCoreAffinity[i] = i;
    sprintf(tbuf,"%d",i);
    strcat(tstr,tbuf);
    ierr = PetscOptionsHasName(PETSC_NULL,tstr,&flg1);CHKERRQ(ierr);
    if(flg1) {
      ierr = PetscOptionsGetInt(PETSC_NULL,tstr,&ThreadCoreAffinity[i],PETSC_NULL);CHKERRQ(ierr);
      ThreadCoreAffinity[i] = ThreadCoreAffinity[i]%N_CORES; /* check on the user */
    }
    tstr[7] = '\0';
  }
#endif

  /*
      Determine whether to use thread pool
   */
  ierr = PetscOptionsHasName(PETSC_NULL,"-use_thread_pool",&flg1);CHKERRQ(ierr);
  if (flg1) {
    PetscCheckCoreAffinity = PETSC_TRUE;
    /* get the thread pool type */
    PetscInt ipool = 0;
    const char *choices[4] = {"true","tree","main","chain"};

    ierr = PetscOptionsGetEList(PETSC_NULL,"-use_thread_pool",choices,4,&ipool,PETSC_NULL);CHKERRQ(ierr);
    switch(ipool) {
    case 1:
      PetscThreadFunc       = &PetscThreadFunc_Tree;
      PetscThreadInitialize = &PetscThreadInitialize_Tree;
      PetscThreadFinalize   = &PetscThreadFinalize_Tree;
      MainWait              = &MainWait_Tree;
      MainJob               = &MainJob_Tree;
      PetscInfo1(PETSC_NULL,"Using tree thread pool with %d threads\n",PetscMaxThreads);
      break;
    case 2:
      PetscThreadFunc       = &PetscThreadFunc_Main;
      PetscThreadInitialize = &PetscThreadInitialize_Main;
      PetscThreadFinalize   = &PetscThreadFinalize_Main;
      MainWait              = &MainWait_Main;
      MainJob               = &MainJob_Main;
      PetscInfo1(PETSC_NULL,"Using main thread pool with %d threads\n",PetscMaxThreads);
      break;
    case 3:
      PetscThreadFunc       = &PetscThreadFunc_Chain;
      PetscThreadInitialize = &PetscThreadInitialize_Chain;
      PetscThreadFinalize   = &PetscThreadFinalize_Chain;
      MainWait              = &MainWait_Chain;
      MainJob               = &MainJob_Chain;
      PetscInfo1(PETSC_NULL,"Using chain thread pool with %d threads\n",PetscMaxThreads);
      break;
#if defined(PETSC_HAVE_PTHREAD_BARRIER_T)
    default:
      PetscThreadFunc       = &PetscThreadFunc_True;
      PetscThreadInitialize = &PetscThreadInitialize_True;
      PetscThreadFinalize   = &PetscThreadFinalize_True;
      MainWait              = &MainWait_True;
      MainJob               = &MainJob_True;
      PetscInfo1(PETSC_NULL,"Using true thread pool with %d threads\n",PetscMaxThreads);
      break;
# else
    default:
      PetscThreadFunc       = &PetscThreadFunc_Chain;
      PetscThreadInitialize = &PetscThreadInitialize_Chain;
      PetscThreadFinalize   = &PetscThreadFinalize_Chain;
      MainWait              = &MainWait_Chain;
      MainJob               = &MainJob_Chain;
      PetscInfo1(PETSC_NULL,"Cannot use true thread pool since pthread_barrier_t is not available,using chain thread pool with %d threads instead\n",PetscMaxThreads);
      break;
#endif
    }
  } else {
    ierr = PetscOptionsHasName(PETSC_NULL,"-use_lock_free",&flg1);CHKERRQ(ierr);
    if (flg1) {
      PetscCheckCoreAffinity = PETSC_TRUE;
      PetscThreadFunc       = &PetscThreadFunc_LockFree;
      PetscThreadInitialize = &PetscThreadInitialize_LockFree;
      PetscThreadFinalize   = &PetscThreadFinalize_LockFree;
      MainWait              = &MainWait_LockFree;
      MainJob               = &MainJob_LockFree;
    } else {
      /* need to define these in the case on 'no threads' or 'thread create/destroy'
	 could take any of the above versions 
      */
      MainJob               = &MainJob_Spawn;
      PetscInfo1(PETSC_NULL,"Using No thread pool with %d threads\n",PetscMaxThreads);
    }
  }

  PetscFunctionReturn(0);
}
