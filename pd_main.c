#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <poll.h>

#include "include/linux_microkit.h"
#include "pd_main.h"

/* USER PROVIDED FUNCTIONS */

__attribute__((weak)) void init(void) {
    printf("==CHILD PROCESS INITIALISED==\n");
}

__attribute__((weak)) void notified(microkit_channel ch) {
    printf("Notification received by process with id: %d, on channel: %u!\n", getpid(), ch);
}

/* HELPER FUNCTIONS */

static struct process *search_process(int pid) {
    struct process *current = process_list;
    while (current != NULL && current->pid != pid) {
        current = current->next;
    }
    return current;
}

/* MAIN CHILD FUNCTION ACTING AS AN EVENT HANDLER */

int child_main(__attribute__((unused)) void *arg) {
    init();

    struct process *process = search_process(getpid());
    printf("Child process received the message \"%s\" from shared buffer.\n", (char *) (*process->shared_memory)->shared_buffer);
    
    // Declare and initialise necessary variables for polling from pipe
    struct pollfd *fds = calloc(MICROKIT_MAX_CHANNELS, sizeof(struct pollfd));
    if (fds == NULL) {
        fprintf(stderr, "Error on allocating poll fds\n");
        return -1;
    }

    struct channel **curr = process->channel;
    for (int i = 0; curr[i] != NULL; i++) {
        fds[i].fd = curr[i]->pipefd[PIPE_READ_FD];
        fds[i].events = POLLIN;
    }

    // Main event loop for polling for changes in pipes and calling notified accordingly
    while (ppoll(fds, MICROKIT_MAX_CHANNELS, NULL, NULL)) {
        for (int i = 0; i < MICROKIT_MAX_CHANNELS; i++) {
            if (fds[i].revents & POLLIN) {
                microkit_channel buf;
                read(fds[i].fd, &buf, sizeof(microkit_channel));
                notified(buf);
            }
        }
    }

    return 0;
}