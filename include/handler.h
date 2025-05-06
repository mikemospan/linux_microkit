#pragma once

#include <stdio.h>
#include <unistd.h>
#include <khash.h>

#define MICROKIT_MAX_PDS 63
#define IPC_BUFFER_SIZE 64
#define PIPE_READ_FD 0
#define PIPE_WRITE_FD 1

typedef unsigned long microkit_channel;
typedef long seL4_Word;
typedef seL4_Word microkit_msginfo;

typedef struct process process_t;
typedef struct shared_memory_stack shared_memory_stack_t;
typedef struct shared_memory shared_memory_t;
typedef struct message message_t;

/**
 * Some of the fields within these structs are not owned by the C implementation, but rather by the Rust.
 * These fields are prefixed with an underscore to indicate that they should not be freed.
 */

KHASH_MAP_INIT_STR(process, process_t *) // Stores Rust owned strings as keys
KHASH_MAP_INIT_INT(channel, process_t *)
KHASH_MAP_INIT_STR(shared_memory, shared_memory_t *) // Stores Rust owned strings as keys

struct process {
    pid_t pid;
    char *_path;

    char *stack_top;
    shared_memory_stack_t *shared_memory;

    khash_t(channel) *channel_id_to_process;
    pid_t notification; // fd for receiving notifications
    pid_t send_pipe[2]; // Send pipe for PPC
    pid_t receive_pipe[2]; // Receive pipe for PPC

    seL4_Word *ipc_buffer;
};

struct shared_memory_stack {
    shared_memory_t *shm;
    const char *_varname;
    shared_memory_stack_t *next;
};

struct shared_memory {
    char *_name;
    void *shared_buffer;
    int size;
};

struct message {
    microkit_channel ch;
    microkit_msginfo msginfo;
    pid_t send_back;
};

int event_handler(void *arg);