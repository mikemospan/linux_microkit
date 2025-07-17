# The Linux Microkit

The seL4 Microkit, re‑implemented as a user‑space runtime on Linux. Written in Rust, with a custom C shared library (```libmicrokit.so```) and example user‑space programs.

---

## Table of Contents

1. [Overview](#overview)
2. [Features](#features)
3. [Prerequisites](#prerequisites)
4. [Directory Structure](#directory-structure)
5. [Building](#building)
6. [Usage](#usage)
7. [Example](#example)
8. [Contributing](#contributing)

---

## Overview

The Linux Microkit is responsible for:

1. Parsing a `.system` XML configuration (using `roxmltree`).  
2. Creating and mapping shared memory regions.  
3. Spawning and linking protection‑domain processes from compiled C `.so` plugins.  
4. Establishing unidirectional communication channels between domains.  
5. Running each process under a custom event‑handler loop for notifications and protected calls.  

All domains communicate via shared buffers and Unix primitives (`mmap`, `eventfd`, `pipe`, `clone`, etc.). The Microkit itself is implemented in Rust (`main.rs`), dynamically links into `libmicrokit.so`, and orchestrates the setup and execution of every domain.

---

## Features

- **XML validation & parsing**: ensure well‑formed `.system` files and extract memory regions, protection domains, and channels.  
- **Automated resource setup**: allocate shared memory regions, create domains with appropriate stack sizes and guard pages.  
- **Dynamic loading & execution**: link user‑space C plugins (`.so`) at runtime and invoke their `init`, `notified`, and `protected` entry points.  
- **Inter-domain communication**: establish and manage unidirectional channels with notifications and protected procedure calls.  
- **Robust error handling**: catch XML errors, symbol lookup failures, and constraint violations (e.g. stack size limits).  

---

## Prerequisites

- **Linux**
- **Rust toolchain**
- **GNU GCC** (with `-fPIC`, `-shared`)  
- **Make**  

---

## Directory Structure

```text
.
├── Cargo.toml              # Rust project manifest
├── src/
│   └── handler.c           # Event handler and process bootstrap
│   ├── loader.c            # Simplified loader as a C DLL
│   └── main.rs             # Rust XML parser implementation
│   ├── microkit.c          # Core microkit API (IPC, notify, PPC)
├── include/
│   └── handler.h           # Internal shared C API definitions
│   └── khash.h             # Hashmap library
│   └── microkit.h          # Public API used by each protection domain
├── example/
│   ├── *.c                 # Example user‑space programs
│   └── example.system      # XML configuration for example
├── build/                  # Output directory for shared objects
├── Makefile                # Build rules for C and Rust components
└── README.md               # You are here
```

---

## Building

1. **Clone the repository**:
    ```bash
    git clone git@github.com:mikemospan/linux_microkit.git
    cd linux_microkit
    ```
2. **Building the Linux Microkit**
    ```bash
    make all
    ```
3. **Cleanup**
    ```bash
    make clean
    ```
---

## Usage

Invoke the loader with the .system filename as the sole positional argument:

```
./linux_microkit <config.system>
```

- ```<config.system>``` can be any ```.system``` file in the ```./example/``` directory.
- The loader will always look for ```libmicrokit.so``` in ```./build/```.

---

## Example

This example shows a simple client–server interaction over a shared memory region and notification channel.
- A single memory region ```shared``` of size 4 KiB is defined.
- Two protection domains, **server** and **client**, map that region read–write at virtual address ```0x4000000```, exposing it in each as ```buffer```.
- The **client** sends notifications on channel ID 1 to the **server**, and the **server** can reply on channel 2.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<system>
    <memory_region name="shared" size="0x1000"/>

    <protection_domain name="server" stack_size="0x2000">
        <program_image path="server.elf"/>
        <map mr="shared" vaddr="0x4000000" perms="rw" cached="false" setvar_vaddr="buffer"/>
    </protection_domain>

    <protection_domain name="client">
        <program_image path="client.elf"/>
        <map mr="shared" vaddr="0x4000000" perms="rw" cached="false" setvar_vaddr="buffer"/>
    </protection_domain>

    <channel>
        <end pd="client" id="1"/>
        <end pd="server" id="2"/>
    </channel>
</system>
```

1. The loader allocates and maps the ```shared``` region into both domains.
2. After ```server.elf``` and ```client.elf``` are loaded; each domain’s ```init()``` is then called.
3. The client notifies the server to write data into ```buffer``` on channel 1.
4. The server’s event handler wakes, processes the request, updates the value of ```buffer``` and notifies back on channel 2.


---

# Contributing

1. Fork the repository
2. Create a feature branch:
    ```
    git checkout -b feature/my-feature
    ```
3. Commit your changes with descriptive messages
4. Push to your fork and open a Pull Request

Please keep C and Rust code style consistent.