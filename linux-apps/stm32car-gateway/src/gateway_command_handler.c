#include "gateway_command_handler.h"
#include "m33_transport.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

static uint32_t g_sequence_id = 0;

static uint32_t monotonic_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (uint32_t)((ts.tv_sec * 1000u) + (ts.tv_nsec / 1000000u));
}

static void trim_whitespace(char *s)
{
    char *start = s;
    char *end;

    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') {
        s++;
    }

    start = s;

    if (start[0] == '\0') {
        return;
    }

    end = start + strlen(start);

    while (end > start &&
           (end[-1] == ' ' || end[-1] == '\t' ||
            end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }

    *end = '\0';

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }
}

static void to_lowercase(char *s)
{
    while (*s != '\0') {
        *s = (char)tolower((unsigned char)*s);
        s++;
    }
}

static void fill_common(vehicle_motion_command_t *cmd,
                        vehicle_command_source_t source)
{
    memset(cmd, 0, sizeof(*cmd));

    cmd->version = VEHICLE_COMMAND_VERSION;
    cmd->source = source;
    cmd->control_mode = VEHICLE_MODE_MANUAL;
    cmd->speed_limit_pct = 25;
    cmd->ttl_ms = 500;
    cmd->timestamp_ms = monotonic_ms();
}

int gateway_handle_command_text(const char *input,
                                vehicle_command_source_t source)
{
    vehicle_motion_command_t cmd;
    char normalized[64];

    if (input == NULL) {
        return -1;
    }

    snprintf(normalized, sizeof(normalized), "%s", input);
    trim_whitespace(normalized);
    to_lowercase(normalized);

    if (normalized[0] == '\0') {
        return 0;
    }

    fill_common(&cmd, source);

    if (strcmp(normalized, "forward") == 0 ||
        strcmp(normalized, "fwd") == 0 ||
        strcmp(normalized, "up") == 0) {

        cmd.command_type = VEHICLE_CMD_MOTION;
        cmd.linear_x = 600;
        cmd.angular_z = 0;

    } else if (strcmp(normalized, "backward") == 0 ||
               strcmp(normalized, "back") == 0 ||
               strcmp(normalized, "reverse") == 0 ||
               strcmp(normalized, "down") == 0) {

        cmd.command_type = VEHICLE_CMD_MOTION;
        cmd.linear_x = -600;
        cmd.angular_z = 0;

    } else if (strcmp(normalized, "left") == 0) {

        cmd.command_type = VEHICLE_CMD_MOTION;
        cmd.linear_x = 300;
        cmd.angular_z = 600;

    } else if (strcmp(normalized, "right") == 0) {

        cmd.command_type = VEHICLE_CMD_MOTION;
        cmd.linear_x = 300;
        cmd.angular_z = -600;

    } else if (strcmp(normalized, "stop") == 0 ||
               strcmp(normalized, "center") == 0 ||
               strcmp(normalized, "release") == 0) {

        cmd.command_type = VEHICLE_CMD_STOP;
        cmd.linear_x = 0;
        cmd.angular_z = 0;

    } else if (strcmp(normalized, "estop") == 0 ||
               strcmp(normalized, "emergency") == 0) {

        cmd.command_type = VEHICLE_CMD_EMERGENCY_STOP;
        cmd.control_mode = VEHICLE_MODE_ESTOP;
        cmd.linear_x = 0;
        cmd.angular_z = 0;

    } else if (strcmp(normalized, "heartbeat") == 0 ||
               strcmp(normalized, "hb") == 0) {

        cmd.command_type = VEHICLE_CMD_HEARTBEAT;

    } else {
        printf("DROP: unknown input: %s\n", input);
        return -1;
    }

    cmd.sequence_id = ++g_sequence_id;

    return m33_transport_send_vehicle_command(&cmd);
}