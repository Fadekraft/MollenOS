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
 * MollenOS IO Space Interface
 * - Contains the shared kernel io space interface
 *   that all sub-layers / architectures must conform to
 */
#define __MODULE        "DVIO"
//#define __TRACE

#include <ds/collection.h>
#include <modules/manager.h>
#include <arch/utils.h>
#include <arch/io.h>
#include <memoryspace.h>
#include <threading.h>
#include <deviceio.h>
#include <string.h>
#include <assert.h>
#include <debug.h>
#include <heap.h>

static Collection_t IoSpaces              = COLLECTION_INIT(KeyId);
static _Atomic(UUId_t) IoSpaceIdGenerator = 1;

OsStatus_t
RegisterSystemDeviceIo(
    _In_ DeviceIo_t* IoSpace)
{
    SystemDeviceIo_t* SystemIo;
    TRACE("RegisterSystemDeviceIo(Type %" PRIuIN ")", IoSpace->Type);

    // Before doing anything, we should do a over-lap
    // check before trying to register this

    // Allocate a new system only copy of the io-space
    // as we don't want anyone to edit our copy
    SystemIo = (SystemDeviceIo_t*)kmalloc(sizeof(SystemDeviceIo_t));
    memset(SystemIo, 0, sizeof(SystemDeviceIo_t));
    SystemIo->Owner = UUID_INVALID;

    IoSpace->Id = atomic_fetch_add(&IoSpaceIdGenerator, 1);
    memcpy(&SystemIo->Io, IoSpace, sizeof(DeviceIo_t));
    SystemIo->Header.Key.Value.Id = IoSpace->Id;
    return CollectionAppend(&IoSpaces, &SystemIo->Header);
}

OsStatus_t
DestroySystemDeviceIo(
    _In_ DeviceIo_t* IoSpace)
{
    SystemDeviceIo_t* SystemIo;
    DataKey_t Key;

    // Debugging
    TRACE("DestroySystemDeviceIo(Id %" PRIuIN ")", IoSpace);

    // Lookup the system copy to validate
    Key.Value.Id    = IoSpace->Id;
    SystemIo        = (SystemDeviceIo_t*)CollectionGetNodeByKey(&IoSpaces, Key, 0);
    if (SystemIo == NULL || SystemIo->Owner != UUID_INVALID) {
        return OsError;
    }
    CollectionRemoveByNode(&IoSpaces, &SystemIo->Header);
    kfree(SystemIo);
    return OsSuccess;
}

OsStatus_t
AcquireSystemDeviceIo(
    _In_ DeviceIo_t* IoSpace)
{
    SystemDeviceIo_t*    SystemIo;
    SystemMemorySpace_t* Space  = GetCurrentMemorySpace();
    SystemModule_t*      Module = GetCurrentModule();
    UUId_t               CoreId = ArchGetProcessorCoreId();
    DataKey_t            Key;
    assert(IoSpace != NULL);

    TRACE("AcquireSystemDeviceIo(Id %" PRIuIN ")", IoSpace->Id);

    // Lookup the system copy to validate this requested operation
    Key.Value.Id    = IoSpace->Id;
    SystemIo        = (SystemDeviceIo_t*)CollectionGetNodeByKey(&IoSpaces, Key, 0);

    // Sanitize the system copy
    if (Module == NULL || SystemIo == NULL || SystemIo->Owner != UUID_INVALID) {
        if (Module == NULL) {
            ERROR(" > non-server process tried to acquire io-space");
        }
        ERROR(" > failed to find the requested io-space, id %" PRIuIN "", IoSpace->Id);
        return OsError;
    }
    SystemIo->Owner = Module->Handle;

    switch (SystemIo->Io.Type) {
        case DeviceIoMemoryBased: {
            uintptr_t MappedAddress;
            uintptr_t BaseAddress = SystemIo->Io.Access.Memory.PhysicalBase;
            size_t    PageSize    = GetMemorySpacePageSize();
            size_t    Length      = SystemIo->Io.Access.Memory.Length + (BaseAddress % PageSize);
            OsStatus_t Status     = CreateMemorySpaceMapping(GetCurrentMemorySpace(),
                &BaseAddress, &MappedAddress, Length, 
                MAPPING_COMMIT | MAPPING_USERSPACE | MAPPING_NOCACHE | MAPPING_PERSISTENT, 
                MAPPING_PHYSICAL_FIXED | MAPPING_VIRTUAL_PROCESS, __MASK);
            if (Status != OsSuccess) {
                ERROR(" > Failed to allocate memory for device io memory");
                SystemIo->Owner = UUID_INVALID;
                return Status;
            }

            // Adjust for offset and store in io copies
            MappedAddress                       += BaseAddress % PageSize;
            IoSpace->Access.Memory.VirtualBase   = MappedAddress;
            SystemIo->MappedAddress              = MappedAddress;
            return OsSuccess;
        } break;

        case DeviceIoPortBased: {
            for (size_t i = 0; i < SystemIo->Io.Access.Port.Length; i++) {
                SetDirectIoAccess(CoreId, Space, ((uint16_t)(SystemIo->Io.Access.Port.Base + i)), 1);
            }
            return OsSuccess;
        } break;

        default:
            ERROR(" > unimplemented device-io type %" PRIuIN "", SystemIo->Io.Type);
            break;
    }
    SystemIo->Owner = UUID_INVALID;
    return OsError;
}

OsStatus_t
ReleaseSystemDeviceIo(
    _In_ DeviceIo_t*    IoSpace)
{
    SystemDeviceIo_t*    SystemIo;
    SystemMemorySpace_t* Space  = GetCurrentMemorySpace();
    SystemModule_t*      Module = GetCurrentModule();
    UUId_t               CoreId = ArchGetProcessorCoreId();
    DataKey_t            Key;
    assert(IoSpace != NULL);

    // Debugging
    TRACE("ReleaseSystemDeviceIo(Id %" PRIuIN ")", IoSpace->Id);

    // Lookup the system copy to validate this requested operation
    Key.Value.Id    = IoSpace->Id;
    SystemIo        = (SystemDeviceIo_t*)CollectionGetNodeByKey(&IoSpaces, Key, 0);

    // Sanitize the system copy and do some security checks
    if (Module == NULL || SystemIo == NULL || 
        SystemIo->Owner != Module->Handle) {
        if (Module == NULL) {
            ERROR(" > non-server process tried to acquire io-space");
        }
        ERROR(" > failed to find the requested io-space, id %" PRIuIN "", IoSpace->Id);
        return OsError;
    }

    switch (SystemIo->Io.Type) {
        case DeviceIoMemoryBased: {
            uintptr_t BaseAddress = SystemIo->Io.Access.Memory.PhysicalBase;
            size_t    PageSize    = GetMemorySpacePageSize();
            size_t    Length      = SystemIo->Io.Access.Memory.Length + (BaseAddress % PageSize);
            assert(Space->Context != NULL);
            RemoveMemorySpaceMapping(Space, SystemIo->MappedAddress, Length);
        } break;

        case DeviceIoPortBased: {
            for (size_t i = 0; i < SystemIo->Io.Access.Port.Length; i++) {
                SetDirectIoAccess(CoreId, Space, ((uint16_t)(SystemIo->Io.Access.Port.Base + i)), 0);
            }
        } break;

        default:
            ERROR(" > unimplemented device-io type %" PRIuIN "", SystemIo->Io.Type);
            break;
    }
    IoSpace->Access.Memory.VirtualBase      = 0;
    SystemIo->MappedAddress                 = 0;
    SystemIo->Owner                         = UUID_INVALID;
    return OsSuccess;
}

OsStatus_t
CreateKernelSystemDeviceIo(
    _In_  DeviceIo_t*  SourceIoSpace,
    _Out_ DeviceIo_t** SystemIoSpace)
{
    SystemDeviceIo_t* SystemIo;
    DataKey_t Key;
    
    Key.Value.Id    = SourceIoSpace->Id;
    SystemIo        = (SystemDeviceIo_t*)CollectionGetNodeByKey(&IoSpaces, Key, 0);
    if (SystemIo == NULL) {
        return OsError;
    }

    switch (SystemIo->Io.Type) {
        case DeviceIoMemoryBased: {
            uintptr_t BaseAddress = SystemIo->Io.Access.Memory.PhysicalBase;
            size_t PageSize       = GetMemorySpacePageSize();
            size_t Length         = SystemIo->Io.Access.Memory.Length + (BaseAddress % PageSize);
            OsStatus_t Status     = CreateMemorySpaceMapping(GetCurrentMemorySpace(),
                &BaseAddress, &SystemIo->Io.Access.Memory.VirtualBase, Length, 
                MAPPING_COMMIT | MAPPING_NOCACHE | MAPPING_PERSISTENT, 
                MAPPING_PHYSICAL_FIXED | MAPPING_VIRTUAL_GLOBAL, __MASK);
            if (Status != OsSuccess) {
                ERROR(" > failed to create mapping");
                return OsError;
            }
        } break;

        // Both port and port/pin are not needed to remap here
        default:
            break;
    }
    *SystemIoSpace = &SystemIo->Io;
    return OsSuccess;
}

OsStatus_t
ReleaseKernelSystemDeviceIo(
    _In_ DeviceIo_t* SystemIoSpace)
{
    SystemDeviceIo_t* SystemIo;
    DataKey_t Key;
    
    Key.Value.Id    = SystemIoSpace->Id;
    SystemIo        = (SystemDeviceIo_t*)CollectionGetNodeByKey(&IoSpaces, Key, 0);
    if (SystemIo == NULL) {
        return OsError;
    }

    switch (SystemIo->Io.Type) {
        case DeviceIoMemoryBased: {
            uintptr_t BaseAddress   = SystemIo->Io.Access.Memory.PhysicalBase;
            size_t PageSize         = GetMemorySpacePageSize();
            size_t Length           = SystemIo->Io.Access.Memory.Length + (BaseAddress % PageSize);
            RemoveMemorySpaceMapping(GetCurrentMemorySpace(), SystemIo->Io.Access.Memory.VirtualBase, Length);
            SystemIo->Io.Access.Memory.VirtualBase = 0;
        } break;

        // Both port and port/pin are not needed to remap here
        default:
            break;
    }
    return OsSuccess;
}

// @interrupt context
uintptr_t
ValidateDeviceIoMemoryAddress(
    _In_ uintptr_t Address)
{
    SystemModule_t* Module = GetCurrentModule();
    TRACE("ValidateDeviceIoMemoryAddress(Process %" PRIuIN ", Address 0x%" PRIxIN ")", Handle, Address);

    if (Module == NULL) {
        return 0;
    }
    
    // Iterate and check each io-space if the process has this mapped in
    foreach(ioNode, &IoSpaces) {
        SystemDeviceIo_t* IoSpace     = (SystemDeviceIo_t*)ioNode;
        uintptr_t         VirtualBase = IoSpace->MappedAddress;

        // Two things has to be true before the io-space
        // is valid, it has to belong to the right process
        // and be in range 
        if (IoSpace->Owner == Module->Handle && IoSpace->Io.Type == DeviceIoMemoryBased &&
            (Address >= VirtualBase && Address < (VirtualBase + IoSpace->Io.Access.Memory.Length))) {
            return IoSpace->Io.Access.Memory.PhysicalBase + (Address - VirtualBase);
        }
    }
    return 0;
}
