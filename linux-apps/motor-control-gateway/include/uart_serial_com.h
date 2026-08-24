#ifndef UART_SERIAL_COM_H_
#define UART_SERIAL_COM_H_

#include "vehicle_ipc_wire.h"

#ifdef __cplusplus
extern "C" {
#endif

int uart_serial_com_init(const char *device);
int uart_serial_com_send( vehicle_motion_command_wire_t * wire );

#ifdef __cplusplus
}
#endif

#endif /* UART_SERIAL_COM_H_ */