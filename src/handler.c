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
process_t *proc;

/**
 * Sets the address of the variables declared as shared within the process to the addresses
 * we stored internally in `loader.c`.
 * @param handle A handle to the dynamically linked process to be opened.
 * @param process The information of the process we will be setting the shared memory of.
 */
static void set_shared_memory(void *handle, process_t *process) {
    shared_memory_stack_t *curr = process->shared_memory;
    while (curr != NULL) {
        seL4_Word *buff = (seL4_Word *) dlsym(handle, curr->varname);
        const char *dlsym_error = dlerror();
        if (dlsym_error != NULL) {
            printf("Error finding shared variable: %s\n", dlsym_error);
            dlclose(handle);
            exit(EXIT_FAILURE);
        }
        *buff = (seL4_Word) curr->shm->shared_buffer;
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
 * @param ch An unsigned integer to the channel id we will be notifying
 */
static void execute_notified(void *handle, microkit_channel ch) {
    void (*notified)(microkit_channel) = dlsym(handle, "notified");
    const char *dlsym_error = dlerror();
    if (dlsym_error != NULL) {
        printf("Error finding function \"notified\": %s\n", dlsym_error);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    notified(ch);
}

/**
 * Finds the symbolic link for the `protected` function in the process's elf
 * and executes it.
 * @param buf A struct with the information needed to send and reply to a message
 */
static void execute_protected(void *handle, message_t msg) {
    microkit_msginfo (*protected)(microkit_channel, microkit_msginfo) = dlsym(handle, "protected");
    const char *dlsym_error = dlerror();
    if (dlsym_error != NULL) {
        printf("Error finding function \"protected\": %s\n", dlsym_error);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    microkit_msginfo info = protected(msg.ch, msg.msginfo);
    write(msg.send_back, &info, sizeof(microkit_msginfo));
}

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
    proc = (process_t *) arg;

    // Dynamically loads the process at runtime
    void *handle = dlopen(proc->path, RTLD_LAZY);
    if (handle == NULL) {
        printf("Error opening file: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    set_shared_memory(handle, proc);
    execute_init(handle);
    
    // Declare and initialise necessary variables for polling for notifications and ppc
    struct pollfd fds[2] = {
        { .fd = proc->notification, .events = POLLIN },
        { .fd = proc->send_pipe[PIPE_READ_FD], .events = POLLIN }
    };

    // Main event loop for polling for changes in pipe and calling notified/protected accordingly
    while (ppoll(fds, 2, NULL, NULL)) {
        if (fds[0].revents & POLLIN) {
            microkit_channel channel;
            read(fds[0].fd, &channel, sizeof(microkit_channel));
            execute_notified(handle, channel);
        } else if (fds[1].revents & POLLIN) {
            message_t msg;
            read(fds[1].fd, &msg, sizeof(message_t));
            execute_protected(handle, msg);
        }
    }

    dlclose(handle);
    return 0;
}