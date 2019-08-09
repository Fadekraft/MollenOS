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
#include <inet/local.h>
#include <errno.h>
#include <os/mollenos.h>

int shutdown(int iod, int how)
{
    stdio_handle_t* handle = stdio_handle_get(iod);
    
    if (!handle) {
        _set_errno(EBADF);
        return -1;
    }
    
    if (how <= 0 || how > SHUT_RDWR) {
        _set_errno(EINVAL);
        return -1;
    }
    
    if (handle->object.type != STDIO_HANDLE_SOCKET) {
        _set_errno(ENOTSOCK);
        return -1;
    }
    
    switch (handle->object.data.socket.domain) {
        case AF_LOCAL: {
            // do nothing
        } break;
        
        case AF_INET:
        case AF_INET6: {
            // do nothing
        } break;
        
        default: {
            _set_errno(ENOTSUP);
            return -1;
        }
    };
    
    if (how & SHUT_WR) {
        handle->object.data.socket.flags |= SOCKET_WRITE_DISABLED;
    }
    
    if (how & SHUT_RD) {
        handle->object.data.socket.flags |= SOCKET_READ_DISABLED;
    }
    return 0;
}