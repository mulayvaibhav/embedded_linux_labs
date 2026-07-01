#include "m33_transport.h"

#include <stdio.h>

int m33_transport_open(void)
{
    printf("M33 transport opened: placeholder mode\n");
    return 0;
}

int m33_transport_send_vehicle_command(const vehicle_motion_command_t *cmd)
{
    if (cmd == NULL)
    {
        return -1;
    }

    /*
     * Placeholder.
     *
     * Later this function will send the binary vehicle_motion_command_t
     * to M33 over RPMsg.
     */

    printf("TX_TO_M33 vehicle_motion_command_t:\n");
    printf("  version=%u\n", cmd->version);
    printf("  source=%d\n", cmd->source);
    printf("  command_type=%d\n", cmd->command_type);
    printf("  control_mode=%d\n", cmd->control_mode);
    printf("  linear_x=%d\n", cmd->linear_x);
    printf("  angular_z=%d\n", cmd->angular_z);
    printf("  speed_limit_pct=%u\n", cmd->speed_limit_pct);
    printf("  ttl_ms=%u\n", cmd->ttl_ms);
    printf("  sequence_id=%u\n", cmd->sequence_id);
    printf("  timestamp_ms=%u\n", cmd->timestamp_ms);

    return 0;
}

void m33_transport_close(void)
{
    printf("M33 transport closed\n");
}
