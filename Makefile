MAKEFLAGS += -j$(shell nproc)

USR_DIR = ./example
BUILD_DIR = $(USR_DIR)/build
USRS = $(wildcard $(USR_DIR)/*.c)
SRCS = $(wildcard ./src/*.c)
USER_OBJS = $(patsubst $(USR_DIR)/%.c,$(BUILD_DIR)/%.so,$(USRS))

.PHONY: all clean

all: libmicrokit.so $(USER_OBJS)

libmicrokit.so: $(SRCS)
	gcc -I./include -shared -o $@ -fPIC $^

$(BUILD_DIR)/%.so: $(USR_DIR)/%.c libmicrokit.so | $(BUILD_DIR)
	gcc -I./include -shared -o $@ -fPIC $< -L. -lmicrokit -Wl,-rpath,.

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -f libmicrokit.so
	rm -rf $(BUILD_DIR)