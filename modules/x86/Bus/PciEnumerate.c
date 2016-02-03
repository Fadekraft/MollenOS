/* MollenOS
*
* Copyright 2011 - 2016, Philip Meulengracht
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
* MollenOS X86 Pci Driver
*/

/* Arch */
#include <x86\AcpiSys.h>

/* Includes */
#include <DeviceManager.h>
#include <Module.h>
#include <Pci.h>
#include <List.h>
#include <Heap.h>
#include <Log.h>

/* Definitions */
#define RUN_EHCI_FIRST

/* The MCFG Entry */
#pragma pack(push, 1)
typedef struct _McfgEntry
{
	/* Base Address */
	uint64_t BaseAddress;

	/* Pci Segment Group */
	uint16_t SegmentGroup;

	/* Bus Range */
	uint8_t StartBus;
	uint8_t EndBus;

	/* Unused */
	uint32_t Reserved;

} McfgEntry_t; 
#pragma pack(pop)

/* Globals */
list_t *GlbPciDevices = NULL;

/* Prototypes */
void PciCheckBus(list_t *Bridge, PciBus_t *Bus, uint8_t BusNo);

/* Create class information from dev */
DevInfo_t PciToDevClass(uint32_t Class, uint32_t SubClass)
{
	return ((Class & 0xFFFF) << 16 | SubClass & 0xFFFF);
}

DevInfo_t PciToDevSubClass(uint32_t Interface)
{
	return ((Interface & 0xFFFF) << 16 | 0);
}

/* Validate size */
uint64_t PciValidateBarSize(uint64_t Base, uint64_t MaxBase, uint64_t Mask)
{
	/* Find the significant bits */
	uint64_t Size = Mask & MaxBase;

	/* Sanity */
	if (!Size)
		return 0;

	/* Calc correct size */
	Size = (Size & ~(Size - 1)) - 1;

	/* Sanitize the base */
	if (Base == MaxBase && ((Base | Size) & Mask) != Mask)
		return 0;

	/* Done! */
	return Size;
}

/* Read IO Areas */
void PciReadBars(PciBus_t *Bus, MCoreDevice_t *Device, uint32_t HeaderType)
{
	/* Sanity */
	int Count = (HeaderType & 0x1) == 0x1 ? 2 : 6;
	int i;

	/* Iterate */
	for (i = 0; i < Count; i++)
	{
		/* Vars */
		uint32_t Offset = 0x10 + (i << 2);
		uint32_t Space32, Size32, Mask32;
		uint64_t Space64, Size64, Mask64;

		/* Calc initial mask */
		Mask32 = (HeaderType & 0x1) == 0x1 ? ~0x7FF : 0xFFFFFFFF;

		/* Read */
		Space32 = PciRead32(Bus, Device->Bus, Device->Device, Device->Function, Offset);
		PciWrite32(Bus, Device->Bus, Device->Device, Device->Function, Offset, Space32 | Mask32);
		Size32 = PciRead32(Bus, Device->Bus, Device->Device, Device->Function, Offset);
		PciWrite32(Bus, Device->Bus, Device->Device, Device->Function, Offset, Space32);

		/* Sanity */
		if (Size32 == 0xFFFFFFFF)
			Size32 = 0;
		if (Space32 == 0xFFFFFFFF)
			Space32 = 0;

		/* Io? */
		if (Space32 & 0x1) 
		{
			/* Modify mask */
			Mask64 = 0xFFFC;
			Size64 = Size32;
			Space64 = Space32 & 0xFFFC;

			/* Correct the size */
			Size64 = PciValidateBarSize(Space64, Size64, Mask64);

			/* Sanity */
			if (Space64 != 0
				&& Size64 != 0)
				Device->IoSpaces[i] = IoSpaceCreate(DEVICE_IO_SPACE_IO, (Addr_t)Space64, (size_t)Size64);
		}

		/* Memory, 64 bit or 32 bit? */
		else if (Space32 & 0x4) {

			/* 64 Bit */
			Space64 = Space32 & 0xFFFFFFF0;
			Size64 = Size32 & 0xFFFFFFF0;
			Mask64 = 0xFFFFFFFFFFFFFFF0;
			
			/* Calculate new offset */
			i++;
			Offset = 0x10 + (i << 2);

			/* Read */
			Space32 = PciRead32(Bus, Device->Bus, Device->Device, Device->Function, Offset);
			PciWrite32(Bus, Device->Bus, Device->Device, Device->Function, Offset, 0xFFFFFFFF);
			Size32 = PciRead32(Bus, Device->Bus, Device->Device, Device->Function, Offset);
			PciWrite32(Bus, Device->Bus, Device->Device, Device->Function, Offset, Space32);

			/* Add */
			Space64 |= ((uint64_t)Space32 << 32);
			Size64 |= ((uint64_t)Size32 << 32);

			/* Sanity */
			if (sizeof(Addr_t) < 8
				&& Space64 > SIZE_MAX) {
				LogFatal("PCIE", "Found 64 bit device with 64 bit address, can't use it in 32 bit mode");
				return;
			}

			/* Correct the size */
			Size64 = PciValidateBarSize(Space64, Size64, Mask64);

			/* Create */
			if (Space64 != 0
				&& Size64 != 0)
				Device->IoSpaces[i] = IoSpaceCreate(DEVICE_IO_SPACE_MMIO, (Addr_t)Space64, (size_t)Size64);
		}
		else {
			/* Set mask */
			Space64 = Space32 & 0xFFFFFFF0;
			Size64 = Size32 & 0xFFFFFFF0;
			Mask64 = 0xFFFFFFF0;

			/* Correct the size */
			Size64 = PciValidateBarSize(Space64, Size64, Mask64);

			/* Create */
			if (Space64 != 0
				&& Size64 != 0)
				Device->IoSpaces[i] = IoSpaceCreate(DEVICE_IO_SPACE_MMIO, (Addr_t)Space64, (size_t)Size64);
		}
	}
}

/* Check a function */
/* For each function we create a 
 * pci_device and add it to the list */
void PciCheckFunction(list_t *Bridge, PciBus_t *BusIo, uint8_t Bus, uint8_t Device, uint8_t Function, uint8_t EhciFirst)
{
	/* Vars */
	MCoreDevice_t *mDevice;
	uint8_t SecondBus;
	PciNativeHeader_t *Pcs;
	PciDevice_t *PciDevice;

	/* Sanity */
	if (EhciFirst != 0)
	{
		/* Get Class/SubClass/Interface */
		uint8_t Class = PciReadBaseClass(BusIo, Bus, Device, Function);
		uint8_t SubClass = PciReadSubclass(BusIo, Bus, Device, Function);
		uint8_t Interface = PciReadInterface(BusIo, Bus, Device, Function);

		/* Is this an ehci? */
		if (Class != 0x0C
			|| SubClass != 0x03
			|| Interface != 0x20)
			return;
	}

	/* Allocate */
	Pcs = (PciNativeHeader_t*)kmalloc(sizeof(PciNativeHeader_t));
	PciDevice = (PciDevice_t*)kmalloc(sizeof(PciDevice_t));

	/* Get full information */
	PciReadFunction(Pcs, BusIo, Bus, Device, Function);

	/* Set information */
	PciDevice->PciBus = BusIo;
	PciDevice->Header = Pcs;
	PciDevice->Bus = Bus;
	PciDevice->Device = Device;
	PciDevice->Function = Function;
	PciDevice->Children = NULL;

	/* Info 
	 * Ignore the spam of device_id 0x7a0 in VMWare*/
	if (Pcs->DeviceId != 0x7a0)
	{
#ifdef X86_PCI_DIAGNOSE
		printf("    * [%d:%d:%d][%d:%d:%d] Vendor 0x%x, Device 0x%x : %s\n",
			Pcs->Class, Pcs->Subclass, Pcs->Interface,
			Bus, Device, Function,
			Pcs->VendorId, Pcs->DeviceId,
			PciToString(Pcs->Class, Pcs->Subclass, Pcs->Interface));
#endif
	}

	/* Do some disabling, but NOT on the video or bridge */
	if ((Pcs->Class != PCI_DEVICE_CLASS_BRIDGE) 
		&& (Pcs->Class != PCI_DEVICE_CLASS_VIDEO))
	{
		/* Disable Device untill further notice */
		uint16_t PciSettings = PciRead16(BusIo, Bus, Device, Function, 0x04);
		PciWrite16(BusIo, Bus, Device, Function, 0x04, PciSettings | X86_PCI_COMMAND_INTDISABLE);
	}
	
	/* Add to list */
	if (Pcs->Class == PCI_DEVICE_CLASS_BRIDGE 
		&& Pcs->Subclass == PCI_DEVICE_SUBCLASS_PCI)
	{
		PciDevice->Type = X86_PCI_TYPE_BRIDGE;
		list_append(Bridge, list_create_node(X86_PCI_TYPE_BRIDGE, PciDevice));

		/* Uh oh, this dude has children */
		PciDevice->Children = list_create(LIST_SAFE);

		/* Get secondary bus no and iterate */
		SecondBus = PciReadSecondaryBusNumber(BusIo, Bus, Device, Function);
		PciCheckBus(PciDevice->Children, BusIo, SecondBus);
	}
	else
	{
		/* This is an device */
		PciDevice->Type = X86_PCI_TYPE_DEVICE;
		list_append(Bridge, list_create_node(X86_PCI_TYPE_DEVICE, PciDevice));

		/* Register device with the OS */
		mDevice = (MCoreDevice_t*)kmalloc(sizeof(MCoreDevice_t));
		memset(mDevice, 0, sizeof(MCoreDevice_t));
		
		/* Setup information */
		mDevice->VendorId = Pcs->VendorId;
		mDevice->DeviceId = Pcs->DeviceId;
		mDevice->Class = PciToDevClass(Pcs->Class, Pcs->Subclass);
		mDevice->Subclass = PciToDevSubClass(Pcs->Interface);

		mDevice->Segment = (DevInfo_t)BusIo->Segment;
		mDevice->Bus = Bus;
		mDevice->Device = Device;
		mDevice->Function = Function;

		mDevice->IrqLine = -1;
		mDevice->IrqPin = (int)Pcs->InterruptPin;
		mDevice->IrqAvailable[0] = Pcs->InterruptLine;

		/* Type */
		mDevice->Type = DeviceUnknown;
		mDevice->Data = NULL;

		/* Save bus information */
		mDevice->BusDevice = PciDevice;

		/* Initial */
		mDevice->Driver.Name = NULL;
		mDevice->Driver.Version = -1;
		mDevice->Driver.Data = NULL;
		mDevice->Driver.Status = DriverNone;

		/* Read Bars */
		PciReadBars(BusIo, mDevice, Pcs->HeaderType);

		/* PCI - IDE Bar Fixup 
		 * From experience ide-bars don't always show up (ex: Oracle VM)
		 * but only the initial 4 bars don't, the BM bar 
		 * always seem to show up */
		if (Pcs->Class == 0x01
			&& Pcs->Subclass == 0x01) 
		{
			/* Let's see */
			if ((Pcs->Interface & 0x1) == 0)
			{
				/* Controller 1 */
				if (mDevice->IoSpaces[0] == NULL)
					mDevice->IoSpaces[0] = IoSpaceCreate(DEVICE_IO_SPACE_IO, 0x1F0, 8);
				if (mDevice->IoSpaces[1] == NULL)
					mDevice->IoSpaces[1] = IoSpaceCreate(DEVICE_IO_SPACE_IO, 0x3F6, 4);
			}

			/* Again, let's see */
			if ((Pcs->Interface & 0x4) == 0)
			{
				/* Controller 2 */
				if (mDevice->IoSpaces[2] == NULL)
					mDevice->IoSpaces[2] = IoSpaceCreate(DEVICE_IO_SPACE_IO, 0x170, 8);
				if (mDevice->IoSpaces[3] == NULL)
					mDevice->IoSpaces[3] = IoSpaceCreate(DEVICE_IO_SPACE_IO, 0x376, 4);
			}
		}

		/* Register */
		DmCreateDevice((char*)PciToString(Pcs->Class, Pcs->Subclass, Pcs->Interface), mDevice);
	}
}

/* Check a device */
void PciCheckDevice(list_t *Bridge, PciBus_t *Bus, uint8_t BusNo, uint8_t Device, uint8_t EhciFirst)
{
	uint8_t Function = 0;
	uint16_t VendorId = 0;
	uint8_t HeaderType = 0;

	/* Get vendor id */
	VendorId = PciReadVendorId(Bus, BusNo, Device, Function);

	/* Sanity */
	if (VendorId == 0xFFFF)
		return;

	/* Check function 0 */
	PciCheckFunction(Bridge, Bus, BusNo, Device, Function, EhciFirst);
	HeaderType = PciReadHeaderType(Bus, BusNo, Device, Function);

	/* Multi-function or single? */
	if (HeaderType & 0x80)
	{
		/* It is a multi-function device, so check remaining functions */
		for (Function = 1; Function < 8; Function++)
		{
			/* Only check if valid vendor */
			if (PciReadVendorId(Bus, BusNo, Device, Function) != 0xFFFF)
				PciCheckFunction(Bridge, Bus, BusNo, Device, Function, EhciFirst);
		}
	}
}

/* Check a bus */
void PciCheckBus(list_t *Bridge, PciBus_t *Bus, uint8_t BusNo)
{
	/* Vars */
	uint8_t Device;

#ifdef RUN_EHCI_FIRST
	/* Find the ehci controllers first & init */
	for (Device = 0; Device < 32; Device++)
		PciCheckDevice(Bridge, Bus, BusNo, Device, 1);
#endif

	/* Iterate devices on bus */
	for (Device = 0; Device < 32; Device++)
		PciCheckDevice(Bridge, Bus, BusNo, Device, 0);
}

/* First of all, devices exists on TWO different
 * busses. PCI and PCI Express. */
MODULES_API void ModuleInit(void *Data)
{
	/* We need these */
	ACPI_TABLE_MCFG *McfgTable = (ACPI_TABLE_MCFG*)Data;
	uint32_t Function;
	uint32_t BusNo;
	uint8_t HeaderType;

	/* Init list, this is "bus 0" */
	GlbPciDevices = list_create(LIST_SAFE);

	/* Pci Express */
	if (McfgTable != NULL)
	{
		/* Woah, there exists Pci Express Controllers */
		uint32_t EntryCount = (McfgTable->Header.Length - sizeof(ACPI_TABLE_MCFG) / sizeof(McfgEntry_t));
		uint32_t Itr = 0;
		McfgEntry_t *Entry = (McfgEntry_t*)((uint8_t*)McfgTable + sizeof(ACPI_TABLE_MCFG));

		/* Iterate */
		for (Itr = 0; Itr < EntryCount; Itr++)
		{
			/* Allocate entry */
			PciBus_t *Bus = (PciBus_t*)kmalloc(sizeof(PciBus_t));

			/* Memory Map 256 MB!!!!! Oh fucking god */
			Bus->IoSpace = IoSpaceCreate(DEVICE_IO_SPACE_MMIO, (Addr_t)Entry->BaseAddress, (1024 * 1024 * 256));
			Bus->IsExtended = 1;
			Bus->BusStart = Entry->StartBus;
			Bus->BusEnd = Entry->EndBus;
			Bus->Segment = Entry->SegmentGroup;

			/* Enumerate devices */
			for (Function = Bus->BusStart; Function <= Bus->BusEnd; Function++)
			{
				/* Check bus */
				BusNo = Function;
				PciCheckBus(GlbPciDevices, Bus, (uint8_t)BusNo);
			}

			/* Next */
			Entry++;
		}
	}
	else
	{
		/* Allocate entry */
		PciBus_t *Bus = (PciBus_t*)kmalloc(sizeof(PciBus_t));

		/* Setup */
		Bus->BusStart = 0;
		Bus->BusEnd = 7;
		Bus->IoSpace = IoSpaceCreate(DEVICE_IO_SPACE_IO, X86_PCI_SELECT, 8);
		Bus->IsExtended = 0;
		Bus->Segment = 0;

		/* Pci Legacy */
		HeaderType = PciReadHeaderType(Bus, 0, 0, 0);

		if ((HeaderType & 0x80) == 0)
		{
			/* Single PCI host controller */
			PciCheckBus(GlbPciDevices, Bus, 0);
		}
		else
		{
			/* Multiple PCI host controllers */
			for (Function = 0; Function < 8; Function++)
			{
				if (PciReadVendorId(Bus, 0, 0, Function) != 0xFFFF)
					break;

				/* Check bus */
				BusNo = Function;
				PciCheckBus(GlbPciDevices, Bus, (uint8_t)BusNo);
			}
		}
	}
}