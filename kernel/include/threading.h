/* MollenOS
 *
 * Copyright 2015, Philip Meulengracht
 *
 * This program is free software : you can redistribute it and / or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ? , either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Threading Interface
 * - Handles all common threading across architectures
 *   and implements systems like signaling, synchronization and rpc
 * 
 * - Threads are only ever cleaned up if they are in the Cleanup State, to reach this state
 *   someone must have either called Join or Detach (+ reach exit) on the thread
 */

#ifndef __THREADING_H__
#define __THREADING_H__

#include <os/osdefs.h>
#include <os/context.h>
#include <ds/collection.h>
#include <semaphore.h>
#include <mutex.h>
#include <signal.h>
#include <time.h>

// Forward some structures we need
typedef struct _SystemMemorySpace SystemMemorySpace_t;
typedef struct _SystemProcess     SystemProcess_t;
typedef struct _SystemPipe        SystemPipe_t;
typedef struct _SchedulerObject   SchedulerObject_t;

#ifndef __THREADING_ENTRY
#define __THREADING_ENTRY
typedef void(*ThreadEntry_t)(void*);
#endif

#define THREADING_CONTEXT_LEVEL0        0   // Kernel
#define THREADING_CONTEXT_LEVEL1        1   // Application
#define THREADING_CONTEXT_SIGNAL        2   // Signal
#define THREADING_NUMCONTEXTS           3
#define THREADING_CONFIGDATA_COUNT      4

/* MCoreThread::Flags Bit Definitions 
 * The first two bits denode the thread
 * runtime mode, which is either:
 * 0 => Kernel
 * 1 => User
 * 2 => Reserved
 * 3 => Reserved 
 * Bit 3: If it's currently in switch-mode */
#define THREADING_KERNELMODE            0x00000000
#define THREADING_USERMODE              0x00000001
#define THREADING_MODEMASK              0x00000003
#define THREADING_RUNMODE(Flags)        (Flags & THREADING_MODEMASK)

/* MCoreThread::Flags Bit Definitions 
 * The rest of the bits denode special other run-modes */
#define THREADING_KERNELENTRY           0x00000004
#define THREADING_IDLE                  0x00000008
#define THREADING_INHERIT               0x00000010
#define THREADING_TRANSITION_USERMODE   0x10000000

typedef struct {
    _Atomic(int)   Status;
    _Atomic(void*) Information;
} SystemSignal_t;

#define SIGNAL_FREE      0
#define SIGNAL_ALLOCATED 1
#define SIGNAL_PENDING   2

typedef struct _MCoreThread {
    CollectionItem_t        Header;
    Mutex_t                 SyncObject;
    Semaphore_t             EventObject;
    _Atomic(int)            References;
    clock_t                 StartedAt;

    const char*             Name;
    UUId_t                  ParentThreadId;
    Flags_t                 Flags;
    _Atomic(int)            Cleanup;
    UUId_t                  Cookie;
    SchedulerObject_t*      SchedulerObject;

    ThreadEntry_t           Function;
    void*                   Arguments;
    int                     RetCode;
    
    Context_t*              Contexts[THREADING_NUMCONTEXTS];
    Context_t*              ContextActive;
    uintptr_t               Data[THREADING_CONFIGDATA_COUNT];

    SystemPipe_t*           Pipe;
    SystemMemorySpace_t*    MemorySpace;
    UUId_t                  MemorySpaceHandle;
    
    _Atomic(int)            PendingSignals;
    int                     HandlingSignals;
    Context_t*              OriginalContext;
    SystemSignal_t          Signals[NUMSIGNALS];
} MCoreThread_t;

/* ThreadingInitialize
 * Initializes static data and allocates resources. */
KERNELAPI OsStatus_t KERNELABI
ThreadingInitialize(void);

/* ThreadingEnable
 * Enables the threading system for the given cpu calling the function. */
KERNELAPI OsStatus_t KERNELABI
ThreadingEnable(void);

/* CreateThread
 * Creates a new thread that will execute the given function as soon as possible. The 
 * thread can be supplied with arguments, mode and a custom memory space. */
KERNELAPI OsStatus_t KERNELABI
CreateThread(
    _In_  const char*    Name,
    _In_  ThreadEntry_t  Function,
    _In_  void*          Arguments,
    _In_  Flags_t        Flags,
    _In_  UUId_t         MemorySpaceHandle,
    _Out_ UUId_t*        Handle);

/* TerminateThread
 * Marks the thread with the given id for finished, and it will be cleaned up
 * on next switch unless specified. The given exitcode will be stored. */
KERNELAPI OsStatus_t KERNELABI
TerminateThread(
    _In_ UUId_t ThreadId,
    _In_ int    ExitCode,
    _In_ int    TerminateChildren);

/* ThreadingJoinThread
 * Can be used to wait for a thread the return 
 * value of this function is the ret-code of the thread */
KERNELAPI int KERNELABI
ThreadingJoinThread(
    _In_ UUId_t ThreadId);

/* ThreadingDetachThread
 * Detaches a running thread by marking it without parent, this will make
 * sure it runs untill it kills itself. */
KERNELAPI OsStatus_t KERNELABI
ThreadingDetachThread(
    _In_  UUId_t ThreadId);

/* EnterProtectedThreadLevel
 * Initializes non-kernel mode and marks the thread
 * for transitioning, there is no return from this function */
KERNELAPI void KERNELABI
EnterProtectedThreadLevel(void);

/* ThreadingIsCurrentTaskIdle
 * Is the given cpu running it's idle task? */
KERNELAPI int KERNELABI
ThreadingIsCurrentTaskIdle(
    _In_ UUId_t CoreId);

/* ThreadingGetCurrentMode
 * Returns the current run-mode for the current thread on the current cpu */
KERNELAPI Flags_t KERNELABI
ThreadingGetCurrentMode(void);

/* GetCurrentThreadForCore
 * Retrieves the current thread on the given cpu if there is any issues it returns NULL */
KERNELAPI MCoreThread_t* KERNELABI
GetCurrentThreadForCore(
    _In_ UUId_t CoreId);

/* GetCurrentThreadId
 * Retrives the current thread id on the current cpu from the callers perspective */
KERNELAPI UUId_t KERNELABI
GetCurrentThreadId(void);

/* AreThreadsRelated 
 * Returns whether or not the threads are running in same address space context. */
KERNELAPI OsStatus_t KERNELABI
AreThreadsRelated(
    _In_ UUId_t Thread1,
    _In_ UUId_t Thread2);

/* GetThread
 * Lookup thread by the given thread-id, returns NULL if invalid */
KERNELAPI MCoreThread_t* KERNELABI
GetThread(
    _In_ UUId_t ThreadId);

/* ThreadingAdvance
 * This is the thread-switch function and must be be called from the below architecture 
 * to get the next thread to run */
KERNELAPI OsStatus_t KERNELABI
ThreadingAdvance(
    _In_  int     Preemptive,
    _In_  size_t  MillisecondsPassed,
    _Out_ size_t* NextDeadlineOut);

/* DisplayActiveThreads
 * Prints out debugging information about each thread in the system, only active threads */
KERNELAPI void KERNELABI
DisplayActiveThreads(void);

/* SignalQueue
 * Dispatches a signal to a thread in the system from an external 
 * source (i.e another thread). */
KERNELAPI OsStatus_t KERNELABI
SignalQueue(
    _In_ UUId_t ThreadId,
    _In_ int    Signal,
    _In_ void*  Argument);

/* SignalExecute
 * Dispatches a signal to the current thread. This immediately loads the signal
 * context and does not return from this function (i.e an exception). */
KERNELAPI void KERNELABI
SignalExecute(
    _In_ Context_t* Context,
    _In_ int        Signal,
    _In_ void*      Argument);

/* SignalReturn
 * Handles returning from a signal, reloads the original thread context.
 * There is no return from this function. */
void
SignalReturn(
    _In_ Context_t* Context);

/* SignalProcess
 * Processes all queued signals and appends to execution list. */
KERNELAPI void KERNELABI
SignalProcess(
    _In_ MCoreThread_t* Thread);

#endif //!__THREADING_H__
