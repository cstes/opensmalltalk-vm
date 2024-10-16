/****************************************************************************
*   PROJECT: Squeak port for Win32 (NT / Win95)
*   FILE:    sqWin32Alloc.c
*   CONTENT: Virtual Memory Management
*
*   AUTHOR:  Andreas Raab (ar)
*   ADDRESS: University of Magdeburg, Germany
*   EMAIL:   raab@isg.cs.uni-magdeburg.de
*
*****************************************************************************/
#include <Windows.h>
#include "sq.h"

#if !SPURVM /* Spur uses sqWin32SpurAlloc.c */

/* For Qwaq Forums: Disallow memory shrinking to avoid crashes
   due to GC/OpenGL relocation problems within glDrawElements.
   It appears that in rare circumstances we trigger a full GC
   which moves the data away from under OGLs feet and if the
   memory gets released at this point OGL may crash.
*/
#define DO_NOT_SHRINK


static LPSTR  pageBase;     /* base address of allocated memory */
static DWORD  pageMask;     /* bit mask for the start of a memory page */
static DWORD  pageSize;     /* size of a memory page */
static DWORD  nowReserved;  /* 'publicly' reserved virtual memory */
static LPSTR  pageLimit;    /* upper limit of commited pages */
static DWORD  maxReserved;  /* maximum reserved virtual memory */
static DWORD  usedMemory;   /* amount of memory currently in use */

# define roundDownToPage(v) ((sqIntptr_t)(v)&pageMask)
# define roundUpToPage(v) (((sqIntptr_t)(v)+pageSize-1)&pageMask)

static void
initPageSize()
{
  SYSTEM_INFO sysInfo;

  /* determine page boundaries & available address space */
  if (!pageSize) {
    GetSystemInfo(&sysInfo);
    pageSize = sysInfo.dwPageSize;
    pageMask = ~(pageSize - 1);
  }
}


/************************************************************************/
/* sqAllocateMemory: Initialize virtual memory                          */
/************************************************************************/
void *sqAllocateMemory(usqInt minHeapSize, usqInt desiredHeapSize)
{ SYSTEM_INFO sysInfo;
  DWORD initialCommit, commit;

  /* determine page boundaries */
  initPageSize();

  /* round the requested size up to the next page boundary */
  nowReserved = roundUpToPage(desiredHeapSize);

  /* round the initial commited size up to the next page boundary */
  initialCommit = roundUpToPage(minHeapSize);

  /* Here, we only reserve the maximum memory to be used
     It will later be committed during actual access */
  maxReserved = MAX_VIRTUAL_MEMORY;
  do {
    pageBase = VirtualAlloc(NULL,maxReserved,MEM_RESERVE, PAGE_NOACCESS);
    if (!pageBase) {
      if (maxReserved == nowReserved) break;
      /* make it smaller in steps of 128MB */
      maxReserved -= 128*1024*1024;
      if (maxReserved < nowReserved) maxReserved = nowReserved;
    }
  } while (!pageBase);
  if (!pageBase) {
    sqMessageBox(MB_OK | MB_ICONSTOP, TEXT("VM Error:"),
		 TEXT("Unable to allocate memory (%d bytes requested)"),
		 maxReserved);
    return pageBase;
  }
  /* commit initial memory as requested */
  commit = nowReserved;
  if (!VirtualAlloc(pageBase, commit, MEM_COMMIT, PAGE_READWRITE)) {
    sqMessageBox(MB_OK | MB_ICONSTOP, TEXT("VM Error:"),
		 TEXT("Unable to commit memory (%d bytes requested)"),
		 commit);
    return NULL;
  }
  pageLimit = pageBase + commit;
  usedMemory += commit;
  return pageBase;
}

/************************************************************************/
/* sqGrowMemoryBy: Grow object memory if possible                       */
/************************************************************************/
int sqGrowMemoryBy(int oldLimit, int delta) {
  /* round delta UP to page size */
  if (fShowAllocations) {
    warnPrintf("Growing memory by %d...", delta);
  }
  delta = (delta + pageSize) & pageMask;
  if (!VirtualAlloc(pageLimit, delta, MEM_COMMIT, PAGE_READWRITE)) {
    if (fShowAllocations) {
      warnPrintf("failed\n");
    }
    /* failed to grow */
    return oldLimit;
  }
  /* otherwise, expand pageLimit and return new top limit */
  if (fShowAllocations) {
    warnPrintf("okay\n");
  }
  pageLimit += delta;
  usedMemory += delta;
  return (int)pageLimit;
}

/************************************************************************/
/* sqShrinkMemoryBy: Shrink object memory if possible                   */
/************************************************************************/
int sqShrinkMemoryBy(int oldLimit, int delta) {
  /* round delta DOWN to page size */
  if (fShowAllocations) {
    warnPrintf("Shrinking by %d...",delta);
  }
#ifdef DO_NOT_SHRINK
  {
    /* Experimental - do not unmap memory and avoid OGL crashes */
    if (fShowAllocations) warnPrintf(" - ignored\n");
    return oldLimit;
  }
#endif
  delta &= pageMask;
  if (!VirtualFree(pageLimit-delta, delta, MEM_DECOMMIT)) {
    if (fShowAllocations) {
      warnPrintf("failed\n");
    }
    /* failed to shrink */
    return oldLimit;
  }
  /* otherwise, shrink pageLimit and return new top limit */
  if (fShowAllocations) {
    warnPrintf("okay\n");
  }
  pageLimit -= delta;
  usedMemory -= delta;
  return (int)pageLimit;
}

/************************************************************************/
/* sqMemoryExtraBytesLeft: Return memory available to Squeak            */
/************************************************************************/
int sqMemoryExtraBytesLeft(int includingSwap) {
  DWORD bytesLeft;
  MEMORYSTATUS mStat;

  ZeroMemory(&mStat,sizeof(mStat));
  mStat.dwLength = sizeof(mStat);
  GlobalMemoryStatus(&mStat);
  bytesLeft = mStat.dwAvailPhys;
  if (includingSwap) {
    bytesLeft += mStat.dwAvailPageFile;
  };
  /* max bytes is also limited by maxReserved page size */
  if (bytesLeft > (maxReserved - usedMemory)) {
    bytesLeft = maxReserved - usedMemory;
  }
  return bytesLeft;
}

# if COGVM
void
sqMakeMemoryExecutableFromToCodeToDataDelta(usqInt startAddr,
											usqInt endAddr,
											sqInt *codeToDataDelta)
{
	DWORD previous;
  SIZE_T size;

  size = endAddr - startAddr;
	if (!VirtualProtect((void *)startAddr,
						size,
						PAGE_EXECUTE_READWRITE,
						&previous))
		perror("VirtualProtect(x,y,PAGE_EXECUTE_READWRITE)");
	if (codeToDataDelta)
		*codeToDataDelta = 0;
}

void *
allocateJITMemory(usqInt *desiredSize)
{
	initPageSize();

  sqInt allocBytes = roundUpToPage(*desiredSize);

  /* Allocate extra memory for the JIT. No need to make it executable (i.e., PAGE_EXECUTE_READWRITE) right away because there will be an extra call to sqMakeMemoryExecutableFromToCodeToDataDelta(..) anyway. */
	char *alloc = VirtualAlloc(NULL,allocBytes,MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE);
  
	if (!alloc) {
		perror("Could not allocate JIT memory");
		exit(1);
	}

  *desiredSize = allocBytes;
	return alloc;
}
# endif /* COGVM */
#endif /* !SPURVM */
