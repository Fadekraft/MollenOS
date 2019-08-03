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
 * System call interface - IPC implementation
 *
 */
#define __MODULE "SCIF"
#define __TRACE

#include <arch/utils.h>
#include <ddk/ipc/ipc.h>
#include <debug.h>
#include <futex.h>
#include <handle.h>
#include <os/input.h>
#include <machine.h>
#include <modules/manager.h>
#include <modules/module.h>
#include <pipe.h>
#include <threading.h>

OsStatus_t
ScReplyIpc(void)
{
    return OsSuccess;
}

OsStatus_t
ScWaitIpc(void)
{
    return OsSuccess;
}

OsStatus_t
ScReplyIpcAndWait(void)
{
    OsStatus_t Status = ScReplyIpc();
    return ScWaitIpc();
}

OsStatus_t
ScGetIpcResponse(void)
{
    MCoreThread_t* Current = GetCurrentThreadForCore(ArchGetProcessorCoreId());
    return OsSuccess;
}

OsStatus_t
ScInvokeIpc(
    _In_ UUId_t        TargetHandle,
    _In_ IpcMessage_t* Message,
    _In_ int           Async,
    _In_ size_t        Timeout)
{
    MCoreThread_t* Target      = LookupHandle(TargetHandle);
    IpcArena_t*    IpcArena    = Target->IpcArena;
    size_t         BufferIndex = 0;
    int            SyncValue;
    int            i;
    
    SyncValue = atomic_exchange(&IpcArena->WriteSyncObject, 1);
    while (SyncValue) {
        if (FutexWait(&IpcArena->WriteSyncObject, SyncValue, 0, Timeout) == OsTimeout) {
            return OsTimeout;
        }
        SyncValue = atomic_exchange(&IpcArena->WriteSyncObject, 1);
    }
    
    IpcArena->SenderHandle = GetCurrentThreadId();
    for (i = 0; i < IPC_MAX_ARGUMENTS; i++) {
        // Handle typed argument
        IpcArena->Message.TypedArguments[i]          = Message->TypedArguments[i];
        IpcArena->Message.UntypedArguments[i].Length = Message->UntypedArguments[i].Length;
        
        // Handle the untyped, a bit more tricky. If the argument is larger than 512
        // bytes, we will do a mapping clone instead of just copying data into sender.
        if (Message->UntypedArguments[i].Length) {
            if (Message->UntypedArguments[i].Length > 512) {
                // perform clone
                // clone_map(GetCurrentThread(), Target, typed_arg[i].buffer, typed_arg[i].size)
            }
            else {
                memcpy(&IpcArena->Buffer[BufferIndex], 
                    Message->UntypedArguments[i].Buffer,
                    Message->UntypedArguments[i].Length);
                IpcArena->Message.UntypedArguments[i].Buffer = (void*)BufferIndex;
                BufferIndex += Message->UntypedArguments[i].Length;
            }
        }
    }
    
    if (Async) {
        (void)FutexWake(&IpcArena->WriteSyncObject, 1, 0);
    }
    else {
        // AssumeThread
    }
    
    return OsSuccess;
}


OsStatus_t
ScCreatePipe( 
    _In_  int     Type,
    _Out_ UUId_t* Handle)
{
    SystemPipe_t* Pipe;

    if (Type == PIPE_RAW) {
        Pipe = CreateSystemPipe(0, PIPE_DEFAULT_ENTRYCOUNT);
    }
    else if (Type == PIPE_STRUCTURED) {
        Pipe = CreateSystemPipe(PIPE_MPMC | PIPE_STRUCTURED_BUFFER, PIPE_DEFAULT_ENTRYCOUNT);
    }
    else {
        return OsInvalidParameters;
    }
    *Handle = CreateHandle(HandleTypePipe, 0, DestroySystemPipe, Pipe);
    return OsSuccess;
}

OsStatus_t
ScDestroyPipe(
    _In_ UUId_t Handle)
{
    SystemPipe_t* Pipe = (SystemPipe_t*)LookupHandle(Handle);

    if (Pipe != NULL) {
        DestroyHandle(Handle);
        
        // Disable the window manager input event pipe
        if (GetMachine()->StdInput == Pipe) {
            GetMachine()->StdInput = NULL;
        }

        // Disable the window manager wm event pipe
        if (GetMachine()->WmInput == Pipe) {
            GetMachine()->WmInput = NULL;
        }
        return OsSuccess;
    }
    return OsDoesNotExist;
}

OsStatus_t
ScReadPipe(
    _In_ UUId_t   Handle,
    _In_ uint8_t* Message,
    _In_ size_t   Length)
{
    SystemPipe_t* Pipe = (SystemPipe_t*)LookupHandle(Handle);
    if (Pipe == NULL) {
        ERROR("Thread %s trying to read from non-existing pipe handle %" PRIuIN "", 
            GetCurrentThreadForCore(ArchGetProcessorCoreId())->Name, Handle);
        return OsDoesNotExist;
    }

    if (Length != 0) {
        ReadSystemPipe(Pipe, Message, Length);
    }
    return OsSuccess;
}

OsStatus_t
ScWritePipe(
    _In_ UUId_t   Handle,
    _In_ uint8_t* Message,
    _In_ size_t   Length)
{
    SystemPipe_t* Pipe = (SystemPipe_t*)LookupHandle(Handle);
    if (Message == NULL || Length == 0) {
        return OsInvalidParameters;
    }

    if (Pipe == NULL) {
        ERROR("%s: ScPipeWrite::Invalid pipe %" PRIuIN "", 
            GetCurrentThreadForCore(ArchGetProcessorCoreId())->Name, Handle);
        return OsDoesNotExist;
    }
    WriteSystemPipe(Pipe, Message, Length);
    return OsSuccess;
}

OsStatus_t
ScRpcResponse(
    _In_ MRemoteCall_t* RemoteCall)
{
    SystemPipe_t* Pipe = GetCurrentThreadForCore(ArchGetProcessorCoreId())->Pipe;
    assert(Pipe != NULL);
    assert(RemoteCall != NULL);
    assert(RemoteCall->Result.Length > 0);
    //TRACE("ScRpcResponse(Message %" PRIiIN ", %" PRIuIN ")", RemoteCall->Function, RemoteCall->Result.Length);

    // Read up to <Length> bytes, this results in the next 1 .. Length
    // being read from the raw-pipe.
    RemoteCall->Result.Length = ReadSystemPipe(Pipe, 
        (uint8_t*)RemoteCall->Result.Data.Buffer, RemoteCall->Result.Length);
    return OsSuccess;
}

OsStatus_t
ScRpcExecute(
    _In_ MRemoteCall_t* RemoteCall,
    _In_ int            Async)
{
    SystemPipeUserState_t State;
    size_t                TotalLength = sizeof(MRemoteCall_t);
    MCoreThread_t*        Thread;
    SystemModule_t*       Module;
    SystemPipe_t*         Pipe;
    int                   i;
    assert(RemoteCall != NULL);
    
    Module = (SystemModule_t*)GetModuleByHandle(RemoteCall->Target);
    if (Module == NULL) {
        Pipe = (SystemPipe_t*)LookupHandle(RemoteCall->Target);
        if (Pipe == NULL) {
            return OsDoesNotExist;
        }
        if (!(Pipe->Configuration & PIPE_STRUCTURED_BUFFER)) {
            return OsInvalidParameters;
        }
    }
    else {
        if (Module->Rpc == NULL) {
            ERROR("RPC-Target %" PRIuIN " did exist, but was not running", RemoteCall->Target);
            return OsInvalidParameters;
        }
        Pipe = Module->Rpc;
    }

    // Calculate how much data to be comitted
    for (i = 0; i < IPC_MAX_ARGUMENTS; i++) {
        if (RemoteCall->Arguments[i].Type == ARGUMENT_BUFFER) {
            TotalLength += RemoteCall->Arguments[i].Length;
        }
    }

    // Decrypt the sender for the receiver
    Thread = GetCurrentThreadForCore(ArchGetProcessorCoreId());
    RemoteCall->From.Process ^= Thread->Cookie;
    RemoteCall->From.Thread   = Thread->Handle;

    // Setup producer access
    AcquireSystemPipeProduction(Pipe, TotalLength, &State);
    WriteSystemPipeProduction(&State, (const uint8_t*)RemoteCall, sizeof(MRemoteCall_t));
    for (i = 0; i < IPC_MAX_ARGUMENTS; i++) {
        if (RemoteCall->Arguments[i].Type == ARGUMENT_BUFFER) {
            WriteSystemPipeProduction(&State, 
                (const uint8_t*)RemoteCall->Arguments[i].Data.Buffer,
                RemoteCall->Arguments[i].Length);
        }
    }
    if (Async) {
        return OsSuccess;
    }
    return ScRpcResponse(RemoteCall);
}

OsStatus_t
ScRpcListen(
    _In_ UUId_t         Handle,
    _In_ MRemoteCall_t* RemoteCall,
    _In_ uint8_t*       ArgumentBuffer)
{
    SystemPipeUserState_t State;
    uint8_t*              BufferPointer = ArgumentBuffer;
    SystemModule_t*       Module;
    SystemPipe_t*         Pipe;
    size_t                Length;
    int                   i;
    
    assert(RemoteCall != NULL);
    
    // Start out by resolving both the process and pipe
    if (Handle == UUID_INVALID) {
        Module = GetCurrentModule();
        if (Module == NULL) {
            return OsInvalidPermissions;
        }
        Pipe = Module->Rpc;
    }
    else {
        Pipe = (SystemPipe_t*)LookupHandle(Handle);
        if (Pipe == NULL) {
            return OsDoesNotExist;
        }
        if (!(Pipe->Configuration & PIPE_STRUCTURED_BUFFER)) {
            return OsInvalidParameters;
        }
    }

    // Get in queue for pipe entry
    AcquireSystemPipeConsumption(Pipe, &Length, &State);
    ReadSystemPipeConsumption(&State, (uint8_t*)RemoteCall, sizeof(MRemoteCall_t));
    for (i = 0; i < IPC_MAX_ARGUMENTS; i++) {
        if (RemoteCall->Arguments[i].Type == ARGUMENT_BUFFER) {
            RemoteCall->Arguments[i].Data.Buffer = (const void*)BufferPointer;
            ReadSystemPipeConsumption(&State, BufferPointer, RemoteCall->Arguments[i].Length);
            BufferPointer += RemoteCall->Arguments[i].Length;
        }
    }
    FinalizeSystemPipeConsumption(Pipe, &State);
    return OsSuccess;
}

OsStatus_t
ScRpcRespond(
    _In_ MRemoteCallAddress_t* RemoteAddress,
    _In_ const uint8_t*        Buffer, 
    _In_ size_t                Length)
{
    MCoreThread_t* Thread = (MCoreThread_t*)LookupHandle(RemoteAddress->Thread);
    if (Thread) {
        if (Thread->Pipe) {
            WriteSystemPipe(Thread->Pipe, Buffer, Length);
            return OsSuccess;
        }
    }
    ERROR("Thread %" PRIuIN " did not exist", RemoteAddress->Thread);
    return OsDoesNotExist;
}
