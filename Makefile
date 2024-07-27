export C_INCLUDE_PATH=./include

SRC_DIR = ./user

.PHONY: all clean

SRCS = $(wildcard $(SRC_DIR)/*.c)
TARGETS = $(SRCS:.c=.so)

all: linux_main $(TARGETS)

# Compile each .c file into a .so file
$(SRC_DIR)/%.so: $(SRC_DIR)/%.c
	gcc -fPIC -shared -o $@ $<

linux_main:
	gcc src/linux_main.c src/pd_main.c -o linux_main

clean:
	rm -f linux_main $(SRC_DIR)/*.o $(SRC_DIR)/*.so
