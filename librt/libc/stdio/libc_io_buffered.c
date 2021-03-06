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
 * C Standard Library
 * - Standard IO Support functions
 */
//#define __TRACE

#include <assert.h>
#include <ddk/utils.h>
#include <ds/collection.h>
#include <internal/_io.h>
#include <io.h>
#include <stdlib.h>

extern Collection_t* stdio_get_handles(void);

void io_buffer_allocate(FILE* stream)
{
    if (!(stream->_flag & _IONBF)) {
        stream->_base = calloc(1, BUFSIZ);
        if (stream->_base) {
            stream->_bufsiz = BUFSIZ;
            stream->_flag |= _IOMYBUF;
        }
        else {
            stream->_flag &= ~(_IONBF | _IOMYBUF | _USERBUF | _IOLBF);
            stream->_flag |= _IONBF;
        }
    }

    // ensure the buffer still set to charbuf
    if (stream->_flag & _IONBF) {
        stream->_base = (char *)(&stream->_charbuf);
        stream->_bufsiz = 2;
    }

    stream->_ptr = stream->_base;
    stream->_cnt = 0;
}

void io_buffer_ensure(FILE* stream)
{
    if (stream->_base) {
        return;
    }
    io_buffer_allocate(stream);
}

OsStatus_t
io_buffer_flush(
    _In_ FILE* file)
{
    if ((file->_flag & (_IOREAD | _IOWRT)) == _IOWRT && file->_flag & (_IOMYBUF | _USERBUF)) {
        int cnt = file->_ptr - file->_base;

        // Flush them
        if (cnt > 0 && write(file->_fd, file->_base, cnt) != cnt) {
            file->_flag |= _IOERR;
            return OsError;
        }

        // If it's rw, clear the write flag
        if (file->_flag & _IORW) {
            file->_flag &= ~_IOWRT;
        }
        file->_ptr = file->_base;
        file->_cnt = 0;
    }
    return OsSuccess;
}

int
io_buffer_flush_all(
    _In_ int mask)
{
    stdio_handle_t*  Object;
    int             FilesFlushes = 0;
    FILE*           File;

    LOCK_FILES();
    foreach(Node, stdio_get_handles()) {
        Object  = (stdio_handle_t*)Node->Data;
        File    = Object->buffered_stream;
        if (File != NULL && (File->_flag & mask)) {
            fflush(File);
            FilesFlushes++;
        }
    }
    UNLOCK_FILES();
    return FilesFlushes;
}

OsStatus_t
_lock_file(
    _In_ FILE *file)
{
    TRACE("_lock_file(0x%" PRIxIN ")", file);
    if (!(file->_flag & _IOSTRG)) {
        assert(stdio_handle_get(file->_fd) != NULL);
        spinlock_acquire(&stdio_handle_get(file->_fd)->lock);
    }
    return OsSuccess;
}

OsStatus_t
_unlock_file(
    _In_ FILE *file)
{
    TRACE("_unlock_file(0x%" PRIxIN ")", file);
    if (!(file->_flag & _IOSTRG)) {
        assert(stdio_handle_get(file->_fd) != NULL);
        spinlock_release(&stdio_handle_get(file->_fd)->lock);
    }
    return OsSuccess;
}
