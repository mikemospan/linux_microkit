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
    printf("Notification received by process with id: %d, on channel: %d!\n", getpid(), ch);
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
    printf("Child process received the message \"%s\" from shared buffer.\n", (char *) process->shared_memory->shared_buffer);
    
    // Declare and initialise necessary variables for polling from pipe
    int ready;
    microkit_channel buf;
    struct pollfd *fds = calloc(MICROKIT_MAX_CHANNELS, sizeof(struct pollfd));
    if (fds == NULL) {
        fprintf(stderr, "Error on allocating poll fds\n");
        return -1;
    }
    fds[0].fd = channel_list->pipefd[PIPE_READ_FD];
    fds[0].events = POLLIN;

    // Main event loop for polling for changes in pipes and calling notified accordingly
    while (ready = ppoll(fds, MICROKIT_MAX_CHANNELS, NULL, NULL)) {
        for (int i = 0; i < MICROKIT_MAX_CHANNELS; i++) {
            if (fds[i].revents) {
                read(fds[i].fd, &buf, sizeof(microkit_channel));
                notified(buf);
            }
        }
    }

    return 0;
}