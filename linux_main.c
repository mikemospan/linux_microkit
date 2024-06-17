#define _GNU_SOURCE

#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdbool.h>

#include "include/linux_microkit.h"
#include "pd_main.h"

#define PAGE_SIZE 4096
#define STACK_SIZE PAGE_SIZE
#define SHARED_MEM_SIZE PAGE_SIZE

struct process *process_list = NULL;
struct channel *channel_list = NULL;
struct shared_memory *shared_memory_list = NULL;

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

static struct channel *create_channel(microkit_channel ch) {
    struct channel *current = channel_list;
    if (current == NULL) {
        channel_list = mmap(NULL, sizeof(struct channel), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
        if (channel_list == MAP_FAILED) {
            fprintf(stderr, "Error on allocating shared buffer\n");
            exit(EXIT_FAILURE);
        }
        current = channel_list;
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

    return current;
}

static void add_channel(struct process *process, microkit_channel ch) {
    struct channel *curr_ch = channel_list;
    while (curr_ch != NULL && curr_ch->channel_id != ch) {
        curr_ch = curr_ch->next;
    }
    if (curr_ch == NULL) {
        curr_ch = create_channel(ch);
    }

    struct process *curr_pr = process;
    while (curr_pr->channel != NULL) {
        curr_pr->channel = curr_pr->channel->next;
    }
    curr_pr->channel = curr_ch;
}

static struct shared_memory *create_shared_memory(char *name, int size) {
    struct shared_memory *current = shared_memory_list;
    if (current == NULL) {
        shared_memory_list = malloc(sizeof(struct shared_memory));
        if (shared_memory_list == NULL) {
            fprintf(stderr, "Error on allocating shared buffer\n");
            exit(EXIT_FAILURE);
        }
        current = shared_memory_list;
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
    current->name = name;
    current->next = NULL;

    return current;
}

static void add_shared_memory(struct process *process, char *name, int size) {
    struct shared_memory *curr_sh = shared_memory_list;
    while (curr_sh != NULL && strcmp(curr_sh->name, name) != 0) {
        curr_sh = curr_sh->next;
    }
    if (curr_sh == NULL) {
        curr_sh = create_shared_memory(name, size);
    }

    struct process *curr_pr = process;
    while (curr_pr->shared_memory != NULL) {
        curr_pr->shared_memory = curr_pr->shared_memory->next;
    }
    curr_pr->shared_memory = curr_sh;
}

static void free_processes() {
    while (process_list != NULL) {
        struct process *next = process_list->next;
        free(process_list->stack_top - STACK_SIZE);
        munmap(process_list, sizeof(struct process));
        process_list = next;
    }
}

static void free_channels() {
    while (channel_list != NULL) {
        struct channel *next = channel_list->next;
        munmap(channel_list, sizeof(struct channel));
        channel_list = next;
    }
}

static void free_shared_memory() {
    while (shared_memory_list != NULL) {
        struct shared_memory *next = shared_memory_list->next;
        munmap(shared_memory_list->shared_buffer, shared_memory_list->size);
        munmap(shared_memory_list, sizeof(struct shared_memory));
        shared_memory_list = next;
    }
}

/* MAIN FUNCTION ACTING AS A SETUP FOR ALL NECESSARY OBJECTS */

int main(void) {
    // Create our processes to act in place of protection domains
    create_process(&process_list);

    // Create our shared memory and initialise it
    char var_name[16] = "my_buffer";
    add_shared_memory(process_list, var_name, SHARED_MEM_SIZE);
    strncpy(process_list->shared_memory->shared_buffer, "Hello World!", SHARED_MEM_SIZE);

    // Create our pipes and channels for interprocess communication
    microkit_channel channel_id = 1;
    add_channel(process_list, channel_id);

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
    free_shared_memory();
    free_processes();

    return 0;
}