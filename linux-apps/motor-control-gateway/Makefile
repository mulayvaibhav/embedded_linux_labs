APP := motor-control-gateway

CC ?= gcc
PKG_CONFIG ?= pkg-config

BUILD_DIR := build

PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags gio-2.0 glib-2.0)
PKG_LIBS   := $(shell $(PKG_CONFIG) --libs gio-2.0 glib-2.0)

APP_CFLAGS := \
    -Wall \
    -Wextra \
    -Iinclude \
    $(PKG_CFLAGS)

APP_LDLIBS := $(PKG_LIBS)

#
# Application-specific preprocessor flags
#
APP_CPPFLAGS :=

ifeq ($(UDP_SOCKET_ENABLED),1)
APP_CPPFLAGS += -DUDP_SOCKET_ENABLED
endif

ifeq ($(UART_COM_ENABLED),1)
APP_CPPFLAGS += -DUART_COM_ENABLED
endif

SRCS := \
    src/main.c \
    src/gateway_command_handler.c \
    src/m33_transport.c \
    src/ble_gatt_server.c \
    src/udp_socket_client.c \
    src/uart_serial_com.c

OBJS := $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

TARGET := $(BUILD_DIR)/$(APP)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS) $(APP_LDLIBS)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(APP_CPPFLAGS) $(CFLAGS) $(APP_CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

print-env:
	@echo "CC=$(CC)"
	@echo "CPPFLAGS=$(CPPFLAGS)"
	@echo "APP_CPPFLAGS=$(APP_CPPFLAGS)"
	@echo "CFLAGS=$(CFLAGS)"
	@echo "UART_COM_ENABLED=$(UART_COM_ENABLED)"
	@echo "UDP_SOCKET_ENABLED=$(UDP_SOCKET_ENABLED)"

.PHONY: all clean print-env