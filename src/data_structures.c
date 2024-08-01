#define _GNU_SOURCE

#include <handler.h>

#include <sys/mman.h>

struct process_list *process_list = NULL;
struct shared_memory_list *shared_memory_list = NULL;

struct process *create_process() {
    struct process *new = malloc(sizeof(struct process));
    if (new == NULL) {
        printf("Error on allocating process\n");
        exit(EXIT_FAILURE);
    }

    if (process_list == NULL) {
        process_list = malloc(sizeof(struct process_list));
        process_list->head = NULL;
        process_list->tail = NULL;
    }

    if (process_list->head == NULL) {
        process_list->head = new;
    } else {
        process_list->tail->next = new;
    }
    process_list->tail = new;

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

struct shared_memory *create_shared_memory(char *name, int size) {
    struct shared_memory *new = malloc(sizeof(struct shared_memory));
    if (new == NULL) {
        printf("Error on allocating shared memory\n");
        exit(EXIT_FAILURE);
    }

    if (shared_memory_list == NULL) {
        shared_memory_list = malloc(sizeof(struct shared_memory_list));
        shared_memory_list->head = NULL;
        shared_memory_list->tail = NULL;
    }

    if (shared_memory_list->head == NULL) {
        shared_memory_list->head = new;
    } else {
        shared_memory_list->tail->next = new;
    }

    shared_memory_list->tail = new;
    new->shared_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (new->shared_buffer == MAP_FAILED) {
        printf("Error on allocating shared buffer\n");
        exit(EXIT_FAILURE);
    }
    new->size = size;
    new->name = name;
    new->next = NULL;

    return new;
}

void free_processes() {
    while (process_list->head != NULL) {
        struct process *next = process_list->head->next;
        free(process_list->head->stack_top - STACK_SIZE);
        kh_free(channel, process_list->head->channel_map);
        free(process_list->head);
        process_list->head = next;
    }
    free(process_list);
}

void free_shared_memory() {
    while (shared_memory_list->head != NULL) {
        struct shared_memory *next = shared_memory_list->head->next;
        struct shared_memory *curr = shared_memory_list->head;
        munmap(curr->shared_buffer, curr->size);
        free(curr);
        shared_memory_list->head = next;
    }
    free(shared_memory_list);
}