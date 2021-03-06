/**
 * MollenOS
 *
 * Copyright 2020, Philip Meulengracht
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
 * Process Manager - Debugger
 *  Contains the implementation of debugging facilities for the process manager which is
 *  invoked once a process crashes
 */

//#define __TRACE

#include <ds/mstring.h>
#include <os/context.h>
#include <ddk/debug.h>
#include <ddk/utils.h>
#include <os/mollenos.h>
#include "../../librt/libds/pe/pe.h"
#include "process.h"
#include "sys_process_service_server.h"
#include "symbols.h"

void
DebuggerInitialize(void)
{
    SymbolInitialize();
}

static OsStatus_t
GetModuleAndOffset(
        _In_  Process_t*   process,
        _In_  uintptr_t    address,
        _Out_ const char** moduleName,
        _Out_ uintptr_t*   moduleBase)
{
    if (address < process->Executable->CodeBase) {
        *moduleBase = process->Executable->VirtualAddress;
        *moduleName = (char*) MStringRaw(process->Executable->Name);
        return OsDoesNotExist;
    }

    // Was it not main executable?
    if (address > (process->Executable->CodeBase + process->Executable->CodeSize)) {
        // Iterate libraries to find the sinner
        if (process->Executable->Libraries != NULL) {
            foreach(i, process->Executable->Libraries) {
                PeExecutable_t* Library = (PeExecutable_t*) i->value;
                if (address >= Library->CodeBase && address < (Library->CodeBase + Library->CodeSize)) {
                    *moduleName = MStringRaw(Library->Name);
                    *moduleBase = Library->VirtualAddress;
                    return OsSuccess;
                }
            }
        }

        return OsDoesNotExist;
    }

    *moduleBase = process->Executable->VirtualAddress;
    *moduleName = (char*) MStringRaw(process->Executable->Name);
    return OsSuccess;
}

static OsStatus_t
HandleProcessCrashReport(
        _In_ Process_t* process,
        _In_ UUId_t     threadHandle,
        _In_ Context_t* crashContext,
        _In_ int        crashReason)
{
    uintptr_t   moduleBase;
    const char* moduleName;
    const char* programName;
    uintptr_t   crashAddress;
    int         i = 0, max = 12;

    if (!process || !crashContext) {
        return OsInvalidParameters;
    }

    crashAddress = CONTEXT_IP(crashContext);
    programName  = MStringRaw(process->Executable->Name);

    TRACE("HandleProcessCrashReport(%i)", crashReason);

    // Debug
    GetModuleAndOffset(process, crashAddress, &moduleName, &moduleBase);
    ERROR("%s: Crashed in module %s, at offset 0x%" PRIxIN " (0x%" PRIxIN ") with reason %i",
          programName, moduleName, crashAddress - moduleBase, crashAddress, crashReason);

    // Print stack trace for application
    if (CONTEXT_USERSP(crashContext)) {
        void*      stack;
        void*      topOfStack;
        OsStatus_t status;

        status = MapThreadMemoryRegion(threadHandle, CONTEXT_USERSP(crashContext), &topOfStack, &stack);
        if (status == OsSuccess) {
            // Traverse the memory region up to stack max
            uintptr_t* stackAddress = (uintptr_t*)stack;
            uintptr_t* stackLimit   = (uintptr_t*)topOfStack;
            ERROR("Stack Trace 0x%llx => 0x%llx", stackAddress, stackLimit);
            while (stackAddress < stackLimit && i < max) {
                uintptr_t stackValue = *stackAddress;
                if (GetModuleAndOffset(process, stackValue, &moduleName, &moduleBase) == OsSuccess) {
                    const char* symbolName;
                    uintptr_t   symbolOffset;

                    if (SymbolLookup(moduleName, stackValue - moduleBase, &symbolName, &symbolOffset) == OsSuccess) {
                        ERROR("%i: %s+%x in module %s", i, symbolName, symbolOffset, moduleName);
                    }
                    else {
                        ERROR("%i: At offset 0x%" PRIxIN " in module %s (0x%" PRIxIN ")",
                              i, stackValue - moduleBase, moduleName, stackValue);
                    }

                    i++;
                }
                stackAddress++;
            }
            ERROR("HandleProcessCrashReport end of stack trace");
            MemoryFree(stack, (uintptr_t)topOfStack - (uintptr_t)stack);
        }
        else {
            ERROR("HandleProcessCrashReport failed to map thread stack: %u", status);
        }
    }
    else {
        ERROR("HandleProcessCrashReport failed to load user stack value: 0x%" PRIxIN, CONTEXT_USERSP(crashContext));
    }

    return OsSuccess;
}

void sys_process_report_crash_invocation(struct gracht_message* message, const UUId_t threadId,
        const UUId_t processId, const uint8_t* crashContext, const uint32_t crashContext_count, const int reason)
{
    Process_t* process = AcquireProcess(processId);
    OsStatus_t status  = HandleProcessCrashReport(process, threadId, (Context_t*)crashContext, reason);
    (void)crashContext_count;

    if (process) {
        ReleaseProcess(process);
    }
    sys_process_report_crash_response(message, status);
}
