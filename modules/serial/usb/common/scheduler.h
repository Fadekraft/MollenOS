/* MollenOS
 *
 * Copyright 2011 - 2017, Philip Meulengracht
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
 * MollenOS MCore - USB Controller Scheduler
 * - Contains the implementation of a shared controller scheduker
 *   for all the usb drivers
 */

#ifndef __USB_SCHEDULER__
#define __USB_SCHEDULER__

/* Includes
 * - Library */
#include <os/osdefs.h>
#include <os/driver/usb.h>

/* Definitions
 * Generic bandwidth allocation constants/support */
#define FRAME_TIME_USECS				1000L
#define FRAME_TIME_BITS                 12000L
#define FRAME_TIME_MAX_BITS_ALLOC       (90L * FRAME_TIME_BITS / 100L)
#define FRAME_TIME_MAX_USECS_ALLOC      (90L * FRAME_TIME_USECS / 100L)

#define BitTime(bytecount)				(7 * 8 * bytecount / 6)
#define NS_TO_US(ns)					DIVUP(ns, 1000L)

/* Full/low speed bandwidth allocation constants/support. */
#define BW_HOST_DELAY   1000L
#define BW_HUB_LS_SETUP 333L

/*
* Ceiling [nano/micro]seconds (typical) for that many bytes at high speed
* ISO is a bit less, no ACK ... from USB 2.0 spec, 5.11.3 (and needed
* to preallocate bandwidth)
*/
#define USB2_HOST_DELAY 5       /* nsec, guess */
#define HS_NSECS(bytes) (((55 * 8 * 2083) \
        + (2083UL * (3 + BitTime(bytes))))/1000 \
        + USB2_HOST_DELAY)
#define HS_NSECS_ISO(bytes) (((38 * 8 * 2083) \
        + (2083UL * (3 + BitTime(bytes))))/1000 \
        + USB2_HOST_DELAY)
#define HS_USECS(bytes)         NS_TO_US(HS_NSECS(bytes))
#define HS_USECS_ISO(bytes)     NS_TO_US(HS_NSECS_ISO(bytes))

/* Structures */
typedef struct _UsbScheduler {
	size_t Size;
	size_t MaskSize;
	size_t MaxBandwidth;
	size_t MaxMaskBandwidth;
	size_t TotalBandwidth;
	size_t *Frames;
	Spinlock_t Lock;
} UsbScheduler_t;

/* Init, Destruct, set mask size 
 * General Setup */
UsbScheduler_t *UsbSchedulerInit(size_t Size, size_t MaxBandwidth, size_t MaskSize);
void UsbSchedulerDestroy(UsbScheduler_t *Schedule);

/* This function calculates the approx time a transfer 
 * needs to spend on the bus in NS. */
long UsbCalculateBandwidth(UsbSpeed_t Speed, int Direction, UsbTransferType_t Type, size_t Length);

/* The actual meat of the scheduling */
int UsbSchedulerValidate(UsbScheduler_t *Schedule, size_t Period, size_t Bandwidth, size_t TransferCount);
int UsbSchedulerReserveBandwidth(UsbScheduler_t *Schedule, size_t Period, size_t Bandwidth, 
												size_t TransferCount, size_t *StartFrame, size_t *FrameMask);
int UsbSchedulerReleaseBandwidth(UsbScheduler_t *Schedule, size_t Period, 
												size_t Bandwidth, size_t StartFrame, size_t FrameMask);

#endif //!__USB_SCHEDULER__