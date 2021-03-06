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
 * IP-Communication
 * - Implementation of inter-thread communication. 
 */

//#define __TRACE

#include <ddk/barrier.h>
#include <ds/streambuffer.h>
#include <debug.h>
#include <handle.h>
#include <handle_set.h>
#include <heap.h>
#include <ioset.h>
#include <ipc_context.h>
#include <memoryspace.h>
#include <memory_region.h>
#include <threading.h>

typedef struct IpcContext {
    UUId_t          Handle;
    UUId_t          CreatorThreadHandle;
    UUId_t          MemoryRegionHandle;
    streambuffer_t* KernelStream;
} IpcContext_t;

static void
IpcContextDestroy(
    _In_ void* resource)
{
    IpcContext_t* context = resource;
    DestroyHandle(context->MemoryRegionHandle);
    kfree(context);
}

OsStatus_t
IpcContextCreate(
    _In_  size_t  Size,
    _Out_ UUId_t* HandleOut,
    _Out_ void**  UserContextOut)
{
    IpcContext_t* Context;
    OsStatus_t    Status;
    void*         KernelMapping;
    
    if (!HandleOut || !UserContextOut) {
        return OsInvalidParameters;
    }
    
    Context = kmalloc(sizeof(IpcContext_t));
    if (!Context) {
        return OsOutOfMemory;
    }
    
    Context->CreatorThreadHandle = ThreadCurrentHandle();
    
    Status = MemoryRegionCreate(Size, Size, 0, &KernelMapping, UserContextOut,
        &Context->MemoryRegionHandle);
    if (Status != OsSuccess) {
        kfree(Context);
        return Status;
    }
    
    Context->Handle       = CreateHandle(HandleTypeIpcContext, IpcContextDestroy, Context);
    Context->KernelStream = (streambuffer_t*)KernelMapping;
    streambuffer_construct(Context->KernelStream, Size - sizeof(streambuffer_t), 
        STREAMBUFFER_GLOBAL | STREAMBUFFER_MULTIPLE_WRITERS);
    
    *HandleOut = Context->Handle;
    return Status;
}

struct message_state {
    unsigned int base;
    unsigned int state;
};

static OsStatus_t
AllocateMessage(
    _In_  struct ipmsg*          message,
    _In_  size_t                 timeout,
    _In_  struct message_state*  state,
    _Out_ IpcContext_t**         targetContext)
{
    IpcContext_t* ipcContext;
    size_t        bytesAvailable;
    size_t        bytesToAllocate = sizeof(UUId_t) + message->length;
    TRACE("AllocateMessage %u", bytesToAllocate);
    
    if (message->addr->type == IPMSG_ADDRESS_HANDLE) {
        ipcContext = LookupHandleOfType(message->addr->data.handle, HandleTypeIpcContext);
    }
    else {
        UUId_t     handle;
        OsStatus_t osStatus = LookupHandleByPath(message->addr->data.path, &handle);
        if (osStatus != OsSuccess) {
            ERROR("AllocateMessage could not find target path %s", message->addr->data.path);
            return osStatus;
        }

        ipcContext = LookupHandleOfType(handle, HandleTypeIpcContext);
    }
    
    if (!ipcContext) {
        ERROR("AllocateMessage could not find target handle %u", message->addr->data.handle);
        return OsDoesNotExist;
    }

    bytesAvailable = streambuffer_write_packet_start(ipcContext->KernelStream,
                                                     bytesToAllocate, 0,
                                                     &state->base, &state->state);
    if (!bytesAvailable) {
        ERROR("AllocateMessage timeout allocating space for message");
        return OsTimeout;
    }
    
    *targetContext = ipcContext;
    return OsSuccess;
}

static void
WriteMessage(
    _In_ IpcContext_t*         context,
    _In_ struct ipmsg*         message,
    _In_ struct message_state* state)
{
    TRACE("WriteMessage()");
    
    // write the header (senders handle)
    streambuffer_write_packet_data(context->KernelStream, &message->from, sizeof(UUId_t), &state->state);

    // write the actual payload
    streambuffer_write_packet_data(context->KernelStream, (void*)message->payload, message->length, &state->state);
}

static inline void
SendMessage(
    _In_ IpcContext_t*         context,
    _In_ struct ipmsg*         message,
    _In_ struct message_state* state)
{
    size_t bytesToCommit;
    TRACE("SendMessage()");

    bytesToCommit = sizeof(UUId_t) + message->length;
    streambuffer_write_packet_end(context->KernelStream, state->base, bytesToCommit);
    MarkHandle(context->Handle, IOSETIN);
}

OsStatus_t
IpcContextSendMultiple(
    _In_ struct ipmsg** messages,
    _In_ int            messageCount,
    _In_ size_t         timeout)
{
    struct message_state state;
    TRACE("[ipc] [send] count %i, timeout %u", MessageCount, LODWORD(Timeout));
    
    if (!messages || !messageCount) {
        return OsInvalidParameters;
    }
    
    for (int i = 0; i < messageCount; i++) {
        IpcContext_t* targetContext;
        OsStatus_t    status = AllocateMessage(messages[i], timeout, &state, &targetContext);
        if (status != OsSuccess) {
            // todo store status in context and return incomplete
            return OsIncomplete;
        }
        WriteMessage(targetContext, messages[i], &state);
        SendMessage(targetContext, messages[i], &state);
    }
    return OsSuccess;
}
