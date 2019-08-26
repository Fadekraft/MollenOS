/**
 * MollenOS
 *
 * Copyright 2011, Philip Meulengracht
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
 * Kernel Module System
 *   - Implements loading and management of modules that exists on the initrd. 
 */
#define __MODULE "MODS"
//#define __TRACE

#include <arch/interrupts.h>
#include <arch/utils.h>
#include <assert.h>
#include <debug.h>
#include <heap.h>
#include "../../librt/libc/include/internal/_io.h"
#include "../../librt/libds/pe/pe.h"
#include <memoryspace.h>
#include <ds/mstring.h>
#include <threading.h>
#include <string.h>

static Collection_t Modules           = COLLECTION_INIT(KeyInteger);
static int          ModuleIdGenerator = 1;

OsStatus_t
RegisterModule(
    _In_ const char*        Path,
    _In_ const void*        Data,
    _In_ size_t             Length,
    _In_ SystemModuleType_t Type,
    _In_ DevInfo_t          VendorId,
    _In_ DevInfo_t          DeviceId,
    _In_ DevInfo_t          DeviceClass,
    _In_ DevInfo_t          DeviceSubclass)
{
    SystemModule_t* Module;

    // Allocate a new module header and copy some values 
    Module = (SystemModule_t*)kmalloc(sizeof(SystemModule_t));
    memset(Module, 0, sizeof(SystemModule_t));
    Module->ListHeader.Key.Value.Integer = (int)Type;

    Module->Handle = ModuleIdGenerator++;
    Module->Data   = Data;
    Module->Length = Length;
    Module->Path   = MStringCreate("rd:/", StrUTF8);
    MStringAppendCharacters(Module->Path, Path, StrUTF8);

    Module->VendorId        = VendorId;
    Module->DeviceId        = DeviceId;
    Module->DeviceClass     = DeviceClass;
    Module->DeviceSubclass  = DeviceSubclass;
    Module->PrimaryThreadId = UUID_INVALID;
    return CollectionAppend(&Modules, &Module->ListHeader);
}

/* SpawnServices
 * Loads all system services present in the initial ramdisk. */
void
SpawnServices(void)
{
    IntStatus_t IrqState;

    // Disable interrupts while doing this as
    // we are still the idle thread -> as soon as a new
    // work is spawned we hardly ever return to this
    IrqState = InterruptDisable();

    // Iterate module list and spawn all servers
    // then they will "run" the system for us
    foreach(Node, &Modules) {
        if (Node->Key.Value.Integer == (int)ServiceResource) {
            SystemModule_t* Module = (SystemModule_t*)Node;
            OsStatus_t      Status = SpawnModule((SystemModule_t*)Node);
            if (Status != OsSuccess) {
                FATAL(FATAL_SCOPE_KERNEL, "Failed to spawn module %s: %" PRIuIN "", MStringRaw(Module->Path), Status);
            }
        }
    }
    InterruptRestoreState(IrqState);
}

/* GetModuleDataByPath
 * Retrieve a pointer to the file-buffer and its length based on 
 * the given <rd:/> path */
OsStatus_t
GetModuleDataByPath(
    _In_  MString_t* Path, 
    _Out_ void**     Buffer, 
    _Out_ size_t*    Length)
{
    OsStatus_t Result = OsError;
    TRACE("GetModuleDataByPath(%s)", MStringRaw(Path));

    foreach(Node, &Modules) {
        SystemModule_t* Module = (SystemModule_t*)Node;
        if (Module->Path != NULL) {
            TRACE("Comparing(%s)To(%s)", MStringRaw(Path), MStringRaw(Module->Path));
            if (MStringCompare(Path, Module->Path, 1) != MSTRING_NO_MATCH) {
                assert(Module->Data != NULL && Module->Length != 0);
                *Buffer = (void*)Module->Data;
                *Length = Module->Length;
                Result  = OsSuccess;
                break;
            }
        }
    }
    return Result;
}

/* GetGenericDeviceModule
 * Resolves a device module by it's generic class and subclass instead of a device specific
 * module that is resolved by vendor id and product id. */
SystemModule_t*
GetGenericDeviceModule(
    _In_ DevInfo_t DeviceClass, 
    _In_ DevInfo_t DeviceSubclass)
{
    foreach(Node, &Modules) {
        SystemModule_t* Module = (SystemModule_t*)Node;
        if (Module->DeviceClass       == DeviceClass
            && Module->DeviceSubclass == DeviceSubclass) {
            return Module;
        }
    }
    return NULL;
}

/* GetSpecificDeviceModule
 * Resolves a specific device module that is specified by both vendor id and product id. */
SystemModule_t*
GetSpecificDeviceModule(
    _In_ DevInfo_t VendorId,
    _In_ DevInfo_t DeviceId)
{
    if (VendorId == 0) {
        return NULL;
    }
    foreach(Node, &Modules) {
        SystemModule_t* Module = (SystemModule_t*)Node;
        if (Module->VendorId == VendorId && Module->DeviceId == DeviceId) {
            return Module;
        }
    }
    return NULL;
}

/* GetModule
 * Retrieves an existing module instance based on the identification markers. */
SystemModule_t*
GetModule(
    _In_  DevInfo_t VendorId,
    _In_  DevInfo_t DeviceId,
    _In_  DevInfo_t DeviceClass,
    _In_  DevInfo_t DeviceSubclass)
{
    foreach(Node, &Modules) {
        SystemModule_t* Module = (SystemModule_t*)Node;
        if (Module->PrimaryThreadId != UUID_INVALID) {
            // Should we check vendor-id && device-id?
            if (VendorId != 0 && DeviceId != 0) {
                if (Module->VendorId == VendorId && Module->DeviceId == DeviceId) {
                    return Module;
                }
            }

            // Skip all fixed-vendor ids
            if (Module->VendorId != 0xFFEF) {
                if (Module->DeviceClass == DeviceClass && Module->DeviceSubclass == DeviceSubclass) {
                    return Module;
                }
            }
        }
    }
    return NULL;
}

SystemModule_t*
GetCurrentModule(void)
{
    MCoreThread_t* Thread = GetCurrentThreadForCore(ArchGetProcessorCoreId());
    foreach(Node, &Modules) {
        SystemModule_t* Module = (SystemModule_t*)Node;
        if (Module->Executable != NULL && 
            AreMemorySpacesRelated(Module->Executable->MemorySpace, 
                (MemorySpaceHandle_t)Thread->MemorySpace) == OsSuccess) {
            return Module;
        }
    }
    return NULL;
}

/* SetModuleAlias
 * Sets the alias for the currently running module. Only the primary thread is allowed to perform
 * this call. */
OsStatus_t
SetModuleAlias(
    _In_ UUId_t Alias)
{
    SystemModule_t* Module = GetCurrentModule();
    if (Module != NULL) {
        Module->Alias = Alias;
        return OsSuccess;
    }
    return OsInvalidPermissions;
}

/* GetModuleByHandle
 * Retrieves a running service/module by it's registered handle. This is usually done
 * by system services to be contactable by applications. */
SystemModule_t*
GetModuleByHandle(
    _In_ UUId_t Handle)
{
    foreach(Node, &Modules) {
        SystemModule_t* Module = (SystemModule_t*)Node;
        if (Module->Handle == Handle || Module->Alias == Handle) {
            return Module;
        }
    }
    return NULL;
}
