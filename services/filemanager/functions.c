/**
 * MollenOS
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
 * File Manager Service
 * - Handles all file related services and disk services
 */

//#define __TRACE

#include <ctype.h>
#include <ddk/convert.h>
#include <ddk/utils.h>
#include "include/vfs.h"
#include <os/dmabuf.h>
#include <os/types/file.h>
#include <os/process.h>
#include <stdlib.h>
#include <string.h>

#include "sys_file_service_server.h"

extern MString_t* VfsPathCanonicalize(const char* path);

static OsStatus_t Flush(UUId_t processId, UUId_t handle);

static list_t g_fileHandles = LIST_INIT;
static UUId_t g_nextFileId  = 10000;

static inline int __IsEntryFile(FileSystemEntry_t* entry)
{
    return (entry->Descriptor.Flags & FILE_FLAG_DIRECTORY) == 0 ? 1 : 0;
}

static inline int __IsAccessExclusive(unsigned int access)
{
    // Exclusive read access?
    if ((access & (__FILE_READ_ACCESS | __FILE_READ_SHARE)) == __FILE_READ_ACCESS) {
        return 1;
    }

    // Exclusive write access?
    if ((access & (__FILE_WRITE_ACCESS | __FILE_WRITE_SHARE)) == __FILE_WRITE_ACCESS) {
        return 1;
    }
    return 0;
}

static FileSystem_t* __GetFileSystemFromPath(MString_t* path, MString_t** subPathOut)
{
    element_t* header;
    MString_t* identifier;
    MString_t* subPath;
    int        index;
    TRACE("__GetFileSystemFromPath(path=%s)", MStringRaw(path));

    // To open a new file we need to find the correct
    // filesystem identifier and seperate it from it's absolute path
    index = MStringFind(path, ':', 0);
    if (index == MSTRING_NOT_FOUND) {
        return NULL;
    }

    identifier = MStringSubString(path, 0, index);
    subPath    = MStringSubString(path, index + 2, -1);

    _foreach(header, VfsGetFileSystems()) {
        FileSystem_t* fileSystem = (FileSystem_t*)header->value;
        if (fileSystem->state != FSLoaded) {
            continue;
        }

        if (MStringCompare(identifier, fileSystem->identifier, 1)) {
            MStringDestroy(identifier);

            // set path out and return
            *subPathOut = subPath;
            return fileSystem;
        }
    }

    // clean up, not found
    MStringDestroy(identifier);
    MStringDestroy(subPath);
    return NULL;
}

static OsStatus_t
VfsIsHandleValid(
    _In_  UUId_t                    processId,
    _In_  UUId_t                    handle,
    _In_  unsigned int              requiredAccess,
    _Out_ FileSystemEntryHandle_t** handleOut)
{
    element_t*               header = list_find(&g_fileHandles, (void*)(uintptr_t)handle);
    FileSystemEntryHandle_t* entry;
    TRACE("VfsIsHandleValid(processId=%u, handle=%u, requiredAccess=0x%x)",
          processId, handle, requiredAccess);

    if (!header) {
        ERROR("VfsIsHandleValid not found: %u", handle);
        return OsInvalidParameters;
    }

    entry = (FileSystemEntryHandle_t*)header->value;
    if (entry->Owner != processId) {
        ERROR("VfsIsHandleValid Owner of the handle did not match the requester. Access Denied.");
        return OsInvalidPermissions;
    }

    if (requiredAccess != 0 && (entry->Access & requiredAccess) != requiredAccess) {
        ERROR("VfsIsHandleValid handle was not opened with the required access parameter. Access Denied.");
        return OsInvalidPermissions;
    }

    *handleOut = entry;
    return OsSuccess;
}

OsStatus_t 
VfsOpenHandleInternal(
    _In_  FileSystemEntry_t*        entry,
    _Out_ FileSystemEntryHandle_t** handleOut)
{
    FileSystem_t*            filesystem;
    FileSystemEntryHandle_t* handle;
    OsStatus_t               status;

    TRACE("VfsOpenHandleInternal(entry=0x%" PRIxIN ", handleOut=0x%" PRIxIN ")",
          entry, handleOut);
    if (!entry || !handleOut) {
        return OsInvalidParameters;
    }

    TRACE("VfsOpenHandleInternal opening %s", MStringRaw(entry->Path));
    filesystem = (FileSystem_t*)entry->System;
    status     = filesystem->module->OpenHandle(&filesystem->descriptor, entry, &handle);
    if (status != OsSuccess) {
        ERROR("VfsOpenHandleInternal failed to initiate a new entry-handle, code %i", status);
        return status;
    }

    handle->LastOperation     = __FILE_OPERATION_NONE;
    handle->OutBuffer         = NULL;
    handle->OutBufferPosition = 0;
    handle->Position          = 0;
    handle->Entry             = entry;

    // handle file specific options
    if (__IsEntryFile(entry)) {
        TRACE("VfsOpenHandleInternal entry is a file, validating options 0x%x", handle->Options);

        // Initialise buffering as long as the file
        // handle is not opened as volatile
        if (!(handle->Options & __FILE_VOLATILE)) {
            handle->OutBuffer = malloc(filesystem->descriptor.Disk.descriptor.SectorSize);
            memset(handle->OutBuffer, 0, filesystem->descriptor.Disk.descriptor.SectorSize);
        }

        // Now comes the step where we handle options 
        // - but only options that are handle-specific
        if (handle->Options & __FILE_APPEND) {
            status = filesystem->module->SeekInEntry(&filesystem->descriptor, handle, entry->Descriptor.Size.QuadPart);
        }
    }

    *handleOut = handle;
    return status;
}

OsStatus_t
VfsVerifyAccessToPath(
        _In_  MString_t*          path,
        _In_  unsigned int        options,
        _In_  unsigned int        access,
        _Out_ FileSystemEntry_t** existingEntry)
{
    FileSystemEntry_t* fileEntry;
    element_t*         element;
    OsStatus_t         status;
    TRACE("VfsVerifyAccessToPath(path=%s, options=0x%x, access=0x%x)",
          MStringRaw(path), options, access);

    if (!path || !existingEntry) {
        return OsInvalidParameters;
    }

    status = VfsCacheGetFile(path, options, &fileEntry);
    if (status != OsSuccess) {
        ERROR("VfsVerifyAccessToPath failed to retrieve file: %u", status);
        return status;
    }

    // we should at this point check other handles to see how many have this file
    // opened, and see if there is any other handles using it
    _foreach(element, &g_fileHandles) {
        FileSystemEntryHandle_t* handle = element->value;
        if (handle->Entry == fileEntry) {
            // Are we trying to open the file in exclusive mode?
            if (__IsAccessExclusive(access)) {
                ERROR("VfsVerifyAccessToPath can't get exclusive lock on file, it is already opened");
                return OsInvalidPermissions;
            }

            // Is the file already opened in exclusive mode
            if (__IsAccessExclusive(handle->Access)) {
                ERROR("VfsVerifyAccessToPath can't open file, it is locked");
                return OsInvalidPermissions;
            }
        }
    }

    *existingEntry = fileEntry;
    return OsSuccess;
}

OsStatus_t
VfsOpenFileInternal(
        _In_  MString_t*                path,
        _In_  unsigned int              options,
        _In_  unsigned int              access,
        _Out_ FileSystemEntryHandle_t** handle)
{
    FileSystemEntry_t* entry;
    OsStatus_t         status;

    TRACE("VfsOpenFileInternal(path=%s, options=0x%x, access=0x%x)",
          MStringRaw(path), options, access);

    status = VfsVerifyAccessToPath(path, options, access, &entry);
    if (status == OsSuccess) {
        // Now we can open the handle
        // Open handle Internal takes care of these flags APPEND/VOLATILE/BINARY
        status = VfsOpenHandleInternal(entry, handle);
        if (status == OsSuccess) {
            entry->References++;
        }
    }
    TRACE("VfsOpenFileInternal returns=%u", status);
    return status;
}

OsStatus_t
VfsGuessBasePath(
    _In_ const char* path,
    _In_ char*       result)
{
    char* dot;

    TRACE("VfsGuessBasePath(path=%s)", path);
    if (!path || !result) {
        return OsInvalidParameters;
    }

    dot = strrchr(path, '.');
    if (dot) {
        // Binaries are found in common
        if (!strcmp(dot, ".app") || !strcmp(dot, ".dll")) {
            strcpy(result, "$bin/");
        }
        // Resources are found in system folder
        else {
            strcpy(result, "$sys/");
        }
    }
    // Assume we are looking for folders in system folder
    else {
        strcpy(result, "$sys/");
    }

    TRACE("VfsGuessBasePath returns=%s", result);
    return OsSuccess;
}

MString_t*
VfsResolvePath(
    _In_ UUId_t      processId,
    _In_ const char* path)
{
    MString_t* resolvedPath = NULL;
    OsStatus_t osStatus;

    TRACE("VfsResolvePath(processId=%u, path=%s)", processId, path);

    if (strchr(path, ':') == NULL && strchr(path, '$') == NULL) {
        char* basePath = (char*)malloc(_MAXPATH);
        if (!basePath) {
            ERROR("VfsResolvePath out of memory");
            return NULL;
        }
        memset(basePath, 0, _MAXPATH);

        osStatus = ProcessGetWorkingDirectory(processId, &basePath[0], _MAXPATH);
        if (osStatus != OsSuccess) {
            WARNING("VfsResolvePath failed to get working directory [%u], guessing base path", osStatus);

            osStatus = VfsGuessBasePath(path, &basePath[0]);
            if (osStatus != OsSuccess) {
                ERROR("VfsResolvePath failed to guess the base path: %u", osStatus);
                free(basePath);
                return NULL;
            }
        }
        else {
            strcat(basePath, "/");
        }
        strcat(basePath, path);
        resolvedPath = VfsPathCanonicalize(basePath);

        free(basePath);
    }
    else {
        resolvedPath = VfsPathCanonicalize(path);
    }

    TRACE("VfsResolvePath returns=%s", MStringRaw(resolvedPath));
    return resolvedPath;
}

static OsStatus_t
OpenFile(
    _In_  UUId_t       processId,
    _In_  const char*  path,
    _In_  unsigned int options,
    _In_  unsigned int access,
    _Out_ UUId_t*      handleOut)
{
    FileSystemEntryHandle_t* entry;
    OsStatus_t               status = OsDoesNotExist;
    MString_t*               resolvedPath;
    UUId_t                   id;

    TRACE("OpenFile path %s, options 0x%x, access 0x%x", path, options, access);
    if (!path || !handleOut) {
        return OsInvalidParameters;
    }

    // If path is not absolute or special, we must try the working directory of caller
    resolvedPath = VfsResolvePath(processId, path);
    if (resolvedPath) {
        status = VfsOpenFileInternal(resolvedPath, options, access, &entry);
        MStringDestroy(resolvedPath);
    }

    // Sanitize code
    if (status != OsSuccess) {
        TRACE("OpenFile error opening entry, exited with code: %i", status);
        return status;
    }

    id = g_nextFileId++;

    ELEMENT_INIT(&entry->header, (uintptr_t)id, entry);
    entry->Id      = id;
    entry->Owner   = processId;
    entry->Access  = access;
    entry->Options = options;

    list_append(&g_fileHandles, &entry->header);

    *handleOut = id;
    return status;
}

void sys_file_open_invocation(struct gracht_message* message,
        const UUId_t processId, const char* path,
        const unsigned int options, const unsigned int access)
{
    UUId_t     handle = UUID_INVALID;
    OsStatus_t status;

    TRACE("svc_file_open_callback()");
    status = OpenFile(processId, path, options, access, &handle);
    TRACE("svc_file_open_callback returns=[status %u, handle %u]", status, handle);
    sys_file_open_response(message, status, handle);
}

static OsStatus_t
CloseFile(
    _In_ UUId_t processId,
    _In_ UUId_t handle)
{
    FileSystemEntryHandle_t* entryHandle;
    FileSystemEntry_t*       entry;
    OsStatus_t               status;
    element_t*               header;
    FileSystem_t*            fileSystem;

    TRACE("CloseFile handle %u", handle);

    status = VfsIsHandleValid(processId, handle, 0, &entryHandle);
    if (status != OsSuccess) {
        return status;
    }

    entry = entryHandle->Entry;

    // handle file specific flags
    if (__IsEntryFile(entryHandle->Entry)) {
        // If there has been allocated any buffers they should
        // be flushed and cleaned up 
        if (!(entryHandle->Options & __FILE_VOLATILE)) {
            Flush(processId, handle);
            free(entryHandle->OutBuffer);
        }
    }

    // remove the entry after flushing
    header = list_find(&g_fileHandles, (void*)(uintptr_t)handle);
    if (header) {
        list_remove(&g_fileHandles, header);
    }

    // Call the filesystem close-handle to cleanup
    fileSystem = (FileSystem_t*)entryHandle->Entry->System;
    status     = fileSystem->module->CloseHandle(&fileSystem->descriptor, entryHandle);

    // Take care of any entry cleanup / reduction
    entry->References--;

    return status;
}

void sys_file_close_invocation(struct gracht_message* message, const UUId_t processId, const UUId_t handle)
{
    OsStatus_t status = CloseFile(processId, handle);
    sys_file_close_response(message, status);
}

static OsStatus_t
DeletePath(
    _In_ UUId_t       processId,
    _In_ const char*  path,
    _In_ unsigned int options)
{
    FileSystemEntryHandle_t* entryHandle;
    OsStatus_t               status;
    FileSystem_t*            fileSystem;
    MString_t*               subPath;
    MString_t*               resolvedPath;
    UUId_t                   handle;

    TRACE("VfsDeletePath(Path %s, Options 0x%x)", path, options);
    if (path == NULL) {
        return OsInvalidParameters;
    }

    // If path is not absolute or special, we should ONLY try either
    // the current working directory.
    resolvedPath = VfsResolvePath(processId, path);
    if (resolvedPath == NULL) {
        return OsDoesNotExist;
    }
    
    fileSystem = __GetFileSystemFromPath(resolvedPath, &subPath);
    MStringDestroy(resolvedPath);
    if (fileSystem == NULL) {
        return OsDoesNotExist;
    }

    // First step is to open the path in exclusive mode
    status = OpenFile(processId, path, __FILE_VOLATILE, __FILE_READ_ACCESS | __FILE_WRITE_ACCESS, &handle);
    if (status == OsSuccess) {
        status = VfsIsHandleValid(processId, handle, 0, &entryHandle);
        if (status != OsSuccess) {
            return status;
        }

        status = fileSystem->module->DeleteEntry(&fileSystem->descriptor, entryHandle);
        (void)CloseFile(processId, handle);
    }

    // remove it after closing the handle so we don't keep any references
    if (status == OsSuccess) {
        MString_t* pathAsMstring = MStringCreate(path, StrUTF8);
        VfsCacheRemoveFile(pathAsMstring);
        MStringDestroy(pathAsMstring);
    }
    return status;
}

void sys_file_delete_invocation(struct gracht_message* message,
        const UUId_t processId, const char* path, const unsigned int flags)
{
    OsStatus_t status = DeletePath(processId, path, flags);
    sys_file_delete_response(message, status);
}

static OsStatus_t
ReadFile(
    _In_  UUId_t  processId,
    _In_  UUId_t  handle,
    _In_  UUId_t  bufferHandle,
    _In_  size_t  offset,
    _In_  size_t  length,
    _Out_ size_t* bytesRead)
{
    FileSystemEntryHandle_t* entryHandle;
    OsStatus_t               status;
    FileSystem_t*            fileSystem;
    struct dma_attachment    dmaAttachment;

    TRACE("[vfs_read] pid => %u, id => %u, b_id => %u, len => %u", 
        processId, handle, bufferHandle, LODWORD(length));
    
    if (bufferHandle == UUID_INVALID || length == 0) {
        ERROR("[vfs_read] error invalid parameters, length 0 or invalid b_id");
        return OsInvalidParameters;
    }

    status = VfsIsHandleValid(processId, handle, __FILE_READ_ACCESS, &entryHandle);
    if (status != OsSuccess) {
        return status;
    }

    // Sanity -> Flush if we wrote and now read
    if ((entryHandle->LastOperation != __FILE_OPERATION_READ) && __IsEntryFile(entryHandle->Entry)) {
        (void)Flush(processId, handle);
    }

    status = dma_attach(bufferHandle, &dmaAttachment);
    if (status != OsSuccess) {
        ERROR("[vfs_read] [dma_attach] failed: %u", status);
        return OsInvalidParameters;
    }
    
    status = dma_attachment_map(&dmaAttachment, DMA_ACCESS_WRITE);
    if (status != OsSuccess) {
        ERROR("[vfs_read] [dma_attachment_map] failed: %u", status);
        dma_detach(&dmaAttachment);
        return OsInvalidParameters;
    }

    fileSystem   = (FileSystem_t*)entryHandle->Entry->System;
    status = fileSystem->module->ReadEntry(&fileSystem->descriptor, entryHandle, bufferHandle,
        dmaAttachment.buffer, offset, length, bytesRead);
    if (status == OsSuccess) {
        entryHandle->LastOperation  = __FILE_OPERATION_READ;
        entryHandle->Position       += *bytesRead;
    }
    
    // Unregister the dma buffer
    dma_attachment_unmap(&dmaAttachment);
    dma_detach(&dmaAttachment);
    return status;
}

static OsStatus_t
WriteFile(
    _In_  UUId_t  processId,
    _In_  UUId_t  handle,
    _In_  UUId_t  bufferHandle,
    _In_  size_t  offset,
    _In_  size_t  length,
    _Out_ size_t* bytesWritten)
{
    FileSystemEntryHandle_t* entryHandle;
    OsStatus_t               status;
    FileSystem_t*            fileSystem;
    struct dma_attachment    dmaAttachment;

    TRACE("[vfs_write] pid => %u, id => %u, b_id => %u", processId, handle, bufferHandle);

    if (bufferHandle == UUID_INVALID || length == 0) {
        ERROR("[vfs_write] error invalid parameters, length 0 or invalid b_id");
        return OsInvalidParameters;
    }

    status = VfsIsHandleValid(processId, handle, __FILE_WRITE_ACCESS, &entryHandle);
    if (status != OsSuccess) {
        return status;
    }

    // Sanity -> Clear read buffer if we are writing
    if ((entryHandle->LastOperation != __FILE_OPERATION_WRITE) && __IsEntryFile(entryHandle->Entry)) {
        (void)Flush(processId, handle);
    }

    status = dma_attach(bufferHandle, &dmaAttachment);
    if (status != OsSuccess) {
        ERROR("[vfs_write] [dma_attach] failed: %u", status);
        return OsInvalidParameters;
    }
    
    status = dma_attachment_map(&dmaAttachment, DMA_ACCESS_WRITE);
    if (status != OsSuccess) {
        ERROR("[vfs_write] [dma_attachment_map] failed: %u", status);
        dma_detach(&dmaAttachment);
        return OsInvalidParameters;
    }

    fileSystem   = (FileSystem_t*)entryHandle->Entry->System;
    status = fileSystem->module->WriteEntry(&fileSystem->descriptor, entryHandle, bufferHandle,
        dmaAttachment.buffer, offset, length, bytesWritten);
    if (status == OsSuccess) {
        entryHandle->LastOperation  = __FILE_OPERATION_WRITE;
        entryHandle->Position       += *bytesWritten;
        if (entryHandle->Position > entryHandle->Entry->Descriptor.Size.QuadPart) {
            entryHandle->Entry->Descriptor.Size.QuadPart = entryHandle->Position;
        }
    }
    
    // Unregister the dma buffer
    dma_attachment_unmap(&dmaAttachment);
    dma_detach(&dmaAttachment);
    return status;
}

void sys_file_transfer_invocation(struct gracht_message* message, const UUId_t processId, const UUId_t handle,
        const enum sys_transfer_direction direction, const UUId_t bufferHandle,
        const size_t offset, const size_t length)
{
    size_t     bytesTransferred;
    OsStatus_t status;
    
    if (direction == SYS_TRANSFER_DIRECTION_READ) {
        status = ReadFile(processId, handle, bufferHandle,
                          offset, length, &bytesTransferred);
    }
    else {
        status = WriteFile(processId, handle, bufferHandle,
                           offset, length, &bytesTransferred);
    }
    
    sys_file_transfer_response(message, status, bytesTransferred);
}

static OsStatus_t
Seek(
    _In_ UUId_t   processId,
    _In_ UUId_t   handle, 
    _In_ uint32_t seekLo, 
    _In_ uint32_t seekHi)
{
    FileSystemEntryHandle_t* entryHandle = NULL;
    OsStatus_t               status;
    FileSystem_t*            fileSystem;

    // Combine two u32 to form one big u64 
    // This is just the declaration
    union {
        struct {
            uint32_t Lo;
            uint32_t Hi;
        } Parts;
        uint64_t Full;
    } seekOffsetAbs;
    seekOffsetAbs.Parts.Lo = seekLo;
    seekOffsetAbs.Parts.Hi = seekHi;

    TRACE("Seek(handle %u, seekLo 0x%x, seekHi 0x%x)", handle, seekLo, seekHi);

    status = VfsIsHandleValid(processId, handle, 0, &entryHandle);
    if (status != OsSuccess) {
        return status;
    }

    // Flush buffers before seeking
    if (!(entryHandle->Options & __FILE_VOLATILE) && __IsEntryFile(entryHandle->Entry)) {
        status = Flush(processId, handle);
        if (status != OsSuccess) {
            TRACE("Failed to flush file before seek");
            return status;
        }
    }

    // Perform the seek on a file-system level
    fileSystem = (FileSystem_t*)entryHandle->Entry->System;
    status     = fileSystem->module->SeekInEntry(&fileSystem->descriptor, entryHandle, seekOffsetAbs.Full);
    if (status == OsSuccess) {
        entryHandle->LastOperation      = __FILE_OPERATION_NONE;
        entryHandle->OutBufferPosition  = 0;
    }
    return status;
}

void sys_file_seek_invocation(struct gracht_message* message, const UUId_t processId,
        const UUId_t handle, const unsigned int seekLow, const unsigned int seekHigh)
{
    OsStatus_t status = Seek(processId, handle, seekLow, seekHigh);
    sys_file_seek_response(message, status);
}

void sys_file_transfer_absolute_invocation(struct gracht_message* message, const UUId_t processId,
        const UUId_t handle, const enum sys_transfer_direction direction,
        const unsigned int seekLow, const unsigned int seekHigh,
        const UUId_t bufferHandle, const size_t offset, const size_t length)
{
    size_t     bytesTransferred;
    OsStatus_t status;

    status = Seek(processId, handle, seekLow, seekHigh);
    if (status != OsSuccess) {
        sys_file_transfer_absolute_response(message, status, 0);
    }

    if (direction == SYS_TRANSFER_DIRECTION_READ) {
        status = ReadFile(processId, handle, bufferHandle,
                          offset, length, &bytesTransferred);
    }
    else {
        status = WriteFile(processId, handle, bufferHandle,
                           offset, length, &bytesTransferred);
    }
    sys_file_transfer_absolute_response(message, status, bytesTransferred);
}

static OsStatus_t
Flush(
    _In_ UUId_t processId, 
    _In_ UUId_t handle)
{
    FileSystemEntryHandle_t* entryHandle = NULL;
    OsStatus_t               status;
    //FileSystem_t *fileSystem;

    status = VfsIsHandleValid(processId, handle, 0, &entryHandle);
    if (status != OsSuccess) {
        return status;
    }

    // If no buffering enabled skip, or if not a file skip
    if ((entryHandle->Options & __FILE_VOLATILE) || !__IsEntryFile(entryHandle->Entry)) {
        return OsSuccess;
    }

    // Empty output buffer 
    // - But sanitize the buffers first
    if (entryHandle->OutBuffer != NULL && entryHandle->OutBufferPosition != 0) {
        size_t BytesWritten = 0;
#if 0
        fileSystem = (FileSystem_t*)entryHandle->File->System;
        status     = fileSystem->Module->WriteFile(&fileSystem->Descriptor, entryHandle, NULL, &BytesWritten);
#endif
        if (BytesWritten != entryHandle->OutBufferPosition) {
            return OsDeviceError;
        }
    }
    return status;
}


void sys_file_flush_invocation(struct gracht_message* message, const UUId_t processId, const UUId_t handle)
{
    OsStatus_t status = Flush(processId, handle);
    sys_file_flush_response(message, status);
}

static OsStatus_t
Move(
    _In_ UUId_t      processId,
    _In_ const char* source, 
    _In_ const char* destination,
    _In_ int         copy)
{
    // @todo implement using existing fs functions
    _CRT_UNUSED(processId);
    _CRT_UNUSED(source);
    _CRT_UNUSED(destination);
    _CRT_UNUSED(copy);
    return OsNotSupported;
}

void sys_file_move_invocation(struct gracht_message* message, const UUId_t processId,
        const char* source, const char* destination, const uint8_t copy)
{
    OsStatus_t status = Move(processId, source, destination, copy);
    sys_file_move_response(message, status);
}

static OsStatus_t
GetPosition(
    _In_  UUId_t           processId,
    _In_  UUId_t           handle,
    _Out_ LargeUInteger_t* Position)
{
    FileSystemEntryHandle_t* entryHandle = NULL;
    OsStatus_t               status;

    status = VfsIsHandleValid(processId, handle, 0, &entryHandle);
    if (status != OsSuccess) {
        return status;
    }

    Position->QuadPart = entryHandle->Position;
    return status;
}

void sys_file_get_position_invocation(struct gracht_message* message, const UUId_t processId, const UUId_t handle)
{
    LargeUInteger_t position;
    OsStatus_t      status = GetPosition(processId, handle, &position);
    sys_file_get_position_response(message, status, position.u.LowPart, position.u.HighPart);
}

static OsStatus_t
GetOptions(
    _In_  UUId_t        processId,
    _In_  UUId_t        handle,
    _Out_ unsigned int* options,
    _Out_ unsigned int* access)
{
    FileSystemEntryHandle_t* entryHandle = NULL;
    OsStatus_t               status;

    status = VfsIsHandleValid(processId, handle, 0, &entryHandle);
    if (status != OsSuccess) {
        return status;
    }

    *options = entryHandle->Options;
    *access  = entryHandle->Access;
    return status;
}

void sys_file_get_options_invocation(struct gracht_message* message, const UUId_t processId, const UUId_t handle)
{
    unsigned int options, access;
    OsStatus_t   status = GetOptions(processId, handle, &options, &access);
    sys_file_get_position_response(message, status, options, access);
}

static OsStatus_t
SetOptions(
    _In_ UUId_t       processId,
    _In_ UUId_t       handle,
    _In_ unsigned int options,
    _In_ unsigned int access)
{
    FileSystemEntryHandle_t* entryHandle = NULL;
    OsStatus_t               status;

    status = VfsIsHandleValid(processId, handle, 0, &entryHandle);
    if (status != OsSuccess) {
        return status;
    }

    entryHandle->Options = options;
    entryHandle->Access  = access;
    return status;
}

void sys_file_set_options_invocation(struct gracht_message* message, const UUId_t processId,
        const UUId_t handle, const unsigned int options, const unsigned int access)
{
    OsStatus_t status = SetOptions(processId, handle, options, access);
    sys_file_set_options_response(message, status);
}

OsStatus_t
GetSize(
    _In_ UUId_t           processId,
    _In_ UUId_t           handle,
    _In_ LargeUInteger_t* size)
{
    FileSystemEntryHandle_t* entryHandle = NULL;
    OsStatus_t               status;

    status = VfsIsHandleValid(processId, handle, 0, &entryHandle);
    if (status != OsSuccess) {
        return status;
    }

    size->QuadPart = entryHandle->Entry->Descriptor.Size.QuadPart;
    return status;
}

void sys_file_get_size_invocation(struct gracht_message* message, const UUId_t processId, const UUId_t handle)
{
    LargeUInteger_t size;
    OsStatus_t      status = GetSize(processId, handle, &size);
    sys_file_get_size_response(message, status, size.u.LowPart, size.u.HighPart);
}

static OsStatus_t
GetAbsolutePathOfHandle(
    _In_  UUId_t      processId,
    _In_  UUId_t      handle,
    _Out_ MString_t** path)
{
    FileSystemEntryHandle_t* entryHandle = NULL;
    OsStatus_t               status;

    status = VfsIsHandleValid(processId, handle, 0, &entryHandle);
    if (status != OsSuccess) {
        return OsError;
    }
    
    *path = entryHandle->Entry->Path;
    return OsSuccess;
}

void sys_file_get_path_invocation(struct gracht_message* message, const UUId_t processId, const UUId_t handle)
{
    MString_t* path;
    OsStatus_t status = GetAbsolutePathOfHandle(processId, handle, &path);
    if (status == OsSuccess) {
        sys_file_get_path_response(message, status, MStringRaw(path));
    }
    else {
        sys_file_get_path_response(message, status, "");
    }
}

OsStatus_t
StatFromHandle(
    _In_ UUId_t              processId,
    _In_ UUId_t              handle,
    _In_ OsFileDescriptor_t* descriptor)
{
    FileSystemEntryHandle_t* entryHandle = NULL;
    OsStatus_t               status;

    status = VfsIsHandleValid(processId, handle, 0, &entryHandle);
    if (status == OsSuccess) {
        memcpy((void*)descriptor, (const void*)&entryHandle->Entry->Descriptor, sizeof(OsFileDescriptor_t));
    }
    return status;
}

void sys_file_fstat_invocation(struct gracht_message* message, const UUId_t processId, const UUId_t handle)
{
    OsFileDescriptor_t         descriptor;
    struct sys_file_descriptor gdescriptor;
    OsStatus_t                 status = StatFromHandle(processId, handle, &descriptor);
    if (status == OsSuccess) {
        to_sys_file_descriptor(&descriptor, &gdescriptor);
    }
    sys_file_fstat_response(message, status, &gdescriptor);
}

OsStatus_t
StatFromPath(
    _In_ UUId_t              processId,
    _In_ const char*         path,
    _In_ OsFileDescriptor_t* descriptor)
{
    OsStatus_t status;
    UUId_t     handle;

    status = OpenFile(processId, path, 0, __FILE_READ_ACCESS | __FILE_READ_SHARE, &handle);
    if (status == OsSuccess) {
        status = StatFromHandle(processId, handle, descriptor);
        (void)CloseFile(processId, handle);
    }
    return status;
}

void sys_file_fstat_path_invocation(struct gracht_message* message, const UUId_t processId, const char* path)
{
    OsFileDescriptor_t         descriptor;
    struct sys_file_descriptor gdescriptor;
    OsStatus_t                 status = StatFromPath(processId, path, &descriptor);
    if (status == OsSuccess) {
        to_sys_file_descriptor(&descriptor, &gdescriptor);
    }
    sys_file_fstat_path_response(message, status, &gdescriptor);
}

void sys_file_fsstat_invocation(struct gracht_message* message, const UUId_t processId, const UUId_t handle)
{
    struct sys_filesystem_descriptor gdescriptor = { 0 };
    sys_file_fsstat_response(message, OsNotSupported, &gdescriptor);
}

void sys_file_fsstat_path_invocation(struct gracht_message* message, const UUId_t processId, const char* path)
{
    struct sys_filesystem_descriptor gdescriptor = { 0 };
    sys_file_fsstat_path_response(message, OsNotSupported, &gdescriptor);
}
