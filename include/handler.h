#include <stdio.h>
#include <unistd.h>
#include <khash.h>

#define PAGE_SIZE 4096
#define STACK_SIZE PAGE_SIZE
#define MICROKIT_MAX_CHANNELS 62
#define PIPE_READ_FD 0
#define PIPE_WRITE_FD 1

typedef unsigned int microkit_channel;

KHASH_MAP_INIT_STR(process, struct process *)
KHASH_MAP_INIT_INT(channel, struct process *)
KHASH_MAP_INIT_STR(shared_memory, struct shared_memory *)

extern khash_t(process) *process_map;
extern khash_t(shared_memory) *shared_memory_map;

struct process {
    pid_t pid;
    char *stack_top;
    pid_t pipefd[2];
    khash_t(channel) *channel_map;
    struct shared_memory_stack *shared_memory;
    struct process *next;
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

struct info {
    struct process *process;
    char *path;
};

int event_handler(void *arg);