#define _GNU_SOURCE

#include <handler.h>

#include <poll.h>
#include <dlfcn.h>

#define FDS_SIZE 496

struct info *info;

/* HELPER FUNCTIONS */

static void set_shared_memory(void *handle) {
    struct shared_memory *curr = shared_memory_list->head;
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
int event_handler(void *arg) {
    info = (struct info *) arg;
    void *handle = dlopen(info->path, RTLD_LAZY);
    if (handle == NULL) {
        printf("Error opening file: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    set_shared_memory(handle);
    execute_init(handle);
    
    // Declare and initialise necessary variables for polling from pipe
    struct pollfd *fds = malloc(FDS_SIZE);
    if (fds == NULL) {
        printf("Error on allocating poll fds\n");
        return -1;
    }

    fds->fd = info->process->pipefd[PIPE_READ_FD];
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
    free(info);
    return 0;
}