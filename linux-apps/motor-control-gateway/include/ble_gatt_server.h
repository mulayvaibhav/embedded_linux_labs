#ifndef BLE_GATT_SERVER_H
#define BLE_GATT_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*ble_command_callback_t)(const char *command_text);

int ble_gatt_server_start(ble_command_callback_t callback);
void ble_gatt_server_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_GATT_SERVER_H */
