#define _GNU_SOURCE

#include <handler.h>

#include <sched.h>
#include <sys/wait.h>

static void run_process(struct process *process, const char *path) {
    struct info *info = malloc(sizeof(struct info));
    info->path = path;
    info->process = process;

    process->pid = clone(event_handler, process->stack_top, SIGCHLD, (void *) info);
    if (process->pid == -1) {
        printf("Error on cloning\n");
        exit(EXIT_FAILURE);
    }
}

/* MAIN FUNCTION ACTING AS A SETUP FOR ALL NECESSARY OBJECTS */

int main(void) {
    // Create our processes to act in place of protection domains
    struct process *p1 = create_process();
    struct process *p2 = create_process();

    // Create our shared memory and initialise it
    create_shared_memory("buffer", SHARED_MEM_SIZE);

    // Create our pipes and channels for interprocess communication
    microkit_channel channel1_id = 1;
    microkit_channel channel2_id = 2;
    create_channel(p1, p2, channel1_id);
    create_channel(p2, p1, channel2_id);

    // Start running the specified processes
    run_process(p1, "./user/server.so");
    run_process(p2, "./user/client.so");

    // Wait for the child processes to finish before leaving
    struct process *curr = process_list->head;
    while (curr != NULL) {
        if (waitpid(curr->pid, NULL, 0) == -1) {
            printf("Error on waitpid\n");
            return -1;
        }
        curr = curr->next;
    }

    // Unmap and free all used heap memory
    free_shared_memory();
    free_processes();

    return 0;
}