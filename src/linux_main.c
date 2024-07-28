#define _GNU_SOURCE

#include <stdio.h>
#include <sched.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdbool.h>

#include "linux_microkit.h"
#include "pd_main.h"

#define PAGE_SIZE 4096
#define STACK_SIZE PAGE_SIZE
#define SHARED_MEM_SIZE PAGE_SIZE

struct process *process_list = NULL;
struct channel *channel_list = NULL;
struct shared_memory *shared_memory_list = NULL;

static struct process *create_process() {
    struct process *current = process_list;
    if (current == NULL) {
        process_list = mmap(NULL, sizeof(struct process), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
        if (process_list == MAP_FAILED) {
            printf("Error on allocating shared buffer\n");
            exit(EXIT_FAILURE);
        }
        process_list->next = NULL;
        current = process_list;
    } else {
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = mmap(NULL, sizeof(struct process), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
        if (current->next == MAP_FAILED) {
            printf("Error on allocating shared buffer\n");
            exit(EXIT_FAILURE);
        }
        current = current->next;
    }

    char *stack = malloc(STACK_SIZE);
    if (stack == NULL) {
        printf("Error on allocating stack\n");
        exit(EXIT_FAILURE);
    }
    current->stack_top = stack + STACK_SIZE;
    current->pid = -1;
    current->channel = mmap(NULL, sizeof(struct channel *) * MICROKIT_MAX_CHANNELS,
        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (current->channel == MAP_FAILED) {
        printf("Error on allocating channel\n");
        exit(EXIT_FAILURE);
    }
    if (pipe(current->pipefd) == -1) {
        printf("Error on creating pipe\n");
        exit(EXIT_FAILURE);
    }

    return current;
}

static void run_process(struct process *process, const char *path) {
    process->pid = clone(child_main, process->stack_top, SIGCHLD, (void *) path);
    if (process->pid == -1) {
        printf("Error on cloning\n");
        exit(EXIT_FAILURE);
    }
}

static struct channel *create_channel(struct process *to, microkit_channel ch) {
    struct channel *current = channel_list;
    if (current == NULL) {
        channel_list = mmap(NULL, sizeof(struct channel), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
        if (channel_list == MAP_FAILED) {
            printf("Error on allocating shared buffer\n");
            exit(EXIT_FAILURE);
        }
        current = channel_list;
    } else {
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = mmap(NULL, sizeof(struct channel), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
        if (current->next == MAP_FAILED) {
            printf("Error on allocating channel\n");
            exit(EXIT_FAILURE);
        }
        current = current->next;
    }

    current->channel_id = ch;
    current->to = to;
    current->next = NULL;

    return current;
}

static void add_channel(struct process *from, struct process *to, microkit_channel ch) {
    struct channel *curr_ch = channel_list;
    while (curr_ch != NULL && (curr_ch->channel_id != ch || curr_ch->to != to)) {
        curr_ch = curr_ch->next;
    }
    if (curr_ch == NULL) {
        curr_ch = create_channel(to, ch);
    }

    struct channel **temp = from->channel;
    while (*temp != NULL) {
        temp += sizeof(struct channel *);
    }
    *temp = curr_ch;
}

static struct shared_memory *create_shared_memory(int size) {
    struct shared_memory *current = shared_memory_list;
    if (current == NULL) {
        shared_memory_list = malloc(sizeof(struct shared_memory));
        if (shared_memory_list == NULL) {
            printf("Error on allocating shared buffer\n");
            exit(EXIT_FAILURE);
        }
        current = shared_memory_list;
    } else {
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = malloc(sizeof(struct shared_memory));
        if (current->next == NULL) {
            printf("Error on allocating shared buffer\n");
            exit(EXIT_FAILURE);
        }
        current = current->next;
    }

    current->shared_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (current->shared_buffer == MAP_FAILED) {
        printf("Error on allocating shared buffer\n");
        exit(EXIT_FAILURE);
    }
    current->size = size;
    current->next = NULL;

    return current;
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
    struct process *p1 = create_process();
    struct process *p2 = create_process();

    // Create our shared memory and initialise it
    create_shared_memory(SHARED_MEM_SIZE);

    // Create our pipes and channels for interprocess communication
    microkit_channel channel1_id = 1;
    microkit_channel channel2_id = 2;
    add_channel(p1, p2, channel1_id);
    add_channel(p2, p1, channel2_id);

    // Start running the specified processes
    run_process(p1, "./user/server.so");
    run_process(p2, "./user/client.so");

    // Wait for the child processes to finish before leaving
    struct process *curr = process_list;
    while (curr != NULL) {
        if (waitpid(curr->pid, NULL, 0) == -1) {
            printf("Error on waitpid\n");
            return -1;
        }
        curr = curr->next;
    }

    // Unmap and free all used heap memory
    free_channels();
    free_shared_memory();
    free_processes();

    return 0;
}