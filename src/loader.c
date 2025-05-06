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
#include <sys/eventfd.h>
#include <sched.h>

/* --- Hashmaps used by the program defined in handler.h --- */
khash_t(process) *process_name_to_info = NULL; // Maps process name (char*) -> process info (process_t*)
khash_t(shared_memory) *shm_name_to_info = NULL; // Maps channel name (char*) -> process info (process_t*)

/**
 * Creates the process data. This involves allocating memory for the process struct which holds its:
 * 1. Stack,
 * 2. Shared memory,
 * 3. Channels (UNIX eventfd and pipes internally),
 * 4. Process ID,
 * 5. Path.
 * This information is then added to a hashmap which maps the process name to its struct.
 * 
 * Time complexity: O(1) average, O(p); worst-case where p is the total number of processes.
 * 
 * @param name The name of the protection domain (process). This is a Rust owned string.
 * @param stack_size The size of the stack to be allocated for the process
 */
void create_process(const char *name, uint32_t stack_size) {
    process_t *new = malloc(sizeof(process_t));
    if (new == NULL) {
        printf("Error on allocating process\n");
        exit(EXIT_FAILURE);
    }

    char *stack = malloc(stack_size);
    if (stack == NULL) {
        printf("Error on allocating stack\n");
        exit(EXIT_FAILURE);
    }
    new->stack_top = stack + stack_size;
    new->shared_memory = NULL;

    new->ipc_buffer = mmap(
        NULL,
        IPC_BUFFER_SIZE * sizeof(seL4_Word),
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANON,
        -1,
        0
    );

    new->notification = eventfd(0, EFD_NONBLOCK);
    if (new->notification == -1) {
        printf("Error on creating eventfd\n");
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
        printf("Error on creating pipe\n");
        exit(EXIT_FAILURE);
    }
    
    new->pid = -1;
    if (process_name_to_info == NULL) {
        process_name_to_info = kh_init(process);
    }

    int ret;
    khiter_t iter = kh_put(process, process_name_to_info, name, &ret);
    if (ret == -1) {
        printf("Error on adding to hash map\n");
        exit(EXIT_FAILURE);
    }
    kh_value(process_name_to_info, iter) = new;
}

/**
 * Establishes a unidirectional channel of the form: 'from' =====> 'to'.
 * 
 * Time complexity: O(1) average, O(p + c) worst-case;
 * where p is the number of processes and c the number of channels.
 * 
 * @param from A string corresponding to the name of the 'from' process.
 * @param to A string corresponding to the name of the 'to' process.
 * @param ch An unsigned integer corresponding to the id of this channel.
 */
void create_channel(const char *from, const char *to, microkit_channel ch) {
    khiter_t process_iter = kh_get(process, process_name_to_info, from);
    process_t *from_process = kh_value(process_name_to_info, process_iter);

    process_iter = kh_get(process, process_name_to_info, to);

    int ret;
    khiter_t channel_iter = kh_put(channel, from_process->channel_id_to_process, ch, &ret);
    if (ret == -1) {
        printf("Error on adding to hash map\n");
        exit(EXIT_FAILURE);
    }
    
    kh_value(from_process->channel_id_to_process, channel_iter) = kh_value(process_name_to_info, process_iter);
}

/**
 * Creates a segment of shared memory by allocating a shared_memory struct with relevant info.
 * This shared memory struct gets put into a hashmap which maps the name of the shared memory to its info.
 * 
 * Time complexity: O(1) average, O(m) worst-case; where m is the number of total shared memory segments.
 * 
 * @param name A string corresponding to the name of the shared memory. This is a Rust owned string.
 * @param size An unsigned 64 bit integer corresponding to the size of the shared memory.
 */
void create_shared_memory(char *name, u_int64_t size) {
    shared_memory_t *new = malloc(sizeof(shared_memory_t));
    if (new == NULL) {
        printf("Error on allocating shared memory\n");
        exit(EXIT_FAILURE);
    }
    new->size = size;
    new->_name = name;

    /**
     * Create the shared buffer within which the actual data shared between protection domains
     * will be stored. Thus, we will mmap a set of anonymous memory which can be shared between processes.
     */
    new->shared_buffer = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (new->shared_buffer == MAP_FAILED) {
        printf("Error on allocating shared buffer\n");
        exit(EXIT_FAILURE);
    }

    if (shm_name_to_info == NULL) {
        shm_name_to_info = kh_init(shared_memory);
    }

    int ret;
    khiter_t iter = kh_put(shared_memory, shm_name_to_info, new->_name, &ret);
    if (ret == -1) {
        printf("Error on adding to hash map\n");
        exit(EXIT_FAILURE);
    }
    kh_value(shm_name_to_info, iter) = new;
}

/**
 * Adds the specified shared memory block to a list of shared memory accessable by the process.
 * 
 * Time complexity: O(1) average, O(p + m) worst-case;
 * where p is the number of total processes and m is the number of shared memory segments.
 * 
 * @param proc_name A string corresponding to the name of a process
 * @param shm_name A string corresponding to the name of some shared memory
 * @param shm_varname A string corresponding to the name of the variable in the process. This is a Rust owned string.
 */
void add_shared_memory(const char *proc_name, const char *shm_name, const char *shm_varname) {
    khiter_t process_iter = kh_get(process, process_name_to_info, proc_name);
    if (process_iter == kh_end(process_name_to_info)) {
        printf("The mapping %s is invalid.\n", proc_name);
        exit(EXIT_FAILURE);
    }
    process_t *process = kh_value(process_name_to_info, process_iter);

    khiter_t shm_iter = kh_get(shared_memory, shm_name_to_info, shm_name);
    if (shm_iter == kh_end(shm_name_to_info)) {
        printf("The mapping %s is invalid.\n", shm_name);
        exit(EXIT_FAILURE);
    }
    shared_memory_t *shared_memory = kh_value(shm_name_to_info, shm_iter);

    // Add the shared memory struct to the stack stored in the process. Stacks give us constant push time.
    shared_memory_stack_t *head = process->shared_memory;
    process->shared_memory = malloc(sizeof(shared_memory_stack_t));
    process->shared_memory->shm = shared_memory;
    process->shared_memory->_varname = shm_varname;
    process->shared_memory->next = head;
}

/**
 * Runs the provided process by spawning a child from the main microkit process using clone.
 * From there, the child calls the `event_handler` function specified in handler.c.
 * 
 * Time complexity: O(1) average, O(p) worst-case; where p is the number of processes.
 * 
 * @param proc_name A string corresponding to the name of the process
 * @param path A string corresponding to the path of the process. This is a Rust owned string.
 */
void run_process(char *proc_name, char *path) {
    khiter_t piter = kh_get(process, process_name_to_info, proc_name);
    process_t *process = kh_value(process_name_to_info, piter);
    process->_path = path;

    process->pid = clone(event_handler, process->stack_top, SIGCHLD, (void *) process);
    if (process->pid == -1) {
        printf("Error on cloning\n");
        exit(EXIT_FAILURE);
    }
}

/**
 * Blocks the main microkit process until all its children protection domains are complete.
 * 
 * Time complexity: O(p); where p is the number of processes.
 */
void block_until_finish() {
    for (khint_t k = kh_begin(h); k != kh_end(process_name_to_info); ++k) {
        process_t *process = kh_value(process_name_to_info, k);
        if (waitpid(process->pid, NULL, 0) == -1) {
            return;
        }
    }
}