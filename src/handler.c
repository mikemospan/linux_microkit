/**
 * This is the main handler file which is called directly by loader.c after executing `run_process`.
 * Every protection domain calls the `event_handler` function to start its exectution.
 * 
 * Author: Michael Mospan (@mmospan)
 */

#define _GNU_SOURCE

#include <handler.h>
#include <dlfcn.h>
#include <sys/epoll.h>
#include <signal.h>
#include <execinfo.h>

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
        seL4_Word *buff = (seL4_Word *) dlsym(handle, curr->_varname);
        const char *dlsym_error = dlerror();
        if (dlsym_error != NULL) {
            fprintf(stderr, "Error finding shared variable: %s\n", dlsym_error);
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
        fprintf(stderr, "Error finding function \"init\": %s\n", dlsym_error);
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
        fprintf(stderr, "Error finding function \"notified\": %s\n", dlsym_error);
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
        fprintf(stderr, "Error finding function \"protected\": %s\n", dlsym_error);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    microkit_msginfo info = protected(msg.ch, msg.msginfo);
    write(msg.send_back, &info, sizeof(microkit_msginfo));
}

/**
 * The signal handler of the child process. Currently only used to catch stack overflows.
 * @param sig The signal number delivered to the handler
 */
void sig_handler(int sig) {
    if (sig != SIGSEGV) {
        fprintf(stderr, "Unknown error detected!\n");
        exit(EXIT_FAILURE);
    }

    void *array[10];
    size_t size;

    // get void*'s for all entries on the stack
    size = backtrace(array, 10);

    // print out all the frames to stderr
    fprintf(stderr, "Stack overflow detected:\n");
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    exit(EXIT_FAILURE);
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

    // Set up the alternative stack
    stack_t ss = {.ss_sp = proc->sig_handler_stack, .ss_size = SIGSTKSZ, .ss_flags = 0};
    if (sigaltstack(&ss, NULL) == -1) {
        perror("sigaltstack");
        exit(1);
    }

    // Install signal handler
    struct sigaction sa = {.sa_handler = sig_handler, .sa_flags = SA_ONSTACK};
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // Dynamically loads the process at runtime
    void *handle = dlopen(proc->_path, RTLD_LAZY);
    if (handle == NULL) {
        fprintf(stderr, "Error opening file: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    set_shared_memory(handle, proc);
    execute_init(handle);

    int epoll_fd = epoll_create1(0);

    struct epoll_event event = {.events = EPOLLIN, .data.fd = proc->notification};
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, proc->notification, &event) == -1) {
        fprintf(stderr, "Failed to initialise polling for notifications");
        exit(EXIT_FAILURE);
    }

    event = (struct epoll_event){.events = EPOLLIN, .data.fd = proc->send_pipe[PIPE_READ_FD]};
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, proc->send_pipe[PIPE_READ_FD], &event) == -1) {
        fprintf(stderr, "Failed to initialise polling for ppc");
        exit(EXIT_FAILURE);
    }

    struct epoll_event events[2];

    for (;;) {
        int nfds = epoll_wait(epoll_fd, events, 2, -1);
        if (nfds == -1) {
            fprintf(stderr, "epoll wait failed");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == proc->notification) {
                microkit_channel channel;
                read(proc->notification, &channel, sizeof(microkit_channel));
                execute_notified(handle, channel);
            } else if (events[i].data.fd == proc->send_pipe[PIPE_READ_FD]) {
                message_t msg;
                read(proc->send_pipe[PIPE_READ_FD], &msg, sizeof(message_t));
                execute_protected(handle, msg);
            }
        }
    }

    dlclose(handle);
    return 0;
}