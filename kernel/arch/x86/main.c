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
 * MollenOS x86 Initialization code
 * - Handles setup from a x86 entry point
 */
#define __MODULE "INIT"

/* Includes 
 * - System */
#include <system/setup.h>
#include <system/utils.h>
#include <interrupts.h>
#include <multiboot.h>
#include <memory.h>
#include <debug.h>
#include <smbios.h>
#include <arch.h>
#include <apic.h>
#include <cpu.h>
#include <gdt.h>
#include <idt.h>
#include <pic.h>
#include <vbe.h>

/* TimersDiscover 
 * Discover the available system timers for the x86 platform. */
OsStatus_t
TimersDiscover(void);

/* SystemInformationQuery 
 * Queries information about the running system
 * and the underlying architecture */
OsStatus_t
SystemInformationQuery(
	_Out_ SystemInformation_t *Information)
{
	// Copy memory information
	if (MmPhysicalQuery(&Information->PagesTotal, &Information->PagesAllocated) != OsSuccess) {
		return OsError;
	}

    // Fill in memory overview
    Information->AllocationGranularity                  = ALLOCATION_BLOCK_SIZE;
    Information->MemoryOverview.UserCodeStart           = MEMORY_LOCATION_RING3_CODE;
    Information->MemoryOverview.UserCodeSize            = MEMORY_LOCATION_RING3_CODE_END - MEMORY_LOCATION_RING3_CODE;
    Information->MemoryOverview.UserSharedMemoryStart   = MEMORY_LOCATION_RING3_SHM;
    Information->MemoryOverview.UserSharedMemorySize    = MEMORY_LOCATION_RING3_SHM_END - MEMORY_LOCATION_RING3_SHM;
    Information->MemoryOverview.UserDriverMemoryStart   = MEMORY_LOCATION_RING3_IOSPACE;
    Information->MemoryOverview.UserDriverMemorySize    = MEMORY_LOCATION_RING3_IOSPACE_END - MEMORY_LOCATION_RING3_IOSPACE;
    Information->MemoryOverview.UserHeapStart           = MEMORY_LOCATION_RING3_HEAP;
    Information->MemoryOverview.UserHeapSize            = MEMORY_LOCATION_RING3_HEAP_END - MEMORY_LOCATION_RING3_HEAP;
	return OsSuccess;
}

/* SystemFeaturesQuery
 * Called by the kernel to get which systems we support */
OsStatus_t
SystemFeaturesQuery(
    _In_ Multiboot_t *BootInformation,
    _Out_ Flags_t *SystemsSupported)
{
    // Variables
    Flags_t Features = 0;

    // Of course we support software features
    Features |= SYSTEM_FEATURE_INITIALIZE;
    Features |= SYSTEM_FEATURE_FINALIZE;

    // Memory features
    Features |= SYSTEM_FEATURE_MEMORY;

    // Output features
    Features |= SYSTEM_FEATURE_OUTPUT;

    // Hardware features
    Features |= SYSTEM_FEATURE_INTERRUPTS;

    // Done
    *SystemsSupported = Features;
    return OsSuccess;
}

/* SystemFeaturesInitialize
 * Called by the kernel to initialize a supported system */
OsStatus_t
SystemFeaturesInitialize(
    _In_ Multiboot_t *BootInformation,
    _In_ Flags_t Systems)
{
    // Handle the memory initialization
    if (Systems & SYSTEM_FEATURE_MEMORY) {
        MmPhyiscalInit(BootInformation);
        MmVirtualInit();
    }

    // Handle interrupt initialization
    if (Systems & SYSTEM_FEATURE_INTERRUPTS) {
        InitializeSoftwareInterrupts();
#if defined(amd64) || defined(__amd64__)
        TssCreateStacks();
#endif
        // Make sure we allocate all device interrupts
        // so system can't take control of them
        InterruptIncreasePenalty(0); // PIT
        InterruptIncreasePenalty(1); // PS/2
        InterruptIncreasePenalty(2); // PIT / Cascade
        InterruptIncreasePenalty(3); // COM 2/4
        InterruptIncreasePenalty(4); // COM 1/3
        InterruptIncreasePenalty(5); // LPT2
        InterruptIncreasePenalty(6); // Floppy
        InterruptIncreasePenalty(7); // LPT1 / Spurious
        InterruptIncreasePenalty(8); // CMOS
        InterruptIncreasePenalty(12); // PS/2
        InterruptIncreasePenalty(13); // FPU
        InterruptIncreasePenalty(14); // IDE
        InterruptIncreasePenalty(15); // IDE / Spurious
        
        // Initialize APIC?
        if (CpuHasFeatures(0, CPUID_FEAT_EDX_APIC) == OsSuccess) {
            ApicInitialize();
        }
        else {
            FATAL(FATAL_SCOPE_KERNEL, "Cpu does not have a local apic. This model is too old and not supported.");
        }
    }

    // Handle final things before the system spawns
    // all services and drivers
    if (Systems & SYSTEM_FEATURE_FINALIZE) {
        // Free all the allocated isa's now for drivers
        InterruptDecreasePenalty(0); // PIT
        InterruptDecreasePenalty(1); // PS/2
        InterruptDecreasePenalty(2); // PIT / Cascade
        InterruptDecreasePenalty(3); // COM 2/4
        InterruptDecreasePenalty(4); // COM 1/3
        InterruptDecreasePenalty(5); // LPT2
        InterruptDecreasePenalty(6); // Floppy
        InterruptDecreasePenalty(7); // LPT1 / Spurious
        InterruptDecreasePenalty(8); // CMOS
        InterruptDecreasePenalty(12); // PS/2
        InterruptDecreasePenalty(13); // FPU
        InterruptDecreasePenalty(14); // IDE
        InterruptDecreasePenalty(15); // IDE

        // Activate fixed system timers
        TimersDiscover();
        
        // Recalibrate in case of apic
        if (CpuHasFeatures(0, CPUID_FEAT_EDX_APIC) == OsSuccess) {
            ApicRecalibrateTimer();
        }
        CpuSmpInitialize();
    }
    return OsSuccess;
}
