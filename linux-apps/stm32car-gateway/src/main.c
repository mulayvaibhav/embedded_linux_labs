#include "vehicle_command.h"
#include "m33_transport.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#define MAX_LINE_LEN 128U
#define DEFAULT_SPEED_LIMIT_PCT 25U
#define DEFAULT_TTL_MS 500U

static uint32_t g_sequence_id = 0U;

static void trim_newline(char *str)
{
    size_t len = strlen(str);

    if ((len > 0U) && (str[len - 1U] == '\n'))
    {
        str[len - 1U] = '\0';
    }
}

static uint32_t get_timestamp_ms(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0U;
    }

    return (uint32_t)((ts.tv_sec * 1000U) + (ts.tv_nsec / 1000000U));
}

static void vehicle_command_init(vehicle_motion_command_t *cmd)
{
    memset(cmd, 0, sizeof(*cmd));

    cmd->version = VEHICLE_COMMAND_VERSION;
    cmd->source = VEHICLE_SOURCE_TEST;
    cmd->command_type = VEHICLE_CMD_MOTION;
    cmd->control_mode = VEHICLE_MODE_MANUAL;
    cmd->linear_x = 0;
    cmd->angular_z = 0;
    cmd->speed_limit_pct = DEFAULT_SPEED_LIMIT_PCT;
    cmd->ttl_ms = DEFAULT_TTL_MS;
    cmd->sequence_id = ++g_sequence_id;
    cmd->timestamp_ms = get_timestamp_ms();
}

static bool build_test_command_from_text(const char *text, vehicle_motion_command_t *cmd)
{
    vehicle_command_init(cmd);

    /*
     * This is only a Linux-side test adapter.
     * Final command interpretation and safety remain on the M33 side.
     */

    if (strcmp(text, "forward") == 0 || strcmp(text, "FORWARD") == 0)
    {
        cmd->command_type = VEHICLE_CMD_MOTION;
        cmd->linear_x = VEHICLE_AXIS_MAX;
        cmd->angular_z = 0;
        return true;
    }

    if (strcmp(text, "backward") == 0 || strcmp(text, "BACKWARD") == 0)
    {
        cmd->command_type = VEHICLE_CMD_MOTION;
        cmd->linear_x = VEHICLE_AXIS_MIN;
        cmd->angular_z = 0;
        return true;
    }

    if (strcmp(text, "left") == 0 || strcmp(text, "LEFT") == 0)
    {
        cmd->command_type = VEHICLE_CMD_MOTION;
        cmd->linear_x = 0;
        cmd->angular_z = VEHICLE_AXIS_MAX;
        return true;
    }

    if (strcmp(text, "right") == 0 || strcmp(text, "RIGHT") == 0)
    {
        cmd->command_type = VEHICLE_CMD_MOTION;
        cmd->linear_x = 0;
        cmd->angular_z = VEHICLE_AXIS_MIN;
        return true;
    }

    if (strcmp(text, "stop") == 0 || strcmp(text, "STOP") == 0)
    {
        cmd->command_type = VEHICLE_CMD_STOP;
        cmd->linear_x = 0;
        cmd->angular_z = 0;
        cmd->speed_limit_pct = 0U;
        return true;
    }

    if (strcmp(text, "estop") == 0 || strcmp(text, "ESTOP") == 0 ||
        strcmp(text, "emergency_stop") == 0 || strcmp(text, "EMERGENCY_STOP") == 0)
    {
        cmd->command_type = VEHICLE_CMD_EMERGENCY_STOP;
        cmd->control_mode = VEHICLE_MODE_ESTOP;
        cmd->linear_x = 0;
        cmd->angular_z = 0;
        cmd->speed_limit_pct = 0U;
        cmd->ttl_ms = 0U;
        return true;
    }

    if (strcmp(text, "heartbeat") == 0 || strcmp(text, "HEARTBEAT") == 0)
    {
        cmd->command_type = VEHICLE_CMD_HEARTBEAT;
        cmd->linear_x = 0;
        cmd->angular_z = 0;
        return true;
    }

    printf("DROP: unknown test input: %s\n", text);
    return false;
}

int main(void)
{
    char line[MAX_LINE_LEN];
    vehicle_motion_command_t cmd;

    printf("STM32Car A35 Gateway started\n");
    printf("Mode: stdin text -> vehicle_motion_command_t -> M33 transport\n");
    printf("Default speed limit: %u%%\n", DEFAULT_SPEED_LIMIT_PCT);
    printf("Default TTL: %u ms\n", DEFAULT_TTL_MS);
    printf("\n");
    printf("Test inputs: forward, backward, left, right, stop, estop, heartbeat\n");
    printf("\n");

    if (m33_transport_open() != 0)
    {
        printf("ERROR: failed to open M33 transport\n");
        return 1;
    }

    while (true)
    {
        printf("gateway> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            break;
        }

        trim_newline(line);

        if (build_test_command_from_text(line, &cmd))
        {
            if (m33_transport_send_vehicle_command(&cmd) != 0)
            {
                printf("ERROR: failed to send vehicle command to M33\n");
            }
        }
    }

    m33_transport_close();

    printf("STM32Car A35 Gateway exiting\n");

    return 0;
}