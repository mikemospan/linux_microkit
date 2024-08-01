#define _GNU_SOURCE

#include <handler.h>

#include <sys/mman.h>

struct process *process_list = NULL;
struct channel *channel_list = NULL;
struct shared_memory *shared_memory_list = NULL;

struct process *create_process() {
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

struct process *search_process(int pid) {
    struct process *current = process_list;
    while (current != NULL && current->pid != pid) {
        current = current->next;
    }
    if (current == NULL) {
        printf("Could not find process with pid %d\n", pid);
        exit(EXIT_FAILURE);
    }
    return current;
}

struct channel *create_channel(struct process *to, microkit_channel ch) {
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

struct shared_memory *create_shared_memory(char *name, int size) {
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
    current->name = name;
    current->next = NULL;

    return current;
}

void free_processes() {
    while (process_list != NULL) {
        struct process *next = process_list->next;
        free(process_list->stack_top - STACK_SIZE);
        munmap(process_list, sizeof(struct process));
        process_list = next;
    }
}

void free_channels() {
    while (channel_list != NULL) {
        struct channel *next = channel_list->next;
        munmap(channel_list, sizeof(struct channel));
        channel_list = next;
    }
}

void free_shared_memory() {
    while (shared_memory_list != NULL) {
        struct shared_memory *next = shared_memory_list->next;
        munmap(shared_memory_list->shared_buffer, shared_memory_list->size);
        munmap(shared_memory_list, sizeof(struct shared_memory));
        shared_memory_list = next;
    }
}