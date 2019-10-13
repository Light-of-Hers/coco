CC			:= gcc
CFLAGS		:= -Wall -O0 -g

SRC	:= $(wildcard *.c)
OBJ	:= $(patsubst %.c, %.o, $(SRC))
BIN	:= a.out
LIB :=

RM	:= rm -rf

all: $(BIN)

run: all
	./$(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LIB)

clean:
	$(RM) $(BIN) $(OBJ)