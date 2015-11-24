/* MollenOS
*
* Copyright 2011 - 2014, Philip Meulengracht
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
* MollenOS Common Entry Point
*/

/* Includes */
#include <revision.h>
#include <MollenOS.h>
#include <Arch.h>
#include <Devices/Cpu.h>
#include <Devices/Video.h>
#include <DeviceManager.h>
#include <Modules/ModuleManager.h>
#include <Scheduler.h>
#include <Threading.h>
#include <Vfs\Vfs.h>
#include <Heap.h>
#include <Log.h>

/* Globals */
MCoreCpuDevice_t BootCpu = { 0 };
MCoreVideoDevice_t BootVideo = { 0 };

/* Print Header Information */
void PrintHeader(MCoreBootInfo_t *BootInfo)
{
	Log("MollenOS - Platform: %s - Version %i.%i.%i",
		ARCHITECTURE_NAME, REVISION_MAJOR, REVISION_MINOR, REVISION_BUILD);
	Log("Written by Philip Meulengracht, Copyright 2011-2015.");
	Log("Bootloader - %s", BootInfo->BootloaderName);
	Log("VC Build %s - %s\n", BUILD_DATE, BUILD_TIME);
}

/* Shared Entry in MollenOS
 * */
void MCoreInitialize(MCoreBootInfo_t *BootInfo)
{
	/* Initialize Cpu */
	CpuInit(&BootCpu, BootInfo->ArchBootInfo);

	/* Setup Video Boot */
	VideoInit(&BootVideo, BootInfo);

	/* Now init log */
	LogInit(LogConsole, LogLevel1);

	/* Print Header */
	PrintHeader(BootInfo);

	/* Init HAL */
	BootInfo->InitHAL(BootInfo->ArchBootInfo, &BootInfo->Descriptor);

	/* Init the heap */
	HeapInit();

	/* Init post-heap systems */
	DmInit();
	DmCreateDevice("Processor", DeviceCpu, &BootCpu);
	DmCreateDevice("BootVideo", DeviceVideo, &BootVideo);

	/* Init ModuleManager */
	ModuleMgrInit(&BootInfo->Descriptor);

	/* Init Threading & Scheduler for boot cpu */
	SchedulerInit(0);
	ThreadingInit();

	/* Init post-systems */
	BootInfo->InitPostSystems();

	/* Beyond this point we need timers 
	 * and right now we have no timers,
	 * and worst of all, timers are VERY 
	 * arch-specific, so we let the underlying
	 * architecture load them */
	BootInfo->InitTimers();

	/* Start out any extra cores */
	//CpuInitSmp(BootInfo->ArchBootInfo);

	/* Start the request handle */
	DmStart();

	/* Virtual Filesystem */
	VfsInit();

	/* From this point, we should start seperate threads and
	* let this thread die out, because initial system setup
	* is now totally done, and the moment we start another
	* thread, it will take over as this is the idle thread */

	/* Drivers */
	ThreadingCreateThread("DriverSetup", DevicesInit, NULL, 0);

	/* Enter Idle Loop */
	while (1)
		Idle();
}