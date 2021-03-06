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
 * Usb Definitions & Structures
 * - This header describes the base usb-structure, prototypes
 *   and functionality, refer to the individual things for descriptions
 */

#include <ddk/service.h>
#include <ddk/usbdevice.h>
#include <usb/usb.h>
#include <internal/_ipc.h>
#include <internal/_utils.h>
#include <gracht/server.h>

extern int __crt_get_server_iod(void);

OsStatus_t
UsbControllerRegister(
        _In_ Device_t*           device,
        _In_ UsbControllerType_t type,
        _In_ int                 portCount)
{
    struct vali_link_message msg          = VALI_MSG_INIT_HANDLE(GetUsbService());
    UUId_t                   serverHandle = GetNativeHandle(__crt_get_server_iod());
    
    sys_usb_register_controller(GetGrachtClient(), &msg.base, serverHandle,
                                (const uint8_t*)device, device->Length, (int)type, (int)portCount);
    return OsSuccess;
}

OsStatus_t
UsbControllerUnregister(
    _In_ UUId_t deviceId)
{
    struct vali_link_message msg = VALI_MSG_INIT_HANDLE(GetUsbService());
    
    sys_usb_unregister_controller(GetGrachtClient(), &msg.base, deviceId);
    return OsSuccess;
}

OsStatus_t
UsbHubRegister(
        _In_ UsbDevice_t* usbDevice,
        _In_ int          portCount)
{
    struct vali_link_message msg          = VALI_MSG_INIT_HANDLE(GetUsbService());
    UUId_t                   serverHandle = GetNativeHandle(__crt_get_server_iod());

    sys_usb_register_hub(GetGrachtClient(), &msg.base,
                         usbDevice->DeviceContext.hub_device_id,
                         usbDevice->Base.Id,
                         serverHandle,
                         portCount);
    return OsSuccess;
}

OsStatus_t
UsbHubUnregister(
        _In_ UUId_t deviceId)
{
    struct vali_link_message msg = VALI_MSG_INIT_HANDLE(GetUsbService());

    sys_usb_unregister_hub(GetGrachtClient(), &msg.base, deviceId);
    return OsSuccess;
}

OsStatus_t
UsbEventPort(
    _In_ UUId_t  DeviceId,
    _In_ uint8_t PortAddress)
{
    int                      status;
    struct vali_link_message msg = VALI_MSG_INIT_HANDLE(GetUsbService());
    
    status = sys_usb_port_event(GetGrachtClient(), &msg.base, DeviceId, PortAddress);
    return OsSuccess;
}

OsStatus_t
UsbPortError(
        _In_ UUId_t  deviceId,
        _In_ uint8_t portAddress)
{
    int                      status;
    struct vali_link_message msg = VALI_MSG_INIT_HANDLE(GetUsbService());

    status = sys_usb_port_error(GetGrachtClient(), &msg.base, deviceId, portAddress);
    return OsSuccess;
}

OsStatus_t
UsbQueryControllerCount(
    _Out_ int* ControllerCount)
{
    int                      status;
    struct vali_link_message msg = VALI_MSG_INIT_HANDLE(GetUsbService());
    
    status = sys_usb_get_controller_count(GetGrachtClient(), &msg.base);
    gracht_client_wait_message(GetGrachtClient(), &msg.base, GRACHT_MESSAGE_BLOCK);
    sys_usb_get_controller_count_result(GetGrachtClient(), &msg.base, ControllerCount);
    return OsSuccess;
}

OsStatus_t
UsbQueryController(
    _In_ int                Index,
    _In_ UsbHcController_t* Controller)
{
    int                      status;
    struct vali_link_message msg = VALI_MSG_INIT_HANDLE(GetUsbService());
    
    status = sys_usb_get_controller(GetGrachtClient(), &msg.base, Index);
    gracht_client_wait_message(GetGrachtClient(), &msg.base, GRACHT_MESSAGE_BLOCK);
    sys_usb_get_controller_result(GetGrachtClient(), &msg.base, (uint8_t*)Controller, sizeof(struct UsbHcController));
    return OsSuccess;
}
