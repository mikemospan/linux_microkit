/**
 * This is the main loader file which is called directly by main.py as a DLL.
 * Its main purpose is to load any dependencies and store all the data related
 * to the running protection domains before calling `run_process`.
 * 
 * Author: Michael Mospan (@mmospan)
 */

#define _GNU_SOURCE

#include <handler.h>

#include <sys/mman.h>
#include <sys/wait.h>
#include <sched.h>

/* --- Hashmaps used by the program defined in handler.h --- */
khash_t(process) *process_name_to_info = NULL; // Maps (char *) -> (struct process *)
khash_t(shared_memory) *shared_memory_map = NULL;

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
    new->shared_memory = NULL;
    if (pipe(new->pipefd) == -1 || pipe(new->ppc_reply) == -1) {
        printf("Error on creating pipe\n");
        exit(EXIT_FAILURE);
    }
    new->pid = -1;

    if (process_name_to_info == NULL) {
        process_name_to_info = kh_init(process);
    }
    int ret;
    khiter_t iter = kh_put(process, process_name_to_info, strdup(name), &ret);
    if (ret == -1) {
        printf("Error on adding to hash map\n");
        exit(EXIT_FAILURE);
    }
    kh_value(process_name_to_info, iter) = new;
}

void create_channel(const char *from, const char *to, microkit_channel ch) {
    khiter_t piter = kh_get(process, process_name_to_info, from);
    struct process *from_process = kh_value(process_name_to_info, piter);

    piter = kh_get(process, process_name_to_info, to);

    int ret;
    khiter_t iter = kh_put(channel, from_process->channel_map, ch, &ret);
    if (ret == -1) {
        printf("Error on adding to hash map\n");
        exit(EXIT_FAILURE);
    }
    kh_value(from_process->channel_map, iter) = kh_value(process_name_to_info, piter);

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
    new->name = strdup(name);

    if (shared_memory_map == NULL) {
        shared_memory_map = kh_init(shared_memory);
    }
    int ret;
    khiter_t iter = kh_put(shared_memory, shared_memory_map, new->name, &ret);
    if (ret == -1) {
        printf("Error on adding to hash map\n");
        exit(EXIT_FAILURE);
    }
    kh_value(shared_memory_map, iter) = new;
}

void add_shared_memory(const char *proc, const char *name) {
    khiter_t piter = kh_get(process, process_name_to_info, proc);
    if (piter == kh_end(process_name_to_info)) {
        printf("The mapping %s is invalid.\n", proc);
        exit(EXIT_FAILURE);
    }
    struct process *process = kh_value(process_name_to_info, piter);

    khiter_t iter = kh_get(shared_memory, shared_memory_map, name);
    if (iter == kh_end(shared_memory_map)) {
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
    for (khint_t k = kh_begin(h); k != kh_end(process_name_to_info); ++k) {
        if (kh_exist(process_name_to_info, k)) {
            struct process *process = kh_value(process_name_to_info, k);
            kill(process->pid, SIGTERM);
            free(process->stack_top - STACK_SIZE);
            struct shared_memory_stack *shared_memory = process->shared_memory;
            while (shared_memory != NULL) {
                struct shared_memory_stack *next = process->shared_memory->next;
                free(shared_memory);
                shared_memory = next;
            }
            kh_free(channel, process->channel_map);
        }
    }
    kh_destroy(shared_memory, shared_memory_map);
    kh_destroy(process, process_name_to_info);
}

void run_process(char *proc, char *path) {
    khiter_t piter = kh_get(process, process_name_to_info, proc);
    struct process *process = kh_value(process_name_to_info, piter);
    process->path = path;

    process->pid = clone(event_handler, process->stack_top, SIGCHLD, (void *) process);
    if (process->pid == -1) {
        printf("Error on cloning\n");
        exit(EXIT_FAILURE);
    }
}

void block_until_finish() {
    for (khint_t k = kh_begin(h); k != kh_end(process_name_to_info); ++k) {
        struct process *process = kh_value(process_name_to_info, k);
        if (waitpid(process->pid, NULL, 0) == -1) {
            return;
        }
    }
}