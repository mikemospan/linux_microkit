#define _GNU_SOURCE

#include <pd_main.h>

#include <poll.h>
#include <dlfcn.h>

#define FDS_SIZE 496

/* HELPER FUNCTIONS */

static void update_buffers(void *handle) {
    struct shared_memory *curr = shared_memory_list;
    while (curr != NULL) {
        unsigned long *buff = (unsigned long *) dlsym(handle, curr->name);
        if (dlerror() == NULL) {
            *buff = (unsigned long) curr->shared_buffer;
        }
        curr = curr->next;
    }
}

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

static void execute_notified(void *handle, microkit_channel buf) {
    void (*notified)(microkit_channel) = (void (*)(microkit_channel)) dlsym(handle, "notified");
    const char *dlsym_error = dlerror();
    if (dlsym_error != NULL) {
        printf("Error finding function \"notified\": %s\n", dlsym_error);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    notified(buf);
}

/* Main child function acting as an event handler */
int child_main(void *arg) {
    void *handle = dlopen((const char *) arg, RTLD_LAZY);
    if (handle == NULL) {
        printf("Error opening file: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }
    struct process *process = search_process(getpid());

    update_buffers(handle);

    execute_init(handle);
    
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
                execute_notified(handle, buf);
            }
        }
    }

    dlclose(handle);
    return 0;
}