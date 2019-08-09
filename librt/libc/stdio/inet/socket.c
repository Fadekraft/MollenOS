/**
 * MollenOS
 *
 * Copyright 2019, Philip Meulengracht
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
 * Standard C Support
 * - Standard Socket IO Implementation
 */

#include <internal/_io.h>
#include <internal/_syscalls.h>
#include <os/mollenos.h>
#include <stdlib.h>

int socket(int domain, int type, int protocol)
{
    stdio_handle_t* io_object;
    OsStatus_t      status;
    UUId_t          handle;
    int             fd;
    
    fd = stdio_handle_create(-1, WX_OPEN | WX_PIPE, &io_object);
    if (fd == -1) {
        return -1;
    }
    
    // We need to create the socket object at kernel level, as we need
    // kernel assisted functionality to support a centralized storage of
    // all system sockets. They are the foundation of the microkernel for
    // communication between processes and are needed long before anything else.
    status = Syscall_CreateSocket(0, &handle, &io_object->object.data.socket.recv_queue);
    if (status != OsSuccess) {
        (void)OsStatusToErrno(status);
        stdio_handle_destroy(io_object, 0);
        return -1;
    }
    
    // We have been given a recieve queue that has been mapped into our
    // own address space for quick reading when bytes available.
    stdio_handle_set_handle(io_object, handle);
    stdio_handle_set_ops_type(io_object, STDIO_HANDLE_SOCKET);
    
    switch (domain) {
        case AF_LOCAL: {
            get_socket_ops_local(&io_object->object.data.socket.domain_ops);
        } break;
        
        case AF_INET:
        case AF_INET6: {
            get_socket_ops_inet(&io_object->object.data.socket.domain_ops);
        } break;
        
        default: {
            get_socket_ops_null(&io_object->object.data.socket.domain_ops);
        } break;
    }
    
    io_object->object.data.socket.domain   = domain;
    io_object->object.data.socket.type     = type;
    io_object->object.data.socket.protocol = protocol;
    return fd;
}