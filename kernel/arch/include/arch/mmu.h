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
 * MMU Interface
 * - Contains a glue layer to access hardware functionality
 *   that all sub-layers / architectures must conform to
 */
#ifndef __SYSTEM_MMU_INTEFACE_H__
#define __SYSTEM_MMU_INTEFACE_H__

#include <os/osdefs.h>
#include <memoryspace.h>

extern OsStatus_t
InitializeVirtualSpace(
        _In_ MemorySpace_t*);

extern OsStatus_t
CloneVirtualSpace(
        _In_ MemorySpace_t*,
        _In_ MemorySpace_t*,
        _In_ int);

extern OsStatus_t
DestroyVirtualSpace(
        _In_ MemorySpace_t*);

/**
 * ArchMmuSwitchMemorySpace
 * * Switches the current memory space out with the given memory space. This
 * * will cause a total TLB flush.
 * @param MemorySpace [In]
 */
KERNELAPI void KERNELABI
ArchMmuSwitchMemorySpace(
        _In_ MemorySpace_t* memorySpace);

/**
 * ArchMmuGetPageAttributes
 * * Retrieves memory attributes for the number of virtual address provided. The array
 * * of attribute values provided must be large enough to hold the number of pages requested.
 * @param MemorySpace     [In]
 * @param VirtualAddress  [In]
 * @param PageCount       [In]
 * @param AttributeValues [In]
 * @param PagesRetrieved  [Out]
 * 
 * @return Status of the page attribute lookup.
 */
KERNELAPI OsStatus_t KERNELABI
ArchMmuGetPageAttributes(
        _In_  MemorySpace_t*,
        _In_  vaddr_t,
        _In_  int,
        _In_  unsigned int*,
        _Out_ int*);

/**
 * ArchMmuUpdatePageAttributes
 * * Changes memory attributes for the number of virtual address provided. The
 * * attributes will be set the same for all pages requested.
 * @param MemorySpace    [In]
 * @param VirtualAddress [In]
 * @param PageCount      [In]
 * @param Attributes     [In]  Replaces the attributes here with the previous attributes 
 * @param PagesUpdated   [Out]
 * 
 * @return OsSuccess if operation was fully completed, otherwise OsIncomplete
 */
KERNELAPI OsStatus_t KERNELABI
ArchMmuUpdatePageAttributes(
        _In_  MemorySpace_t*,
        _In_  vaddr_t,
        _In_  int,
        _In_  unsigned int*,
        _Out_ int*);

/**
 * ArchMmuCommitVirtualPage
 * * Update the underlying mapping for a single page at the virtual address given.
 * @param MemorySpace           [In]
 * @param VirtualAddress        [In]
 * @param PhysicalAddressValues [In]
 * @param PageCount             [In]
 * @param PagesComitted         [Out]
 * 
 * @return Status of the address mapping update.
 */
KERNELAPI OsStatus_t KERNELABI
ArchMmuCommitVirtualPage(
        _In_  MemorySpace_t*           memorySpace,
        _In_  vaddr_t         startAddress,
        _In_  const paddr_t* physicalAddresses,
        _In_  int                      pageCount,
        _Out_ int*                     pagesComittedOut);

/**
 * ArchMmuSetContiguousVirtualPages
 * * Creates @PageCount number of virtual memory mappings that correspond to the
 * * physical address given.
 * @param MemorySpace          [In]
 * @param VirtualAddress       [In]
 * @param PhysicalStartAddress [In]
 * @param PageCount            [In]
 * @param Flags                [In]
 * @param PagesUpdated         [Out]
 * 
 * @return Status of the address mapping creation.
 */
KERNELAPI OsStatus_t KERNELABI
ArchMmuSetContiguousVirtualPages(
        _In_  MemorySpace_t*,
        _In_  vaddr_t,
        _In_  paddr_t,
        _In_  int,
        _In_  unsigned int,
        _Out_ int*);

/**
 * ArchMmuReserveVirtualPages
 * * Reserves @PageCount number of virtual memory mappings.
 * @param MemorySpace          [In]
 * @param VirtualAddress       [In]
 * @param PageCount            [In]
 * @param Flags                [In]
 * @param PagesReserved        [Out]
 * 
 * @return Status of the address mapping reservation.
 */
KERNELAPI OsStatus_t KERNELABI
ArchMmuReserveVirtualPages(
        _In_  MemorySpace_t*,
        _In_  vaddr_t,
        _In_  int,
        _In_  unsigned int,
        _Out_ int*);

/**
 * ArchMmuSetVirtualPages
 * * Creates @PageCount number of virtual memory mappings that correspond to the
 * * array of physical mappings given. The array provided must be large enough to
 * * fit the requested number of mappings.
 * @param MemorySpace           [In]
 * @param VirtualAddress        [In]
 * @param PhysicalAddressValues [In]
 * @param PageCount             [In]
 * @param Flags                 [In]
 * @param PagesUpdated          [Out]
 * 
 * @return Status of the address mapping creation.
 */
KERNELAPI OsStatus_t KERNELABI
ArchMmuSetVirtualPages(
        _In_  MemorySpace_t*           memorySpace,
        _In_  vaddr_t         startAddress,
        _In_  const paddr_t* physicalAddressValues,
        _In_  int                      pageCount,
        _In_  unsigned int             attributes,
        _Out_ int*                     pagesUpdatedOut);

/**
 * ArchMmuClearVirtualPages
 * Removes <pageCount> number of virtual memory mappings, the physical pages that are available for freeing
 * will be returned in <freedAddresses> array.
 * @param memorySpace            [In]  The memory space to perform the unmapping in.
 * @param startAddress           [In]  The start address from where to unmap from.
 * @param pageCount              [In]  The number of pages that should be unmapped.
 * @param freedAddresses         [In]  An array of size <pageCount> that will hold all freed physical addresses.
 *                                     This array can contain less elements than the value returned in <pagesClearedOut>
 *                                     if some of the physical pages were marked as non-freeable.
 * @param freedAddressesCountOut [Out] The number of valid entries in <freedAddresses>.
 * @param pagesClearedOut        [Out] The number of pages that was unmapped
 * 
 * @return Status of the address mapping removal.
 */
KERNELAPI OsStatus_t KERNELABI
ArchMmuClearVirtualPages(
        _In_  MemorySpace_t*     memorySpace,
        _In_  vaddr_t   startAddress,
        _In_  int                pageCount,
        _In_  paddr_t* freedAddresses,
        _Out_ int*               freedAddressesCountOut,
        _Out_ int*               pagesClearedOut);

/**
 * ArchMmuVirtualToPhysical
 * * Converts a range of virtual addresses to physical addresses. The number of
 * * pages requested converted must be able to fit in to the PhysicalAddress array.
 * * Offset of the first page will be preserved in the translation.
 * @param MemorySpace           [In]
 * @param VirtualAddress        [In]
 * @param PageCount             [In]
 * @param PhysicalAddressValues [In]
 * @param PagesRetrieved        [Out]
 * 
 * @return Status of the address space translation.
 */
KERNELAPI OsStatus_t KERNELABI
ArchMmuVirtualToPhysical(
        _In_  MemorySpace_t*,
        _In_  vaddr_t,
        _In_  int,
        _In_  paddr_t*,
        _Out_ int*);

#endif //!__SYSTEM_MMU_INTEFACE_H__
