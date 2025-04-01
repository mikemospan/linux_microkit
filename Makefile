MAKEFLAGS += -j$(shell nproc)

USR_DIR = ./example
BUILD_DIR = ./build
USRS = $(wildcard $(USR_DIR)/*.c)
SRCS = $(wildcard ./src/*.c)
USER_OBJS = $(patsubst $(USR_DIR)/%.c,$(BUILD_DIR)/%.so,$(USRS))

# Define the compiler and flags
CC = gcc
CFLAGS = -I./include -shared -fPIC -O3 -Wno-unused-result
LDFLAGS = -L$(BUILD_DIR) -lmicrokit -Wl,-rpath,$(BUILD_DIR)

.PHONY: all clean microkit

all: $(BUILD_DIR)/libmicrokit.so $(USER_OBJS) microkit

# Build the shared library from C sources
$(BUILD_DIR)/libmicrokit.so: $(SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^

# Build user objects from example C files
$(BUILD_DIR)/%.so: $(USR_DIR)/%.c $(BUILD_DIR)/libmicrokit.so | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(BUILD_DIR):
	mkdir -p $@

# Assumes Cargo.toml and src/main.rs exist in the project root.
microkit:
	cargo build --release
	cp target/release/linux_microkit .

clean:
	rm -f $(BUILD_DIR)/libmicrokit.so microkit
	rm -rf $(BUILD_DIR)
	rm -f linux_microkit
	cargo clean
