#include <stdio.h>
#include <unistd.h>
#include <khash.h>

#define PAGE_SIZE 4096
#define STACK_SIZE PAGE_SIZE
#define MICROKIT_MAX_PDS 63
#define IPC_BUFFER_SIZE 64
#define PIPE_READ_FD 0
#define PIPE_WRITE_FD 1

typedef unsigned int microkit_channel;
typedef long seL4_Word;
typedef seL4_Word microkit_msginfo;

KHASH_MAP_INIT_STR(process, struct process *)
KHASH_MAP_INIT_INT(channel, struct process *)
KHASH_MAP_INIT_STR(shared_memory, struct shared_memory *)

struct process {
    pid_t pid;
    char *path;

    char *stack_top;
    struct shared_memory_stack *shared_memory;

    khash_t(channel) *channel_id_to_process;
    pid_t send_pipe[2];
    pid_t receive_pipe[2];

    seL4_Word *ipc_buffer;
};

struct shared_memory_stack {
    struct shared_memory *shm;
    struct shared_memory_stack *next;
};

struct shared_memory {
    char *name;
    void *shared_buffer;
    int size;
};

struct message {
    microkit_channel ch;
    microkit_msginfo msginfo;
    pid_t send_back;
};

int event_handler(void *arg);