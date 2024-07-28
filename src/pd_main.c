#define _GNU_SOURCE

#include <stdio.h>
#include <poll.h>
#include <dlfcn.h>

#include "linux_microkit.h"
#include "pd_main.h"

#define FDS_SIZE 496

/* HELPER FUNCTIONS */

struct process *search_process(int pid) {
    struct process *current = process_list;
    while (current != NULL && current->pid != pid) {
        current = current->next;
    }
    return current;
}

static void execute_init(const char *path) {
    void *handle = dlopen(path, RTLD_LAZY);
    if (handle == NULL) {
        printf("Error opening file: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    void (*init)(void) = (void (*)(void)) dlsym(handle, "init");
    const char *dlsym_error = dlerror();
    if (dlsym_error != NULL) {
        printf("Error finding function \"init\": %s\n", dlsym_error);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    init();
    dlclose(handle);
}

static void execute_notified(const char *path, microkit_channel buf) {
    void *handle = dlopen(path, RTLD_LAZY);
    if (handle == NULL) {
        printf("Error opening file: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    void (*notified)(microkit_channel) = (void (*)(microkit_channel)) dlsym(handle, "notified");
    const char *dlsym_error = dlerror();
    if (dlsym_error != NULL) {
        printf("Error finding function \"notified\": %s\n", dlsym_error);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    notified(buf);
    dlclose(handle);
}

/* Main child function acting as an event handler */
int child_main(void *arg) {
    execute_init((const char *) arg);

    struct process *process = search_process(getpid());
    printf("Child process received the message \"%s\" from shared buffer.\n", (char *) (*process->shared_memory)->shared_buffer);
    
    // Declare and initialise necessary variables for polling from pipe
    struct pollfd *fds = malloc(FDS_SIZE);
    if (fds == NULL) {
        printf("Error on allocating poll fds\n");
        return -1;
    }

    fds->fd = process->pipefd[PIPE_READ_FD];
    fds->events = POLLIN;

    // Main event loop for polling for changes in pipes and calling notified accordingly
    while (ppoll(fds, MICROKIT_MAX_CHANNELS, NULL, NULL)) {
        for (int i = 0; i < MICROKIT_MAX_CHANNELS; i++) {
            if (fds[i].revents & POLLIN) {
                microkit_channel buf;
                read(fds[i].fd, &buf, sizeof(microkit_channel));
                execute_notified((const char *) arg, buf);
            }
        }
    }

    return 0;
}