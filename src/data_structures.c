#define _GNU_SOURCE

#include <handler.h>

#include <sys/mman.h>

struct process *process_stack = NULL;
struct shared_memory *shared_memory_stack = NULL;

struct process *create_process() {
    struct process *new = malloc(sizeof(struct process));
    if (new == NULL) {
        printf("Error on allocating process\n");
        exit(EXIT_FAILURE);
    }

    new->pid = -1;
    char *stack = malloc(STACK_SIZE);
    if (stack == NULL) {
        printf("Error on allocating stack\n");
        exit(EXIT_FAILURE);
    }
    new->stack_top = stack + STACK_SIZE;
    new->channel_map = kh_init(channel);
    if (pipe(new->pipefd) == -1) {
        printf("Error on creating pipe\n");
        exit(EXIT_FAILURE);
    }

    struct process *next = process_stack;
    process_stack = new;
    process_stack->next = next;

    return new;
}

void create_channel(struct process *from, struct process *to, microkit_channel ch) {
    int ret;
    khiter_t iter = kh_put(channel, from->channel_map, ch, &ret);
    if (ret == -1) {
        printf("Error on adding to hash map\n");
        exit(EXIT_FAILURE);
    }
    kh_value(from->channel_map, iter) = to;
}

void create_shared_memory(char *name, int size) {
    struct shared_memory *new = malloc(sizeof(struct shared_memory));
    if (new == NULL) {
        printf("Error on allocating shared memory\n");
        exit(EXIT_FAILURE);
    }

    new->shared_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (new->shared_buffer == MAP_FAILED) {
        printf("Error on allocating shared buffer\n");
        exit(EXIT_FAILURE);
    }
    new->size = size;
    new->name = name;
    new->next = NULL;

    struct shared_memory *next = shared_memory_stack;
    shared_memory_stack = new;
    shared_memory_stack->next = next;
}

void free_processes() {
    while (process_stack != NULL) {
        struct process *next = process_stack->next;
        free(process_stack->stack_top - STACK_SIZE);
        kh_free(channel, process_stack->channel_map);
        free(process_stack);
        process_stack = next;
    }
}

void free_shared_memory() {
    while (shared_memory_stack != NULL) {
        struct shared_memory *next = shared_memory_stack->next;
        munmap(shared_memory_stack->shared_buffer, shared_memory_stack->size);
        free(shared_memory_stack);
        shared_memory_stack = next;
    }
}