#include <linux_microkit.h>
#include <data_structures.h>

#include <stdio.h>

void microkit_notify(microkit_channel ch) {
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