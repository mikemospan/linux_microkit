#define _GNU_SOURCE

#include <stdio.h>
#include <poll.h>
#include <dlfcn.h>

#include "../include/linux_microkit.h"
#include "../include/pd_main.h"

/* HELPER FUNCTIONS */

static struct process *search_process(int pid) {
    struct process *current = process_list;
    while (current != NULL && current->pid != pid) {
        current = current->next;
    }
    return current;
}

void execute_init(const char *path) {
    void *handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Error opening file: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    void (*init)(void) = (void (*)(void)) dlsym(handle, "init");
    const char *dlsym_error = dlerror();
    if (dlsym_error) {
        fprintf(stderr, "Error finding function '%s': %s\n", "init", dlsym_error);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    init();
    dlclose(handle);
}

void execute_notified(const char *path, microkit_channel buf) {
    void *handle = dlopen(path, RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "Error opening file: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    void (*notified)(microkit_channel) = (void (*)(microkit_channel)) dlsym(handle, "notified");
    const char *dlsym_error = dlerror();
    if (dlsym_error) {
        fprintf(stderr, "Error finding function '%s': %s\n", "notified", dlsym_error);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    notified(buf);
    dlclose(handle);
}

/* MAIN CHILD FUNCTION ACTING AS AN EVENT HANDLER */

int child_main(__attribute__((unused)) void *arg) {
    execute_init("./user/test.so");

    struct process *process = search_process(getpid());
    printf("Child process received the message \"%s\" from shared buffer.\n", (char *) (*process->shared_memory)->shared_buffer);
    
    // Declare and initialise necessary variables for polling from pipe
    struct pollfd *fds = calloc(MICROKIT_MAX_CHANNELS, sizeof(struct pollfd));
    if (fds == NULL) {
        fprintf(stderr, "Error on allocating poll fds\n");
        return -1;
    }

    struct channel **curr = process->channel;
    for (int i = 0; curr[i] != NULL; i++) {
        fds[i].fd = curr[i]->pipefd[PIPE_READ_FD];
        fds[i].events = POLLIN;
    }

    // Main event loop for polling for changes in pipes and calling notified accordingly
    while (ppoll(fds, MICROKIT_MAX_CHANNELS, NULL, NULL)) {
        for (int i = 0; i < MICROKIT_MAX_CHANNELS; i++) {
            if (fds[i].revents & POLLIN) {
                microkit_channel buf;
                read(fds[i].fd, &buf, sizeof(microkit_channel));
                execute_notified("./user/test.so", buf);
            }
        }
    }

    return 0;
}