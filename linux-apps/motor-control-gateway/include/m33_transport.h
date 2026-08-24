#ifndef M33_TRANSPORT_H
#define M33_TRANSPORT_H

#include "vehicle_command.h"

int m33_transport_open(void);
int m33_transport_send_vehicle_command(const vehicle_motion_command_t *cmd);
void m33_transport_close(void);

#endif
