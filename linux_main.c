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
    fds[0].fd = channel->pipefd[PIPE_READ_FD];
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

static void create_process(struct process **process_list) {
    struct process *current = *process_list;
    if (current == NULL) {
        *process_list = malloc(sizeof(struct process));
        (*process_list)->next = NULL;
        current = *process_list;
    } else {
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = malloc(sizeof(struct process));
        current = current->next;
    }

    char *stack = malloc(STACK_SIZE);
    if (stack == NULL) {
        fprintf(stderr, "Error on allocating stack\n");
        exit(EXIT_FAILURE);
    }
    current->stack_top = stack + STACK_SIZE;

    current->pid = clone(child_main, current->stack_top, SIGCHLD, NULL);
    if (current->pid == -1) {
        fprintf(stderr, "Error on cloning\n");
        exit(EXIT_FAILURE);
    }
}

static void create_channel(microkit_channel ch) {
    struct channel *current = channel;
    if (current == NULL) {
        channel = mmap(NULL, sizeof(struct channel), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
        if (channel == MAP_FAILED) {
            fprintf(stderr, "Error on allocating shared buffer\n");
            exit(EXIT_FAILURE);
        }
        current = channel;
    } else {
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = mmap(NULL, sizeof(struct channel), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
        current = current->next;
    }

    current->channel_id = ch;
    current->next = NULL;
    if (pipe(current->pipefd) == -1) {
        fprintf(stderr, "Error on creating pipe\n");
        exit(EXIT_FAILURE);
    }
}

static void free_processes(struct process *process_list) {
    while (process_list != NULL) {
        struct process *next = process_list->next;
        free(process_list->stack_top - STACK_SIZE);
        free(process_list);
        process_list = next;
    }
}

static void free_channels() {
    while (channel != NULL) {
        struct channel *next = channel->next;
        free(channel);
        channel = next;
    }
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
    create_channel(1);

    // Create our processes to act in place of protection domains
    struct process *process_list = NULL;
    create_process(&process_list);

    // Testing a microkit_notify
    microkit_notify(channel->channel_id);

    // Wait for the child process to finish before leaving
    if (waitpid(process_list->pid, NULL, 0) == -1) {
        fprintf(stderr, "Error on waitpid\n");
        return -1;
    }

    // Unmap and free all used heap memory
    munmap(channel, sizeof(struct channel));
    munmap(shared_buffer, SHARED_MEM_SIZE);
    free_channels();
    free_processes(process_list);

    return 0;
}