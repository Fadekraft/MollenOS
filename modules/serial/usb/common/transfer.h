/**
 * MollenOS
 *
 * Copyright 2018, Philip Meulengracht
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
 * USB Controller Scheduler
 * - Contains the implementation of a shared controller scheduker
 *   for all the usb drivers
 */

#ifndef __USB_TRANSFER__
#define __USB_TRANSFER__

#include <os/dmabuf.h>
#include <os/osdefs.h>
#include <os/spinlock.h>
#include <ddk/contracts/usbhost.h>
#include <ddk/services/usb.h>
#include <threads.h>

typedef enum _UsbManagerTransferFlags {
    TransferFlagNone        = 0,
    TransferFlagShort       = 0x1,
    TransferFlagSync        = 0x2,
    TransferFlagSchedule    = 0x4,
    TransferFlagUnschedule  = 0x8,
    TransferFlagCleanup     = 0x10,
    TransferFlagNotified    = 0x20,
    TransferFlagPartial     = 0x40
} UsbManagerTransferFlags_t;

typedef struct {
    UsbTransfer_t             Transfer;
    thrd_t                    Address;

    // Transfer Metadata
    UUId_t                    Id;
    UUId_t                    DeviceId;
    UsbTransferStatus_t       Status;
    UsbManagerTransferFlags_t Flags;
    void*                     EndpointDescriptor;  // We only use one no matter what
    
    // Per-transaction data
    struct UsbManagerTransaction {
        struct dma_attachment DmaAttachment;
        struct dma_sg_table   DmaTable;
        int                   SgIndex;
        size_t                SgOffset;
        size_t                BytesTransferred;
    } Transactions[USB_TRANSACTIONCOUNT];

    // Periodic Transfers
    size_t                    CurrentDataIndex;    
} UsbManagerTransfer_t;

/**
 * UsbManagerCreateTransfer
 * * Creates a new transfer that is registered with the usb-management
 */
__EXTERN UsbManagerTransfer_t*
UsbManagerCreateTransfer(
    _In_ UsbTransfer_t* Transfer,
    _In_ thrd_t         Address,
    _In_ UUId_t         Device);

/**
 * UsbManagerDestroyTransfer
 * * Cleans up a usb transfer and frees resources.
 */
__EXTERN void
UsbManagerDestroyTransfer(
    _In_ UsbManagerTransfer_t* Transfer);

/**
 * UsbManagerSendNotification
 * * Sends a notification to the subscribing process whenever a periodic
 * * transaction has completed/failed. */
__EXTERN void
UsbManagerSendNotification(
    _In_ UsbManagerTransfer_t* Transfer);

#endif //!__USB_TRANSFER__
 