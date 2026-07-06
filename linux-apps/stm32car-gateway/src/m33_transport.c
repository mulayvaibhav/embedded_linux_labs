#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "vehicle_command.h"
#include "vehicle_ipc_wire.h"

#define M33_RPMSG_CMD_DEV "/dev/rpmsg0"
#define M33_ACK_TIMEOUT_MS 200

static vehicle_motion_command_wire_t to_wire_command(
    const vehicle_motion_command_t *cmd)
{
    vehicle_motion_command_wire_t wire = {
        .magic = VEHICLE_CMD_MAGIC,
        .version = VEHICLE_WIRE_VERSION,

        .source = (uint8_t)cmd->source,
        .command_type = (uint8_t)cmd->command_type,
        .control_mode = (uint8_t)cmd->control_mode,

        .linear_x = cmd->linear_x,
        .angular_z = cmd->angular_z,

        .speed_limit_pct = cmd->speed_limit_pct,
        .reserved0 = 0,

        .ttl_ms = cmd->ttl_ms,

        .sequence_id = cmd->sequence_id,
        .timestamp_ms = cmd->timestamp_ms,
    };

    return wire;
}

int m33_transport_send_vehicle_command(const vehicle_motion_command_t *cmd)
{
    if (cmd == NULL) {
        return -1;
    }

    vehicle_motion_command_wire_t wire = to_wire_command(cmd);

    int fd = open(M33_RPMSG_CMD_DEV, O_RDWR | O_CLOEXEC | O_NONBLOCK);
    if (fd < 0) {
        fprintf(stderr, "open(%s) failed: %s\n",
                M33_RPMSG_CMD_DEV, strerror(errno));
        return -1;
    }

    ssize_t written = write(fd, &wire, sizeof(wire));
    if (written != (ssize_t)sizeof(wire)) {
        fprintf(stderr, "write(%s) failed: %s\n",
                M33_RPMSG_CMD_DEV, strerror(errno));
        close(fd);
        return -1;
    }

    struct pollfd pfd = {
        .fd = fd,
        .events = POLLIN,
    };

    int poll_ret = poll(&pfd, 1, M33_ACK_TIMEOUT_MS);
    if (poll_ret == 0) {
        fprintf(stderr, "M33 ACK timeout for seq=%u\n", wire.sequence_id);
        close(fd);
        return -1;
    }

    if (poll_ret < 0) {
        fprintf(stderr, "poll(%s) failed: %s\n",
                M33_RPMSG_CMD_DEV, strerror(errno));
        close(fd);
        return -1;
    }

    vehicle_command_ack_wire_t ack;
    ssize_t rd = read(fd, &ack, sizeof(ack));
    if (rd != (ssize_t)sizeof(ack)) {
        fprintf(stderr, "Invalid ACK size: %zd\n", rd);
        close(fd);
        return -1;
    }

    close(fd);

    printf("ACK received:\n");
    printf("  magic       = 0x%08x\n", ack.magic);
    printf("  version     = %u\n", ack.version);
    printf("  status      = %u\n", ack.status);
    printf("  error_code  = %u\n", ack.error_code);
    printf("  sequence_id = %u\n", ack.sequence_id);
    printf("  timestamp   = %u\n", ack.timestamp_ms);

    if (ack.magic != VEHICLE_ACK_MAGIC) {
        fprintf(stderr, "Invalid ACK magic: 0x%08x\n", ack.magic);
        return -1;
    }

    if (ack.version != VEHICLE_WIRE_VERSION) {
        fprintf(stderr, "Invalid ACK version: %u\n", ack.version);
        return -1;
    }

    if (ack.sequence_id != wire.sequence_id) {
        fprintf(stderr, "ACK sequence mismatch: sent=%u received=%u\n",
                wire.sequence_id, ack.sequence_id);
        return -1;
    }

    if (ack.status != VEHICLE_ACK_STATUS_OK) {
        fprintf(stderr, "M33 rejected command seq=%u status=%u error=%u\n",
                ack.sequence_id, ack.status, ack.error_code);
        return -1;
    }

    return 0;
}



int m33_transport_open(void) {

    return 0;
}

void m33_transport_close(void) {

}