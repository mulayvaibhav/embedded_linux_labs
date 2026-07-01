#ifndef VEHICLE_COMMAND_H
#define VEHICLE_COMMAND_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VEHICLE_COMMAND_VERSION        (1u)
#define VEHICLE_AXIS_MIN               (-1000)
#define VEHICLE_AXIS_MAX               (1000)
#define VEHICLE_SPEED_MIN_PCT          (0u)
#define VEHICLE_SPEED_MAX_PCT          (100u)

typedef enum {
    VEHICLE_SOURCE_UNKNOWN = 0,
    VEHICLE_SOURCE_HC05_UART,
    VEHICLE_SOURCE_BLE_GATT,
    VEHICLE_SOURCE_ROS2,
    VEHICLE_SOURCE_AUTONOMOUS,
    VEHICLE_SOURCE_TEST,
    VEHICLE_SOURCE_SAFETY
} vehicle_command_source_t;

typedef enum {
    VEHICLE_CMD_MOTION = 0,
    VEHICLE_CMD_STOP,
    VEHICLE_CMD_EMERGENCY_STOP,
    VEHICLE_CMD_SET_SPEED,
    VEHICLE_CMD_HEARTBEAT
} vehicle_command_type_t;

typedef enum {
    VEHICLE_MODE_MANUAL = 0,
    VEHICLE_MODE_AUTONOMOUS,
    VEHICLE_MODE_ASSISTED,
    VEHICLE_MODE_ESTOP
} vehicle_control_mode_t;

typedef struct {
    uint8_t version;

    vehicle_command_source_t source;
    vehicle_command_type_t command_type;
    vehicle_control_mode_t control_mode;

    int16_t linear_x;
    int16_t angular_z;

    uint8_t speed_limit_pct;
    uint16_t ttl_ms;

    uint32_t sequence_id;
    uint32_t timestamp_ms;
} vehicle_motion_command_t;

typedef struct {
    int16_t left_pct;
    int16_t right_pct;

    bool emergency_stop_active;
    bool command_timeout_active;
} vehicle_motor_output_t;

#ifdef __cplusplus
}
#endif

#endif /* VEHICLE_COMMAND_H */
