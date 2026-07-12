#include "gateway_command_handler.h"
#include "m33_transport.h"
#include "ble_gatt_server.h"

#include <stdio.h>
#include <string.h>

static int on_ble_command(const char *cmd)
{
    printf("BLE RX: %s\n", cmd);
    return gateway_handle_command_text(cmd, VEHICLE_SOURCE_BLE_GATT);
}

int main(void)
{
    printf("STM32Car A35 Gateway started\n");

    //if (m33_transport_init() != 0) {
    //    printf("ERROR: failed to initialize M33 transport\n");
    //    return 1;
    //}

    if (ble_gatt_server_start(on_ble_command) != 0) {
        printf("ERROR: failed to start BLE GATT server\n");
        //m33_transport_deinit();
        return 1;
    }

    /*
     * Keep stdin for debugging.
     * BLE and stdin should both use gateway_handle_command_text().
     */
    char line[64];

    while (1) {
        printf("gateway> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }

        line[strcspn(line, "\r\n")] = '\0';

        if (strcmp(line, "quit") == 0) {
            break;
        }

        gateway_handle_command_text(line, VEHICLE_SOURCE_TEST);
    }

    ble_gatt_server_stop();
    //m33_transport_deinit();

    return 0;
}