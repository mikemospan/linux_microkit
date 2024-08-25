#define _GNU_SOURCE

#include <handler.h>

#include <poll.h>
#include <dlfcn.h>

struct process *proc;

/* HELPER FUNCTIONS */

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

static void *get_function(void *handle, const char *func_name) {
    void *func_ptr = dlsym(handle, func_name);
    const char *dlsym_error = dlerror();

    if (dlsym_error != NULL) {
        printf("Error finding function \"%s\": %s\n", func_name, dlsym_error);
        dlclose(handle);
        exit(EXIT_FAILURE);
    }

    return func_ptr;
}

static void *notification_thread(void *arg) {
    struct pollfd *fds = (struct pollfd *) ((long *) arg)[1];
    void *handle = (void *) ((long *) arg)[0];
    if (fds->revents & POLLIN) {
        microkit_channel buf;
        read(fds->fd, &buf, sizeof(microkit_channel));
        ((void (*)(microkit_channel)) get_function(handle, "notified"))(buf);
    }
    return NULL;
}

static void notification_handler(void *handle) {
    struct pollfd *fds = malloc(sizeof(struct pollfd));
    if (fds == NULL) {
        printf("Error on allocating poll fds\n");
        exit(EXIT_FAILURE);
    }

    fds->fd = proc->notif_pipe[PIPE_READ_FD];
    fds->events = POLLIN;

    while (ppoll(fds, 1, NULL, NULL)) {
        pthread_t thread_id;
        long addresses[2] = {(long) handle, (long) fds};
        pthread_create(&thread_id, NULL, notification_thread, addresses);
    }

    free(fds);
}

static void *ppcall_handler(void *handle) {
    struct pollfd *fds = malloc(sizeof(struct pollfd));
    if (fds == NULL) {
        printf("Error on allocating poll fds\n");
        exit(EXIT_FAILURE);
    }

    fds->fd = proc->ppc_pipe[PIPE_READ_FD];
    fds->events = POLLIN;

    while (ppoll(fds, 1, NULL, NULL)) {
        if (fds->revents & POLLIN) {
            struct ppc_message buf;
            read(fds->fd, &buf, sizeof(struct ppc_message));
            kill(buf.pid, SIGSTOP);
            ((void (*)(microkit_channel)) get_function(handle, "protected"))(buf.ch);
            kill(buf.pid, SIGCONT);
        }
    }

    free(fds);
    return NULL;
}

/* Main child function acting as an event handler */
int event_handler(void *arg) {
    proc = (struct process *) arg;

    void *handle = dlopen(proc->path, RTLD_LAZY);
    if (handle == NULL) {
        printf("Error opening file: %s\n", dlerror());
        exit(EXIT_FAILURE);
    }

    set_shared_memory(handle, proc);
    void (*init)(void) = get_function(handle, "init");
    init();
    
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, ppcall_handler, handle);
    notification_handler(handle);

    dlclose(handle);
    return 0;
}