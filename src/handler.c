/**
 * This is the main handler file which is called directly by loader.c after executing `run_process`.
 * Every protection domain calls the `event_handler` function to start its exectution.
 * 
 * Author: Michael Mospan (@mmospan)
 */

#define _GNU_SOURCE

#include <handler.h>

#include <poll.h>
#include <dlfcn.h>

// The current process. Useful to keep track of to avoid a worst-case O(p) search in microkit.c.
struct process *proc;

/**
 * Sets the address of the variables declared as shared within the process to the addresses
 * we stored internally in `loader.c`.
 * @param handle A handle to the dynamically linked process to be opened.
 * @param process The information of the process we will be setting the shared memory of.
 */
static void set_shared_memory(void *handle, struct process *process) {
    struct shared_memory_stack *curr = process->shared_memory;
    while (curr != NULL) {
        unsigned long *buff = (unsigned long *) dlsym(handle, curr->shm->name);
        if (dlerror() == NULL) {
            *buff = (unsigned long) curr->shm->shared_buffer;
        }
        curr = curr->next;
    }
}

/**
 * Finds the symbolic link for the `init` function in the process's elf
 * and executes it.
 * @param handle A handle to the dynamically linked process to be opened.
 */
static void execute_init(void *handle) {
    void (*init)(void) = (void (*)(void)) dlsym(handle, "init");
    const char *dlsym_error = dlerror();
    if (dlsym_error != NULL) {
        printf("Error finding function \"init\": %s\n", dlsym_error);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    init();
}

/**
 * Finds the symbolic link for the `notified` function in the process's elf
 * and executes it.
 * @param handle A handle to the dynamically linked process to be opened.
 * @param buf An unsigned integer to the channel id we will be notifying
 */
static void execute_notified(void *handle, microkit_channel buf) {
    void (*notified)(microkit_channel) = dlsym(handle, "notified");
    const char *dlsym_error = dlerror();
    if (dlsym_error != NULL) {
        printf("Error finding function \"notified\": %s\n", dlsym_error);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    notified(buf);
}

/**
 * Finds the symbolic link for the `protected` function in the process's elf
 * and executes it.
 * @param buf A struct with the information needed to send and reply to a message
 */
static void execute_protected(void *handle, struct message buf) {
    microkit_msginfo (*protected)(microkit_channel, microkit_msginfo) = dlsym(handle, "protected");
    const char *dlsym_error = dlerror();
    if (dlsym_error != NULL) {
        printf("Error finding function \"protected\": %s\n", dlsym_error);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    protected(buf.ch - MICROKIT_MAX_PDS, buf.msginfo);
    long info = 1000;
    write(buf.send_back, &info, sizeof(long));
}

/* Main child function acting as an event handler */
/**
 * The main function that will be executed by the handler. Its main job is to:
 * 
 * 1. Initialise all the memory marked as shared
 * 
 * 2. Execute the init function
 * 
 * 3. Poll for any notifications/ppc and execute the notified/protected function accordingly
 * @param arg A void pointer containing the address of a process
 */
int event_handler(void *arg) {
    proc = (struct process *) arg;
    void *handle = dlopen(proc->path, RTLD_LAZY);
    if (handle == NULL) {
        printf("Error opening file: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    set_shared_memory(handle, proc);
    execute_init(handle);
    
    // Declare and initialise necessary variables for polling from pipe
    struct pollfd *fds = malloc(sizeof(struct pollfd));
    if (fds == NULL) {
        printf("Error on allocating poll fds\n");
        return -1;
    }

    fds->fd = proc->pipefd[PIPE_READ_FD];
    fds->events = POLLIN;

    // Main event loop for polling for changes in pipe and calling notified/protected accordingly
    while (ppoll(fds, 1, NULL, NULL)) {
        if (fds->revents & POLLIN) {
            struct message buf;
            read(fds->fd, &buf, sizeof(struct message));
            if (buf.ch > MICROKIT_MAX_PDS) execute_protected(handle, buf);
            else execute_notified(handle, buf.ch);
        }
    }

    free(fds);
    dlclose(handle);
    return 0;
}