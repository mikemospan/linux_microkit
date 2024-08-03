#define _GNU_SOURCE

#include <handler.h>

#include <sys/mman.h>

khash_t(process) *process_map = NULL;
struct shared_memory *shared_memory_stack = NULL;

void create_process(const char *name) {
    struct process *new = malloc(sizeof(struct process));
    if (new == NULL) {
        printf("Error on allocating process\n");
        exit(EXIT_FAILURE);
    }

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
    new->pid = -1;

    if (process_map == NULL) {
        process_map = kh_init(process);
    }
    int ret;
    khiter_t iter = kh_put(process, process_map, name, &ret);
    if (ret == -1) {
        printf("Error on adding to hash map\n");
        exit(EXIT_FAILURE);
    }
    kh_value(process_map, iter) = new;
}

void create_channel(const char *from, const char *to, microkit_channel ch) {
    khiter_t piter = kh_get(process, process_map, from);
    struct process *from_process = kh_value(process_map, piter);

    piter = kh_get(process, process_map, to);
    struct process *to_process = kh_value(process_map, piter);

    int ret;
    khiter_t iter = kh_put(channel, from_process->channel_map, ch, &ret);
    if (ret == -1) {
        printf("Error on adding to hash map\n");
        exit(EXIT_FAILURE);
    }
    kh_value(from_process->channel_map, iter) = kh_value(process_map, piter);
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
    for (khint_t k = kh_begin(h); k != kh_end(process_map); ++k) {
        struct process *process = kh_value(process_map, k);
        free(process->stack_top - STACK_SIZE);
        kh_free(channel, process->channel_map);
    }
    kh_destroy(process, process_map);
}

void free_shared_memory() {
    while (shared_memory_stack != NULL) {
        struct shared_memory *next = shared_memory_stack->next;
        munmap(shared_memory_stack->shared_buffer, shared_memory_stack->size);
        free(shared_memory_stack);
        shared_memory_stack = next;
    }
}