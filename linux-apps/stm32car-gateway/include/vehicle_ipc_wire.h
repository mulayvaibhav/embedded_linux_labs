#ifndef VEHICLE_IPC_WIRE_H
#define VEHICLE_IPC_WIRE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VEHICLE_CMD_MAGIC      (0x56434D44u) /* 'VCMD' */
#define VEHICLE_ACK_MAGIC      (0x5641434Bu) /* 'VACK' */
#define VEHICLE_WIRE_VERSION   (1u)

typedef enum {
    VEHICLE_ACK_STATUS_OK = 0,
    VEHICLE_ACK_STATUS_INVALID_LENGTH = 1,
    VEHICLE_ACK_STATUS_INVALID_MAGIC = 2,
    VEHICLE_ACK_STATUS_INVALID_VERSION = 3,
    VEHICLE_ACK_STATUS_INVALID_RANGE = 4,
    VEHICLE_ACK_STATUS_REJECTED = 5,
    VEHICLE_ACK_STATUS_BUSY = 6,
} vehicle_ack_status_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;

    uint8_t version;
    uint8_t source;
    uint8_t command_type;
    uint8_t control_mode;

    int16_t linear_x;
    int16_t angular_z;

    uint8_t speed_limit_pct;
    uint8_t reserved0;

    uint16_t ttl_ms;

    uint32_t sequence_id;
    uint32_t timestamp_ms;
} vehicle_motion_command_wire_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint8_t version;
    uint8_t status;
    uint16_t error_code;
    uint32_t sequence_id;
    uint32_t timestamp_ms;
} vehicle_command_ack_wire_t;

_Static_assert(sizeof(vehicle_motion_command_wire_t) == 24,
               "vehicle_motion_command_wire_t size changed");

_Static_assert(sizeof(vehicle_command_ack_wire_t) == 16,
               "vehicle_command_ack_wire_t size changed");

#ifdef __cplusplus
}
#endif

#endif