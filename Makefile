export C_INCLUDE_PATH=./include

USR_DIR = ./user
SRC_DIR = ./src

USRS = $(wildcard $(USR_DIR)/*.c)
SRCS = $(wildcard $(SRC_DIR)/*.c)

USRS_TARGET = $(USRS:.c=.so)

.PHONY: all clean

all: main $(USRS_TARGET)

# Compile each .c file into a .so file
$(USR_DIR)/%.so: $(USR_DIR)/%.c
	gcc -fPIC -shared -o $@ $<

main: $(SRCS)
	gcc -rdynamic -o $@ $^ -ldl

clean:
	rm -f main $(USR_DIR)/*.o $(USR_DIR)/*.so
