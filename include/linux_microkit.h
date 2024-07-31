#pragma once

#include <data_structures.h>

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

typedef unsigned int microkit_channel;

/* User provided functions */
void init(void);
void notified(microkit_channel ch);

/* Microkit API */
static inline void microkit_notify(microkit_channel ch) {
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