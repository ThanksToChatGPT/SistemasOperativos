# Makefile
CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS =
LDLIBS  = -lcrypto

BIN     = p2-dataProgram
SRC1     = p2-dataProgram.c
SRC2     = serverSocket.c
SRC3     = clientSocket.c

all: $(BIN)

$(BIN): $(SRC1) $(SRC2) $(SRC3)
	$(CC) $(CFLAGS) $(SRC1) $(SRC2) $(SRC3) -o $(BIN) $(LDFLAGS) $(LDLIBS)

server: $(BIN)
	./$(BIN) server

client: $(BIN)
	./$(BIN) client

clean:
	rm -f $(BIN)

.PHONY: all clean run-servidor run-cliente


#make ./p1-dataProgram servidor

