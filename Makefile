MAKEFLAGS += -j$(shell nproc)

USR_DIR = ./user
USRS = $(wildcard $(USR_DIR)/*.c)
SRCS = $(wildcard ./src/*.c)

.PHONY: all clean

all: libmain.so $(USRS:.c=.so)

libmain.so:
	gcc -I./include -shared -o libmain.so -fPIC $(SRCS)

$(USR_DIR)/%.so: $(USR_DIR)/%.c libmain.so
	gcc -I./include -shared -o $@ -fPIC $< -L. -lmain -Wl,-rpath,.

clean:
	rm -f $(USR_DIR)/*.so libmain.so