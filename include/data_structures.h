#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <khash.h>

#define PIPE_READ_FD 0
#define PIPE_WRITE_FD 1

typedef unsigned int microkit_channel;

KHASH_MAP_INIT_INT(channel, struct process *)

struct process {
    pid_t pid;
    char *stack_top;
    pid_t pipefd[2];
    khash_t(channel) *channel_map;
    struct process *next;
};

struct shared_memory {
    char *name;
    void *shared_buffer;
    int size;
    struct shared_memory *next;
};

struct info {
    struct process *process;
    const char *path;
};

struct process *create_process();

void create_channel(struct process *from, struct process *to, microkit_channel ch);

void create_shared_memory(char *name, int size);

void free_processes();

void free_shared_memory();