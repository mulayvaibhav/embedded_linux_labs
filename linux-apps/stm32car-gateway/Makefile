APP := stm32car-gateway

CC ?= gcc
PKG_CONFIG ?= pkg-config

PKG_CFLAGS := $(shell $(PKG_CONFIG) --cflags gio-2.0 glib-2.0)
PKG_LIBS   := $(shell $(PKG_CONFIG) --libs gio-2.0 glib-2.0)

CFLAGS += -Wall -Wextra -O2
CFLAGS += -Iinclude
CFLAGS += $(PKG_CFLAGS)

LDLIBS += $(PKG_LIBS)

ifeq ($(UDP_SOCKET_ENABLED),1)
CPPFLAGS += -DUDP_SOCKET_ENABLED
endif

SRCS := \
	src/main.c \
	src/gateway_command_handler.c \
	src/m33_transport.c \
	src/ble_gatt_server.c \
	src/udp_socket_client.c

OBJS := $(SRCS:.c=.o)

all: $(APP)

$(APP): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

clean:
	rm -f $(OBJS) $(APP)

print-env:
	@echo "CC=$(CC)"
	@echo "PKG_CONFIG=$(PKG_CONFIG)"
	@echo "PKG_CFLAGS=$(PKG_CFLAGS)"
	@echo "PKG_LIBS=$(PKG_LIBS)"

.PHONY: all clean print-env