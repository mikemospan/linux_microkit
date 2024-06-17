#pragma once

#define __thread
#define MICROKIT_MAX_CHANNELS 62
#define PIPE_READ_FD 0
#define PIPE_WRITE_FD 1

typedef unsigned int microkit_channel;

struct process {
    pid_t pid;
    char *stack_top;
    struct shared_memory *shared_memory;
    struct channel *channel;
    struct process *next;
};

struct channel {
    microkit_channel channel_id;
    pid_t pipefd[2];
    struct channel *next;
};

struct shared_memory {
    char *name;
    void *shared_buffer;
    int size;
    struct shared_memory *next;
};

struct process *process_list = NULL;
struct channel *channel_list = NULL;
struct shared_memory *shared_memory_list = NULL;

/* User provided functions */
void init(void);
void notified(microkit_channel ch);

/* Microkit API */
static inline void microkit_notify(microkit_channel ch) {
    struct channel *current = channel_list;
    while (current != NULL) {
        if (current->channel_id == ch) {
            write(current->pipefd[PIPE_WRITE_FD], &(current->channel_id), sizeof(microkit_channel));
            return;
        }
    }

    fprintf(stderr, "Channel id %u is not a valid channel\n", ch);
    exit(EXIT_FAILURE);
}