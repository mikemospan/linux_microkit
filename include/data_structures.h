#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <search.h>

#define PIPE_READ_FD 0
#define PIPE_WRITE_FD 1

typedef unsigned int microkit_channel;

struct process {
    pid_t pid;
    char *stack_top;
    pid_t pipefd[2];
    struct channel **channel;
    struct process *next;
};

struct channel {
    microkit_channel channel_id;
    struct process *to;
    struct channel *next;
};

struct shared_memory {
    char *name;
    void *shared_buffer;
    int size;
    struct shared_memory *next;
};

struct process *create_process();

struct process *search_process(int pid);

struct channel *create_channel(struct process *to, microkit_channel ch);

struct shared_memory *create_shared_memory(char *name, int size);

void free_processes();

void free_channels();

void free_shared_memory();