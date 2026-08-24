#ifndef UDP_SOCKET_CLIENT_H_
#define UDP_SOCKET_CLIENT_H_

#include "vehicle_ipc_wire.h"

#ifdef __cplusplus
extern "C" {
#endif

int udp_socket_init( void );
int udp_send( vehicle_motion_command_wire_t * wire );

#ifdef __cplusplus
}
#endif

#endif /* UDP_SOCKET_CLIENT_H_ */
