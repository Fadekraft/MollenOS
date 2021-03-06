/**
 * MollenOS
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
 */

#ifndef __VALI_THREADING_PRIVATE_H__
#define __VALI_THREADING_PRIVATE_H__

#include <os/osdefs.h>

// Forward some structures we need
DECL_STRUCT(MemorySpace);
DECL_STRUCT(SystemProcess);
DECL_STRUCT(SchedulerObject);
DECL_STRUCT(streambuffer);

#ifndef __THREADING_ENTRY
#define __THREADING_ENTRY
typedef void(*ThreadEntry_t)(void*);
#endif

typedef struct ThreadSignal {
    int          Signal;
    void*        Argument;
    unsigned int Flags;
} ThreadSignal_t;

typedef struct ThreadSignals {
    streambuffer_t* Signals;
    _Atomic(int)    Pending;
    unsigned int    Mask;
} ThreadSignals_t;

typedef struct Thread {
    MemorySpace_t*          MemorySpace;
    UUId_t                  MemorySpaceHandle;
    SchedulerObject_t*      SchedulerObject;
    Context_t*              ContextActive;

    UUId_t                  Handle;
    UUId_t                  ParentHandle;

    Mutex_t                 SyncObject;
    Semaphore_t             EventObject;
    _Atomic(int)            References;
    clock_t                 StartedAt;
    struct Thread*          Children;
    struct Thread*          Sibling;

    // configuration data
    const char*             Name;
    unsigned int            Flags;
    _Atomic(int)            Cleanup;
    UUId_t                  Cookie;
    ThreadEntry_t           Function;
    void*                   Arguments;
    int                     RetCode;
    size_t                  KernelStackSize;
    size_t                  UserStackSize;

    Context_t*              Contexts[THREADING_NUMCONTEXTS];
    uintptr_t               Data[THREADING_CONFIGDATA_COUNT];

    // signal support
    ThreadSignals_t         Signaling;
} Thread_t;

#endif //__VALI_THREADING_PRIVATE_H__
