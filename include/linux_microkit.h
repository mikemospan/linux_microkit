#pragma once

#define __thread
#define MICROKIT_MAX_CHANNELS 62

typedef unsigned int microkit_channel;

struct process {
    int pid;
    char *stack_top;
    struct process *next;
};

struct channel {
    microkit_channel channel_id;
    int pipefd[2];
};

/* User provided functions */
void init(void);
void notified(microkit_channel ch);

static inline void microkit_notify(microkit_channel ch) {
    
}