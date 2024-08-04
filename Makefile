NPROCS = $(shell nproc)
MAKEFLAGS += -j$(NPROCS)

USR_DIR = ./user
SRC_DIR = ./src
USRS = $(wildcard $(USR_DIR)/*.c)
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:.c=.o)
USRS_TARGET = $(USRS:.c=.so)
CFLAGS = -g -Wall -I./include
LDFLAGS = -L. -lmain -Wl,-rpath,.

.PHONY: all clean

all: build cleanup

build: libmain.so $(USRS_TARGET)

%.o: %.c
	gcc $(CFLAGS) -fPIC -c $< -o $@

libmain.so: $(OBJS)
	gcc $(CFLAGS) -shared -o $@ $^
	@touch $@

$(USR_DIR)/%.so: $(USR_DIR)/%.c libmain.so
	gcc $(CFLAGS) -fPIC -shared -o $@ $< $(LDFLAGS)

cleanup: build
	@echo "Cleaning up object files..."
	@rm -f $(OBJS)

clean:
	rm -f main $(USR_DIR)/*.o $(USR_DIR)/*.so libmain.so $(OBJS) .cleanup