#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include "uart_serial_com.h"

static int uart_fd = -1;

/**
 * Configures the UART file descriptor.
 * Sets 115200 baud, 8 data bits, no parity, 1 stop bit (8N1), and disables flow control.
 */
int uart_serial_com_init(const char *device) {
    // Open the device file in Read/Write mode
    uart_fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart_fd == -1) {
        perror("UART: Failed to open device file");
        return -1;
    }

    // Clear O_NDELAY flag so read() blocks according to termios VMIN/VTIME settings
    fcntl(uart_fd, F_SETFL, 0);

    struct termios options;
    if (tcgetattr(uart_fd, &options) != 0) {
        perror("UART: Failed to get attributes");
        close(uart_fd);
        return -1;
    }

    // 1. Force full RAW binary mode (clears canonical mode, echo, line translations)
    cfmakeraw(&options);

    // 2. Set Baud Rate to 115200
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);

    // 3. Control Modes (c_cflag)
    options.c_cflag |= (CLOCAL | CREAD); // Enable receiver, ignore modem control lines
    options.c_cflag &= ~PARENB;          // No parity bit
    options.c_cflag &= ~CSTOPB;          // 1 stop bit
    options.c_cflag &= ~CSIZE;           // Clear current data size mask
    options.c_cflag |= CS8;              // 8 data bits
    options.c_cflag &= ~CRTSCTS;         // Disable hardware RTS/CTS flow control

    // 4. Read Timeout Parameters (c_cc):
    // VMIN = 0, VTIME = 20 (2.0 seconds):
    // read() will block waiting for data, but will time out after 2.0s if nothing arrives.
    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 20; // 20 x 100ms = 2.0s timeout

    // Apply configuration immediately
    if (tcsetattr(uart_fd, TCSANOW, &options) != 0) {
        perror("UART: Failed to set attributes");
        close(uart_fd);
        return -1;
    }

    // Flush any stale bytes sitting in the hardware RX/TX buffers
    tcflush(uart_fd, TCIOFLUSH);

    return uart_fd;
}

int uart_serial_com_send( vehicle_motion_command_wire_t * wire ) {
    // --- WRITE EXAMPLE ---
    //const char *tx_buffer = "Hello Embedded Linux USART!\r\n";
    if( wire == NULL ) {
        return -1;
    }
    vehicle_command_ack_wire_t ack = {0};

    printf("Send Commands:\n");
    printf("  magic       = 0x%08x\n",wire->magic);
    printf("  version     = %u\n", wire->version);
    printf("  source      = %u\n", wire->source);
    printf("  command_type= %u\n", wire->command_type);
    printf("  control_mode= %u\n", wire->control_mode);
    printf("  linear_x    = %u\n", wire->linear_x);
    printf("  angular_z   = %u\n", wire->angular_z);

    printf("  speed_limit = %u\n", wire->speed_limit_pct);
    printf("  reserved    = %u\n", wire->reserved0);
    printf("  ttl_ms      = %u\n", wire->ttl_ms);
    printf("  sequence_id = %u\n", wire->sequence_id);
    printf("  timestamp   = %u\n", wire->timestamp_ms);

    // Send the command
    ssize_t bytes_written = write(uart_fd, wire, sizeof(vehicle_motion_command_wire_t));
    if (bytes_written < 0) {
        perror("UART: Write failed");
        return -1;
    }

    // Ensure all bytes are physically pushed out the serial port
    tcdrain(uart_fd);

    printf("Waiting for ACK structure...\n");

    // --- ROBUST ACK READ LOOP ---
    size_t total_bytes_read = 0;
    size_t target_size = sizeof(vehicle_command_ack_wire_t);
    uint8_t *ack_buffer = (uint8_t *)&ack;

    while (total_bytes_read < target_size) {
        ssize_t bytes_read = read(
            uart_fd, 
            ack_buffer + total_bytes_read, 
            target_size - total_bytes_read
        );

        if (bytes_read < 0) {
            perror("UART: Read error");
            break;
        } 
        else if (bytes_read == 0) {
            // VTIME 2.0s timeout expired with no new data arriving
            printf("UART: Timeout waiting for ACK! (Received %zu/%zu bytes)\n", 
                   total_bytes_read, target_size);
            break;
        }

        total_bytes_read += bytes_read;
    }

    // Check if the complete packet was received
    if (total_bytes_read == target_size) {
        printf("ACK received successfully:\n");
        printf("  magic       = 0x%08x\n", ack.magic);
        printf("  version     = %u\n", ack.version);
        printf("  status      = %u\n", ack.status);
        printf("  error_code  = %u\n", ack.error_code);
        printf("  sequence_id = %u\n", ack.sequence_id);
        printf("  timestamp   = %u\n", ack.timestamp_ms);
    }
    
    return (total_bytes_read == target_size) ? 0 : -1;
}
