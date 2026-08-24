#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "udp_socket_client.h"

#define MAXBUF 1024

struct sockaddr_in udp_server_addr;

socklen_t addr_size;

int udp_socket_init( void )
{ 
  /* configure settings to communicate with remote UDP server */
  udp_server_addr.sin_family = AF_INET;
  udp_server_addr.sin_port = htons(4242); // server port
  udp_server_addr.sin_addr.s_addr = inet_addr("192.0.2.1"); // local-host
  memset(udp_server_addr.sin_zero, '\0', sizeof udp_server_addr.sin_zero);  
  addr_size = sizeof udp_server_addr;


  return 0;
}

int udp_send( vehicle_motion_command_wire_t * wire ) {
  int len, sockfd;
  vehicle_command_ack_wire_t ack;

  sockfd = socket(PF_INET, SOCK_DGRAM, 0); // create a UDP socket
  if(sockfd <= 0) { 
    printf("socket error !\n"); 
    return -1; 
  }

  //memset(buf, 0xab, MAXBUF); // set the entire buffer with 0xab (i.e 1010 1011 binary)
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

  sendto( sockfd, 
          wire, 
          sizeof(vehicle_motion_command_wire_t), 
          0, 
          (struct sockaddr *)&udp_server_addr, 
          addr_size); //send the data to server
  
  len = recvfrom( sockfd, 
                  &ack, 
                  sizeof( vehicle_command_ack_wire_t ), 
                  0, 
                  NULL, 
                  NULL ); // receive data from server
  
  printf("Received from server: %d bytes\n", len); 

  close(sockfd); //close socket file-descriptor
  
    printf("ACK received:\n");
    printf("  magic       = 0x%08x\n", ack.magic);
    printf("  version     = %u\n", ack.version);
    printf("  status      = %u\n", ack.status);
    printf("  error_code  = %u\n", ack.error_code);
    printf("  sequence_id = %u\n", ack.sequence_id);
    printf("  timestamp   = %u\n", ack.timestamp_ms);

  return 0;
}