#include <stdlib.h>
#include <unistd.h>
#include <khash.h>

#define PIPE_READ_FD 0
#define PIPE_WRITE_FD 1

typedef unsigned int microkit_channel;

KHASH_MAP_INIT_STR(process, struct process *)
KHASH_MAP_INIT_INT(channel, struct process *)
KHASH_MAP_INIT_STR(shared_memory, struct shared_memory *)

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

void create_process(const char *name);

void create_channel(const char *from, const char *to, microkit_channel ch);

void create_shared_memory(char *name, int size);

void add_shared_memory(const char *proc, char *name);

void free_resources();