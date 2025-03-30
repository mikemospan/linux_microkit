MAKEFLAGS += -j$(shell nproc)

USR_DIR = ./example
BUILD_DIR = ./build
USRS = $(wildcard $(USR_DIR)/*.c)
SRCS = $(wildcard ./src/*.c)
USER_OBJS = $(patsubst $(USR_DIR)/%.c,$(BUILD_DIR)/%.so,$(USRS))

.PHONY: all clean microkit

all: $(BUILD_DIR)/libmicrokit.so $(USER_OBJS) microkit

# Build the shared library from C sources
$(BUILD_DIR)/libmicrokit.so: $(SRCS) | $(BUILD_DIR)
	gcc -I./include -shared -o $@ -fPIC $^

# Build user objects from example C files
$(BUILD_DIR)/%.so: $(USR_DIR)/%.c $(BUILD_DIR)/libmicrokit.so | $(BUILD_DIR)
	gcc -I./include -shared -o $@ -fPIC $< -L$(BUILD_DIR) -lmicrokit -Wl,-rpath,$(BUILD_DIR)

$(BUILD_DIR):
	mkdir -p $@

# Build the Rust binary using Cargo.
# Assumes Cargo.toml and src/main.rs exist in the project root.
microkit:
	cargo build --release
	cp target/release/linux_microkit .

clean:
	rm -f $(BUILD_DIR)/libmicrokit.so microkit
	rm -rf $(BUILD_DIR)
	rm -f linux_microkit
	cargo clean
