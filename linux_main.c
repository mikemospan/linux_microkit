#define _GNU_SOURCE

#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <poll.h>

#include "include/linux_microkit.h"

#define PAGE_SIZE 4096
#define STACK_SIZE PAGE_SIZE
#define SHARED_MEM_SIZE PAGE_SIZE

void *shared_buffer;
struct channel *channel;

/* USER PROVIDED FUNCTIONS */

__attribute__((weak)) void init(void) {
    printf("==CHILD PROCESS INITIALISED==\n");
}

__attribute__((weak)) void notified(microkit_channel ch) {
    printf("Notification received by process with id: %d, on channel: %d!\n", getpid(), ch);
}

/* MAIN CHILD FUNCTION ACTING AS AN EVENT HANDLER */

int child_main(__attribute__((unused)) void *arg) {
    init();
    printf("Child process received the message \"%s\" from shared buffer.\n", (char *) shared_buffer);
    
    // Declare and initialise necessary variables for polling from pipe
    int ready;
    nfds_t nfds = MICROKIT_MAX_CHANNELS;
    microkit_channel buf;
    struct pollfd *fds = calloc(nfds, sizeof(struct pollfd));
    if (fds == NULL) {
        fprintf(stderr, "Error on allocating poll fds\n");
        return -1;
    }
    fds[0].fd = channel->pipefd[0];
    fds[0].events = POLLIN;

    // Main event loop for polling for changes in pipes and calling notified accordingly
    while (ready = ppoll(fds, nfds, NULL, NULL)) {
        for (nfds_t i = 0; i < nfds; i++) {
            if (fds[i].revents) {
                read(fds[i].fd, &buf, sizeof(microkit_channel));
                notified(buf);
            }
        }
    }
    return 0;
}

static struct process *create_process() {
    struct process *process = malloc(sizeof(struct process));
    process->next = NULL;

    char *stack = malloc(STACK_SIZE);
    if (stack == NULL) {
        fprintf(stderr, "Error on allocating stack\n");
        return NULL;
    }
    process->stack_top = stack + STACK_SIZE;

    process->pid = clone(child_main, process->stack_top, SIGCHLD, NULL);
    if (process->pid == -1) {
        fprintf(stderr, "Error on cloning\n");
        return NULL;
    }

    return process;
}

/* MAIN FUNCTION ACTING AS A SETUP FOR ALL NECESSARY OBJECTS */

int main(void) {
    // Create our shared memory and initialise it
    shared_buffer = mmap(NULL, SHARED_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (shared_buffer == MAP_FAILED) {
        fprintf(stderr, "Error on allocating shared buffer\n");
        return -1;
    }
    strncpy(shared_buffer, "Hello World!", SHARED_MEM_SIZE);

    // Create our pipes and channels for interprocess communication
    channel = mmap(NULL, sizeof(struct channel), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (channel == MAP_FAILED) {
        fprintf(stderr, "Error on allocating shared buffer\n");
        return -1;
    }
    channel->channel_id = 1;
    if (pipe(channel->pipefd) == -1) {
        fprintf(stderr, "Error on creating pipe\n");
        return -1;
    }

    struct process *process_list = create_process();

    // Replace this with user-level microkit_notify
    write(channel->pipefd[1], &(channel->channel_id), sizeof(microkit_channel));

    // Wait for the child process to finish before leaving
    if (waitpid(process_list->pid, NULL, 0) == -1) {
        fprintf(stderr, "Error on waitpid\n");
        return -1;
    }

    // Unmap and free all used heap memory
    munmap(channel, sizeof(struct channel));
    munmap(shared_buffer, SHARED_MEM_SIZE);
    free(process_list->stack_top - STACK_SIZE);

    return 0;
}