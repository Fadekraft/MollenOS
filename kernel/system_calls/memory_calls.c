/* MollenOS
 *
 * Copyright 2017, Philip Meulengracht
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
 * System Calls
 */
#define __MODULE "SCIF"
//#define __TRACE

#include <os/mollenos.h>
#include <ddk/memory.h>

#include <modules/manager.h>
#include <memoryspace.h>
#include <threading.h>
#include <machine.h>
#include <string.h>
#include <handle.h>
#include <debug.h>

OsStatus_t
ScMemoryAllocate(
    _In_      void*   Hint,
    _In_      size_t  Length,
    _In_      Flags_t Flags,
    _Out_     void**  MemoryOut)
{
    OsStatus_t           Status;
    uintptr_t            AllocatedAddress;
    SystemMemorySpace_t* Space          = GetCurrentMemorySpace();
    Flags_t              MemoryFlags    = MAPPING_USERSPACE;
    Flags_t              PlacementFlags = MAPPING_PHYSICAL_DEFAULT | MAPPING_VIRTUAL_PROCESS;
    
    if (!Length || !MemoryOut) {
        return OsInvalidParameters;
    }

    // Convert flags from memory domain to memory space domain
    if (Flags & MEMORY_COMMIT) {
        MemoryFlags |= MAPPING_COMMIT;
    }
    if (Flags & MEMORY_UNCHACHEABLE) {
        MemoryFlags |= MAPPING_NOCACHE;
    }
    if (Flags & MEMORY_LOWFIRST) {
        MemoryFlags |= MAPPING_LOWFIRST;
    }
    if (!(Flags & MEMORY_WRITE)) {
        //MemoryFlags |= MAPPING_READONLY;
    }
    if (Flags & MEMORY_EXECUTABLE) {
        MemoryFlags |= MAPPING_EXECUTABLE;
    }
    
    // Create the actual mappings
    Status = CreateMemorySpaceMapping(Space, &AllocatedAddress, NULL, Length, 
        MemoryFlags, PlacementFlags, __MASK);
    if (Status == OsSuccess) {
        *MemoryOut = (void*)AllocatedAddress;
        
        if ((Flags & (MEMORY_COMMIT | MEMORY_CLEAN)) == (MEMORY_COMMIT | MEMORY_CLEAN)) {
            memset((void*)AllocatedAddress, 0, Length);
        }
    }
    return Status;
}

OsStatus_t 
ScMemoryFree(
    _In_ uintptr_t  Address, 
    _In_ size_t     Size)
{
    SystemMemorySpace_t* Space = GetCurrentMemorySpace();
    if (Address == 0 || Size == 0) {
        return OsInvalidParameters;
    }
    return RemoveMemorySpaceMapping(Space, Address, Size);
}

OsStatus_t
ScMemoryProtect(
    _In_  void*    MemoryPointer,
    _In_  size_t   Length,
    _In_  Flags_t  Flags,
    _Out_ Flags_t* PreviousFlags)
{
    uintptr_t AddressStart = (uintptr_t)MemoryPointer;
    if (MemoryPointer == NULL || Length == 0) {
        return OsSuccess;
    }
    return ChangeMemorySpaceProtection(GetCurrentMemorySpace(), 
        AddressStart, Length, Flags | MAPPING_USERSPACE, PreviousFlags);
}

OsStatus_t
ScMemoryShare(
    _In_  void*   Buffer,
    _In_  size_t  Length,
    _Out_ UUId_t* HandleOut)
{
    if (!Buffer || !Handle || !Length) {
        return OsInvalidParameters;
    }
    return MemoryCreateSharedRegion(Buffer, Length, HandleOut);
}

OsStatus_t
ScMemoryInherit(
    _In_  UUId_t Handle,
    _Out_ void** MemoryOut)
{
    VirtualAddress_t      Memory;
    OsStatus_t            Status;
    SystemSharedRegion_t* Buffer = (SystemSharedRegion_t*)AcquireHandle(Handle);
    if (!Buffer) {
        return OsInvalidParameters;
    }

    Status = CreateMemorySpaceMapping(GetCurrentMemorySpace(), &Memory,
        &Buffer->Pages[0], Buffer->Length, 
        MAPPING_USERSPACE | MAPPING_PERSISTENT | MAPPING_COMMIT,
        MAPPING_PHYSICAL_FIXED | MAPPING_VIRTUAL_PROCESS,
        __MASK);
    if (Status == OsSuccess) {
        *MemoryOut = (void*)Memory;
    }
    return Status;
}

OsStatus_t
ScMemoryGetSharedMetrics(
    _In_      UUId_t     Handle,
    _Out_Opt_ size_t*    LengthOut,
    _Out_Opt_ uintptr_t* VectorOut)
{
    SystemSharedRegion_t* Buffer = (SystemSharedRegion_t*)
        LookupHandleOfType(Handle, HandleTypeMemoryBuffer);
    if (!Buffer) {
        return OsInvalidParameters;
    }
    
    if (LengthOut) {
        *LengthOut = Buffer->BlockCount;
    }
    
    if (VectorOut) {
        memcpy((void*)&VectorOut[0], (void*)&Buffer->Pages[0], sizeof(uintptr_t) * Buffer->PageCount);
    }
    return OsSuccess;
}

OsStatus_t 
ScCreateMemorySpace(
    _In_  Flags_t Flags,
    _Out_ UUId_t* Handle)
{
    SystemModule_t* Module = GetCurrentModule();
    if (Handle == NULL || Module == NULL) {
        if (Module == NULL) {
            return OsInvalidPermissions;
        }
        return OsError;
    }
    return CreateMemorySpace(Flags | MEMORY_SPACE_APPLICATION, Handle);
}

OsStatus_t 
ScGetThreadMemorySpaceHandle(
    _In_  UUId_t  ThreadHandle,
    _Out_ UUId_t* Handle)
{
    MCoreThread_t*  Thread;
    SystemModule_t* Module = GetCurrentModule();
    if (Handle == NULL || Module == NULL) {
        if (Module == NULL) {
            return OsInvalidPermissions;
        }
        return OsError;
    }
    Thread = GetThread(ThreadHandle);
    if (Thread != NULL) {
        *Handle = Thread->MemorySpaceHandle;
        return OsSuccess;
    }
    return OsDoesNotExist;
}

OsStatus_t 
ScCreateMemorySpaceMapping(
    _In_  UUId_t                          Handle,
    _In_  struct MemoryMappingParameters* Parameters,
    _Out_ void**                          AddressOut)
{
    SystemModule_t*      Module         = GetCurrentModule();
    SystemMemorySpace_t* MemorySpace    = (SystemMemorySpace_t*)LookupHandle(Handle);
    Flags_t              RequiredFlags  = MAPPING_COMMIT | MAPPING_USERSPACE;
    Flags_t              PlacementFlags = MAPPING_PHYSICAL_DEFAULT | MAPPING_VIRTUAL_FIXED;
    VirtualAddress_t     CopyPlacement  = 0;
    OsStatus_t           Status;
    if (Parameters == NULL || AddressOut == NULL || Module == NULL) {
        if (Module == NULL) {
            return OsInvalidPermissions;
        }
        return OsInvalidParameters;
    }
    if (MemorySpace == NULL) {
        return OsDoesNotExist;
    }
    
    if (Parameters->Flags & MEMORY_EXECUTABLE) {
        RequiredFlags |= MAPPING_EXECUTABLE;
    }
    if (!(Parameters->Flags & MEMORY_WRITE)) {
        RequiredFlags |= MAPPING_READONLY;
    }

    // Create the original mapping in the memory space passed, with the correct
    // access flags. The copied one must have all kinds of access.
    Status = CreateMemorySpaceMapping(MemorySpace, &Parameters->VirtualAddress, NULL,
        Parameters->Length, RequiredFlags, PlacementFlags, __MASK);
    if (Status != OsSuccess) {
        ERROR("ScCreateMemorySpaceMapping::Failed the create mapping in original space");
        return Status;
    }
    
    // Create a cloned copy in our own memory space, however we will set new placement and
    // access flags
    PlacementFlags = MAPPING_PHYSICAL_DEFAULT | MAPPING_VIRTUAL_PROCESS;
    RequiredFlags  = MAPPING_COMMIT | MAPPING_USERSPACE;
    Status         = CloneMemorySpaceMapping(MemorySpace, GetCurrentMemorySpace(),
        Parameters->VirtualAddress, &CopyPlacement, Parameters->Length, RequiredFlags, PlacementFlags);
    if (Status != OsSuccess) {
        ERROR("ScCreateMemorySpaceMapping::Failed the create mapping in parent space");
        RemoveMemorySpaceMapping(MemorySpace, Parameters->VirtualAddress, Parameters->Length);
    }
    *AddressOut = (void*)CopyPlacement;
    return Status;
}
