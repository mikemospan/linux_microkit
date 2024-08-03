#define _GNU_SOURCE

#include <handler.h>

#include <sys/mman.h>

struct process *process_stack = NULL;
khash_t(shared_memory) *shared_memory_map = NULL;

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
    new->shared_memory = NULL;
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

    if (shared_memory_map == NULL) {
        shared_memory_map = kh_init(shared_memory);
    }
    int ret;
    khiter_t iter = kh_put(shared_memory, shared_memory_map, name, &ret);
    if (ret == -1) {
        printf("Error on adding to hash map\n");
        exit(EXIT_FAILURE);
    }
    kh_value(shared_memory_map, iter) = new;
}

void add_shared_memory(struct process *process, char *name) {
    khiter_t iter = kh_get(shared_memory, shared_memory_map, name);
    if (strcmp(kh_key(shared_memory_map, iter), name) != 0) {
        printf("The mapping %s is invalid.\n", name);
        exit(EXIT_FAILURE);
    }
    struct shared_memory *shared_memory = kh_value(shared_memory_map, iter);
    struct shared_memory_stack *head = process->shared_memory;
    process->shared_memory = malloc(sizeof(struct shared_memory_stack));
    process->shared_memory->shm = shared_memory;
    process->shared_memory->next = head;
}

void free_resources() {
    kh_destroy(shared_memory, shared_memory_map);
    while (process_stack != NULL) {
        struct process *next_process = process_stack->next;
        free(process_stack->stack_top - STACK_SIZE);
        kh_free(channel, process_stack->channel_map);
        while (process_stack->shared_memory != NULL) {
            struct shared_memory_stack *next = process_stack->shared_memory->next;
            free(process_stack->shared_memory);
            process_stack->shared_memory = next;
        }
        free(process_stack);
        process_stack = next_process;
    }
}