# Makefile
CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS =
LDLIBS  = -lcrypto

BIN     = p1-dataProgram
SRC     = p1-dataProgram.c

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN) $(LDFLAGS) $(LDLIBS)

run-servidor: $(BIN)
	./$(BIN) servidor

run-cliente: $(BIN)
	./$(BIN) cliente

clean:
	rm -f $(BIN)

.PHONY: all clean run-servidor run-cliente


#make ./p1-dataProgram servidor

