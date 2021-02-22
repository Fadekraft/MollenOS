/**
 * MollenOS
 *
 * Copyright 2021, Philip Meulengracht
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
 * Service - Usb Manager
 * - Contains the implementation of the usb-manager which keeps track
 *   of all usb-controllers and their devices
 */

#define __TRACE

#include <ddk/usbdevice.h>
#include <usb/usb.h>
#include <ddk/device.h>
#include <ddk/utils.h>
#include <ds/hashtable.h>
#include <internal/_ipc.h>
#include "manager.h"
#include <stdlib.h>
#include <string.h>

static uint64_t hub_hash(const void* element);
static int      hub_cmp(const void* element1, const void* element2);
static void     hub_free_entry(int index, const void* element, void* userContext);

static hashtable_t g_hubs;

void UsbCoreHubsInitialize(void)
{
    hashtable_construct(&g_hubs, 0, sizeof(struct UsbHub), hub_hash, hub_cmp);
}

void UsbCoreHubsCleanup(void)
{
    hashtable_enumerate(&g_hubs, hub_free_entry, NULL);
    hashtable_destroy(&g_hubs);
}

OsStatus_t
UsbCoreHubsRegister(
        _In_ UUId_t  parentHubDeviceId,
        _In_ UUId_t  hubDeviceId,
        _In_ UUId_t  hubDriverId,
        _In_ int     portCount)
{
    UsbHub_t* parentHub;
    UUId_t    controllerDeviceId = parentHubDeviceId;
    uint8_t   address = 0;
    TRACE("UsbCoreHubsRegister(parentHubDeviceId=%u, hubDeviceId=%u, hubDriverId=%u, portCount=%i)",
          parentHubDeviceId, hubDeviceId, hubDriverId, portCount);

    // take into account root hubs, they provide same ids
    if (parentHubDeviceId != hubDriverId) {
       parentHub = hashtable_get(&g_hubs, &(struct UsbHub) { .DeviceId = parentHubDeviceId });
       if (parentHub) {
           int i;

           controllerDeviceId = parentHub->ControllerDeviceId;
           for (i = 0; i < USB_MAX_PORTS; i++) {
               if (parentHub->Ports[i] && parentHub->Ports[i]->Device &&
                        parentHub->Ports[i]->Device->DeviceId == hubDeviceId) {
                   address = parentHub->Ports[i]->Address;
                   break;
               }
           }
       }
    }

    hashtable_set(&g_hubs, &(struct UsbHub) {
        .ControllerDeviceId = controllerDeviceId,
        .DeviceId = hubDeviceId,
        .DriverId = hubDriverId,
        .PortCount = portCount,
        .Address = address,
        .Ports = { 0 }
    });
    return OsSuccess;
}

void
UsbCoreHubsUnregister(
        _In_ UUId_t hubDeviceId)
{
    int            i;
    struct UsbHub  hub;
    struct UsbHub* hubCopy = hashtable_remove(&g_hubs, &(struct UsbHub) {
        .DeviceId = hubDeviceId
    });
    TRACE("UsbCoreHubsUnregister(hubDeviceId=%u)", hubDeviceId);

    if (!hubCopy) {
        return;
    }

    memcpy(&hub, hubCopy, sizeof(struct UsbHub));
    for (i = 0; i < USB_MAX_PORTS; i++) {
        if (hub.Ports[i] && hub.Ports[i]->Device) {
            UsbCoreDevicesDestroy(UsbCoreControllerGet(hub.ControllerDeviceId), hub.Ports[i]);
            free(hub.Ports[i]);
        }
    }
}

UsbPort_t* __CreatePort(
        _In_ uint8_t address)
{
    UsbPort_t* usbPort;

    usbPort = (UsbPort_t*)malloc(sizeof(UsbPort_t));
    if (!usbPort) {
        return NULL;
    }

    memset(usbPort, 0, sizeof(UsbPort_t));
    usbPort->Address = address;
    return usbPort;
}

UsbPort_t*
UsbCoreHubsGetPort(
        _In_ UsbHub_t* hub,
        _In_ uint8_t   portIndex)
{
    if (!hub || portIndex >= USB_MAX_PORTS) {
        return NULL;
    }

    if (!hub->Ports[portIndex]) {
        hub->Ports[portIndex] = __CreatePort(portIndex);
    }
    return hub->Ports[portIndex];
}

UsbHub_t*
UsbCoreHubsGet(
        _In_ UUId_t hubDeviceId)
{
    return hashtable_get(&g_hubs, &(struct UsbHub) { .DeviceId = hubDeviceId });
}

static void hub_free_entry(int index, const void* element, void* userContext)
{
    const struct UsbHub* hub = element;
    // what do
}

static uint64_t hub_hash(const void* element)
{
    const struct UsbHub* hub = element;
    return hub->DeviceId;
}

static int hub_cmp(const void* element1, const void* element2)
{
    const struct UsbHub* hub1 = element1;
    const struct UsbHub* hub2 = element2;
    return hub1->DeviceId == hub2->DeviceId ? 0 : 1;
}

void svc_usb_register_hub_callback(
        _In_ struct gracht_recv_message*       message,
        _In_ struct svc_usb_register_hub_args* args)
{
    OsStatus_t osStatus = UsbCoreHubsRegister(args->parent_hub_device_id, args->device_id,
                                              args->driver_id, args->port_count);
    if (osStatus != OsSuccess) {
        // log
    }
}

void svc_usb_unregister_hub_callback(
        _In_ struct gracht_recv_message*         message,
        _In_ struct svc_usb_unregister_hub_args* args)
{
    UsbCoreHubsUnregister(args->device_id);
}
