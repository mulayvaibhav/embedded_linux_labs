/* gateway_command_handler.h */
#ifndef GATEWAY_COMMAND_HANDLER_H
#define GATEWAY_COMMAND_HANDLER_H

#include "vehicle_command.h"

int gateway_handle_command_text(const char *input,
                                vehicle_command_source_t source);

#endif