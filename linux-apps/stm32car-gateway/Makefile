CC ?= gcc
CFLAGS ?= -Wall -Wextra -Werror -O2 -Iinclude

TARGET = stm32car-gateway

SRC = \
	src/main.c \
	src/m33_transport.c

all:
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)