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
 * C Library - Driver Entry 
 */

#include "../libc/threads/tls.h"
#include <ddk/service.h>
#include <gracht/link/vali.h>
#include <gracht/server.h>
#include <internal/_ipc.h>
#include <internal/_utils.h>
#include <stdlib.h>
#include <ioset.h>

// Module Interface
extern void       GetModuleIdentifiers(unsigned int*, unsigned int*, unsigned int*, unsigned int*);
extern OsStatus_t OnLoad(void);
extern OsStatus_t OnUnload(void);
extern OsStatus_t OnEvent(struct ioset_event* event);

static gracht_server_t*         g_server     = NULL;
static struct gracht_link_vali* g_serverLink = NULL;

extern char**
__crt_init(
    _In_  thread_storage_t* threadStorage,
    _In_  int               isModule,
    _Out_ int*              argumentCount);

int __crt_get_server_iod(void)
{
    return gracht_link_get_handle((struct gracht_link*)g_serverLink);
}

gracht_server_t* __crt_get_module_server(void)
{
    return g_server;
}

void __crt_module_load(void)
{
    struct vali_link_message msg = VALI_MSG_INIT_HANDLE(GetDeviceService());
    unsigned int             vendorId;
    unsigned int             deviceId;
    unsigned int             class;
    unsigned int             subClass;
    
    GetModuleIdentifiers(&vendorId, &deviceId, &class, &subClass);
    
    // Call the driver load function 
    // - This will be run once, before loop
    if (OnLoad() != OsSuccess) {
        exit(-1);
    }
    
    if (vendorId != 0 || deviceId != 0 || class != 0 || subClass != 0) {
        sys_device_notify(GetGrachtClient(), &msg.base,
            GetNativeHandle(__crt_get_server_iod()),
            vendorId, deviceId, class, subClass);
    }
}

_Noreturn static void __crt_module_main(int setIod)
{
    struct ioset_event events[32];

    while (1) {
        int num_events = ioset_wait(setIod, &events[0], 32, 0);
        for (int i = 0; i < num_events; i++) {
            if (events[i].data.iod == gracht_client_iod(GetGrachtClient())) {
                gracht_client_wait_message(GetGrachtClient(), NULL, 0);
            }
            else {
                // Check if the driver had any IRQs registered
                if (OnEvent(&events[i]) == OsSuccess) {
                    continue;
                }
                gracht_server_handle_event(g_server, events[i].data.iod, events[i].events);
            }
        }
    }
}

void __CrtModuleEntry(void)
{
    thread_storage_t              threadStorage;
    gracht_server_configuration_t config;
    struct ipmsg_addr             addr = { .type = IPMSG_ADDRESS_HANDLE };
    int                           status;

    // initialize runtime environment
    __crt_init(&threadStorage, 1, NULL);

    // initialize the link
    status = gracht_link_vali_create(&g_serverLink);
    if (status) {
        exit(-1);
    }
    gracht_link_vali_set_listen(g_serverLink, 1);
    gracht_link_vali_set_address(g_serverLink, &addr);

    // initialize configurations for server
    gracht_server_configuration_init(&config);
    gracht_server_configuration_set_aio_descriptor(&config, ioset(0));

    status = gracht_server_create(&config, &g_server);
    if (status) {
        exit(status);
    }

    // listen to client events as well
    ioset_ctrl(config.set_descriptor, IOSET_ADD,
               gracht_client_iod(GetGrachtClient()),
               &(struct ioset_event) {
                       .data.iod = gracht_client_iod(GetGrachtClient()),
                       .events   = IOSETIN | IOSETCTL | IOSETLVT
               });

    // Wait for the device-manager service, as all modules require the device-manager
    // service to perform requests.
    if (WaitForDeviceService(2000) != OsSuccess) {
        exit(-1);
    }

    __crt_module_load();
    atexit((void (*)(void))OnUnload);
    at_quick_exit((void (*)(void))OnUnload);
    __crt_module_main(config.set_descriptor);
}
