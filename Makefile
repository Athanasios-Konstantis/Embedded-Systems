#CC= gcc
CC= arm-linux-gnueabihf-gcc
INC_DIRS= -I/root/openssl-1.1.1t/openssl-arm/include -I/root/libwebsockets/include -I/root/libwebsockets/build 
LIB_DIRS= -L/root/openssl-1.1.1t/openssl-arm/lib

CFLAGS= -Wall -g $(INC_DIRS)
LDFLAGS= $(LIB_DIRS) -lwebsockets -pthread -lssl -lcrypto -ljansson

SRC= /root/apiconn.c
TARGET= test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

clean:
	rm -f $(TARGET)
