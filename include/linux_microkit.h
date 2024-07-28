#pragma once

#include <stdlib.h>
#include <unistd.h>

#define __thread
#define MICROKIT_MAX_CHANNELS 62
#define MICROKIT_MAX_SHARED_MEMORY 62
#define PIPE_READ_FD 0
#define PIPE_WRITE_FD 1

typedef unsigned int microkit_channel;

struct process {
    pid_t pid;
    char *stack_top;
    pid_t pipefd[2];
    struct shared_memory **shared_memory;
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

extern struct process *process_list;
extern struct channel *channel_list;
extern struct shared_memory *shared_memory_list;

/* User provided functions */
void init(void);
void notified(microkit_channel ch);

/* Microkit API */
static inline void microkit_notify(microkit_channel ch) {
    extern struct process *search_process(int pid);
    struct channel **current = search_process(getpid())->channel;
    for (int i = 0; current[i] != NULL; i++) {
        if (current[i]->channel_id == ch) {
            write(current[i]->to->pipefd[PIPE_WRITE_FD], &(current[i]->channel_id), sizeof(microkit_channel));
            return;
        }
    }

    printf("Channel id %u is not a valid channel\n", ch);
    exit(EXIT_FAILURE);
}