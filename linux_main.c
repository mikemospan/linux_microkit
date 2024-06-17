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

/* USER PROVIDED FUNCTIONS */

__attribute__((weak)) void init(void) {
    printf("==CHILD PROCESS INITIALISED==\n");
}

__attribute__((weak)) void notified(microkit_channel ch) {
    printf("Notification received by process with id: %d, on channel: %d!\n", getpid(), ch);
}

/* MAIN CHILD FUNCTION ACTING AS AN EVENT HANDLER */

static struct process *search_process(pid_t pid) {
    struct process *current = process_list;
    while (current != NULL && current->pid != pid) {
        current = current->next;
    }
    return current;
}

int child_main(__attribute__((unused)) void *arg) {
    init();

    struct process *process = search_process(getpid());
    printf("Child process received the message \"%s\" from shared buffer.\n", (char *) process->shared_memory->shared_buffer);
    
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
        *process_list = mmap(NULL, sizeof(struct process), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
        if (*process_list == MAP_FAILED) {
            fprintf(stderr, "Error on allocating shared buffer\n");
            exit(EXIT_FAILURE);
        }
        (*process_list)->next = NULL;
        current = *process_list;
    } else {
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = mmap(NULL, sizeof(struct process), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
        if (current->next == MAP_FAILED) {
            fprintf(stderr, "Error on allocating shared buffer\n");
            exit(EXIT_FAILURE);
        }
        current = current->next;
    }

    char *stack = malloc(STACK_SIZE);
    if (stack == NULL) {
        fprintf(stderr, "Error on allocating stack\n");
        exit(EXIT_FAILURE);
    }
    current->stack_top = stack + STACK_SIZE;
    current->pid = -1;
}

static void run_process(struct process *process) {
    process->pid = clone(child_main, process->stack_top, SIGCHLD, NULL);
    if (process->pid == -1) {
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
        if (current->next == MAP_FAILED) {
            fprintf(stderr, "Error on allocating channel\n");
            exit(EXIT_FAILURE);
        }
        current = current->next;
    }

    current->channel_id = ch;
    current->next = NULL;
    if (pipe(current->pipefd) == -1) {
        fprintf(stderr, "Error on creating pipe\n");
        exit(EXIT_FAILURE);
    }
}

static void create_shared_memory(struct process *process, int size) {
    struct shared_memory *current = process->shared_memory;
    if (current == NULL) {
        process->shared_memory = malloc(sizeof(struct shared_memory));
        if (process->shared_memory == NULL) {
            fprintf(stderr, "Error on allocating shared buffer\n");
            exit(EXIT_FAILURE);
        }
        current = process->shared_memory;
    } else {
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = malloc(sizeof(struct shared_memory));
        if (current->next == NULL) {
            fprintf(stderr, "Error on allocating shared buffer\n");
            exit(EXIT_FAILURE);
        }
        current = current->next;
    }

    current->shared_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (current->shared_buffer == MAP_FAILED) {
        fprintf(stderr, "Error on allocating shared buffer\n");
        exit(EXIT_FAILURE);
    }
    current->size = size;
    current->next = NULL;
}

static void free_processes(struct process *process_list) {
    while (process_list != NULL) {
        struct process *next = process_list->next;
        while (process_list->shared_memory != NULL) {
            struct shared_memory *next = process_list->shared_memory->next;
            munmap(process_list->shared_memory->shared_buffer, process_list->shared_memory->size);
            munmap(process_list->shared_memory, sizeof(struct shared_memory));
            process_list->shared_memory = next;
        }
        free(process_list->stack_top - STACK_SIZE);
        munmap(process_list, sizeof(struct process));
        process_list = next;
    }
}

static void free_channels() {
    while (channel != NULL) {
        struct channel *next = channel->next;
        munmap(channel, sizeof(struct channel));
        channel = next;
    }
}

/* MAIN FUNCTION ACTING AS A SETUP FOR ALL NECESSARY OBJECTS */

int main(void) {
    // Create our processes to act in place of protection domains
    create_process(&process_list);

    // Create our shared memory and initialise it
    create_shared_memory(process_list, SHARED_MEM_SIZE);
    strncpy(process_list->shared_memory->shared_buffer, "Hello World!", SHARED_MEM_SIZE);

    // Create our pipes and channels for interprocess communication
    microkit_channel channel_id = 1;
    create_channel(channel_id);

    // Start running the specified process
    run_process(process_list);

    // Testing a microkit_notify
    microkit_notify(channel_id);

    // Wait for the child process to finish before leaving
    if (waitpid(process_list->pid, NULL, 0) == -1) {
        fprintf(stderr, "Error on waitpid\n");
        return -1;
    }

    // Unmap and free all used heap memory
    free_channels();
    free_processes(process_list);

    return 0;
}