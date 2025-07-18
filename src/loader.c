/**
 * This is the simplified loader file which is called directly by main.rs as a DLL.
 * Its main purpose is to load any dependencies and store all the data related
 * to the running protection domains before calling `run_process`.
 * 
 * Author: Michael Mospan (@mmospan)
 */

#define _GNU_SOURCE

#include <handler.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/eventfd.h>
#include <sched.h>

/**
 * Creates the process data and returns a handle to it.
 * This involves allocating memory for the process struct which holds its:
 * 1. Stack,
 * 2. Signal stack,
 * 3. Shared memory,
 * 4. Channels (UNIX eventfd and pipes internally),
 * 5. Path.
 * 
 * @param name The name of the protection domain (process). This is a Rust owned string.
 * @param stack_size The size of the stack to be allocated for the process
 */
process_t *create_process(const char *name, uint32_t stack_size) {
    process_t *new = malloc(sizeof(process_t));
    if (new == NULL) {
        fprintf(stderr, "Error on allocating process\n");
        exit(EXIT_FAILURE);
    }
    
    // Allocate main stack with guard page
    void *stack = mmap(NULL, stack_size + PAGE_SIZE, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANON | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
        fprintf(stderr, "Error on creating the stack in %s\n", name);
        exit(EXIT_FAILURE);
    }
    
    // Make guard page at the bottom of the stack unreadable, unwritable and unexecutable
    if (mprotect(stack, PAGE_SIZE, PROT_NONE) != 0) {
        fprintf(stderr, "Error on allocating guard page in %s\n", name);
        exit(EXIT_FAILURE);
    }
    new->stack_top = (char *) stack + stack_size + PAGE_SIZE;
    
    // Allocate alternative stack for signal handling done by the child process
    new->sig_handler_stack = mmap(NULL, SIGSTKSZ, PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANON | MAP_STACK, -1, 0);
    if (new->sig_handler_stack == MAP_FAILED) {
        fprintf(stderr, "Error creating alternative stack\n");
        exit(EXIT_FAILURE);
    }
    
    new->shared_memory = NULL;
    new->ipc_buffer = mmap(NULL, IPC_BUFFER_SIZE * sizeof(seL4_Word),
                           PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (new->ipc_buffer == MAP_FAILED) {
        fprintf(stderr, "Error on creating ipc buffer in %s\n", name);
        exit(EXIT_FAILURE);
    }
    
    new->notification = eventfd(0, EFD_NONBLOCK);
    if (new->notification == -1) {
        fprintf(stderr, "Error on creating eventfd in %s\n", name);
        exit(EXIT_FAILURE);
    }
    
    /**
     * Create the process's channels stored internally as UNIX pipes. Pipes are a unidirectional
     * meaning we will need two of them: one for sending a message, and one for receiving a reply.
     * 
     * Each pipe is structured as an array of size 2 for the write and read end of the pipe.
     * Take the example: `proc1 -> X=====Y <- proc2` where X and Y are the ends of the pipe. For proc2
     * to receive a message from proc1, proc1 will need to write to X and proc2 will need to read from Y.
     */
    new->channel_id_to_process = kh_init(channel);
    if (pipe(new->send_pipe) == -1 || pipe(new->receive_pipe) == -1) {
        fprintf(stderr, "Error on creating pipe in %s\n", name);
        exit(EXIT_FAILURE);
    }
    
    return new;
}

/**
 * Creates a segment of shared memory and returns a handle to it.
 * 
 * @param name A string corresponding to the name of the shared memory. This is a Rust owned string.
 * @param size An unsigned 64 bit integer corresponding to the size of the shared memory.
 */
shared_memory_t *create_shared_memory(const char *name, uint64_t size) {
    shared_memory_t *new = malloc(sizeof(shared_memory_t));
    if (new == NULL) {
        fprintf(stderr, "Error on allocating shared memory\n");
        exit(EXIT_FAILURE);
    }
    
    new->size = size;
    
    /**
     * Create the shared buffer within which the actual data shared between protection domains
     * will be stored. Thus, we will mmap a set of anonymous memory which can be shared between processes.
     */
    new->shared_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (new->shared_buffer == MAP_FAILED) {
        fprintf(stderr, "Error on allocating shared buffer for %s\n", name);
        exit(EXIT_FAILURE);
    }
    
    return new;
}

/**
 * Adds the specified shared memory block to a list of shared memory accessible by the process.
 * 
 * @param process_handle Handle to the process (returned by create_process)
 * @param shm_handle Handle to the shared memory (returned by create_shared_memory)
 * @param shm_varname A string corresponding to the name of the variable in the process. This is a Rust owned string.
 */
void add_shared_memory(process_t *process, shared_memory_t *shared_memory, const char *shm_varname) {
    // Add the shared memory struct to the stack stored in the process. Stacks give us constant push time.
    shared_memory_stack_t *head = process->shared_memory;
    process->shared_memory = malloc(sizeof(shared_memory_stack_t));
    if (process->shared_memory == NULL) {
        fprintf(stderr, "Error allocating shared memory stack node\n");
        exit(EXIT_FAILURE);
    }
    
    process->shared_memory->shm = shared_memory;
    process->shared_memory->_varname = shm_varname;
    process->shared_memory->next = head;
}

/**
 * Establishes a unidirectional channel of the form: 'from' =====> 'to'.
 * 
 * @param from_process Handle to the 'from' process
 * @param to_process Handle to the 'to' process
 * @param ch An unsigned integer corresponding to the id of this channel.
 */
void create_channel(process_t *from_process, process_t *to_process, microkit_channel ch) {
    int ret;
    khiter_t channel_iter = kh_put(channel, from_process->channel_id_to_process, ch, &ret);
    if (ret == -1) {
        fprintf(stderr, "Error on adding to channel hash map\n");
        exit(EXIT_FAILURE);
    }
    
    kh_value(from_process->channel_id_to_process, channel_iter) = to_process;
}

/**
 * Runs the provided process by spawning a child from the main microkit process using clone.
 * From there, the child calls the `event_handler` function specified in handler.c.
 * 
 * @param process_handle Handle to the process to run
 * @param path A string corresponding to the path of the process. This is a Rust owned string.
 */
void run_process(process_t *process, char *path) {
    process->_path = path;
    pid_t pid = clone(event_handler, process->stack_top, SIGCHLD, (void *) process);
    if (pid == -1) {
        fprintf(stderr, "Error on cloning process %s\n", path);
        exit(EXIT_FAILURE);
    }
}

/**
 * Used purely for testing purposes by `tests/loader_test.rs`.
 * 
 * @param from The process to get the channel mapping from
 * @param ch The channel along which we return the receiver process.
 */
process_t *get_channel_target(process_t *from, microkit_channel ch) {
    khiter_t it = kh_get(channel, from->channel_id_to_process, ch);
    if (it == kh_end(from->channel_id_to_process)) return NULL;
    return kh_val(from->channel_id_to_process, it);
}