#pragma once

#define __thread
#define MICROKIT_MAX_CHANNELS 62
#define PIPE_READ_FD 0
#define PIPE_WRITE_FD 1

typedef unsigned int microkit_channel;

struct process {
    int pid;
    char *stack_top;
    struct process *next;
};

struct channel {
    microkit_channel channel_id;
    int pipefd[2];
    struct channel *next;
};

struct channel *channel = NULL;

/* User provided functions */
void init(void);
void notified(microkit_channel ch);

/* Microkit API */
static inline void microkit_notify(microkit_channel ch) {
    struct channel *current = channel;
    while (current != NULL) {
        if (current->channel_id == ch) {
            write(current->pipefd[PIPE_WRITE_FD], &(current->channel_id), sizeof(microkit_channel));
            return;
        }
    }

    fprintf(stderr, "Channel id %u is not a valid channel\n", ch);
    exit(EXIT_FAILURE);
}