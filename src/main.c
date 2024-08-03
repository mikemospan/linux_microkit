#define _GNU_SOURCE

#include <handler.h>

#include <sched.h>
#include <sys/wait.h>

static void run_process(const char *proc, const char *path) {
    khiter_t piter = kh_get(process, process_map, proc);
    struct process *process = kh_value(process_map, piter);

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
    create_process("server");
    create_process("client");

    // Create our shared memory and initialise it
    create_shared_memory("buffer", SHARED_MEM_SIZE);

    // Create our pipes and channels for interprocess communication
    microkit_channel channel1_id = 1;
    microkit_channel channel2_id = 2;
    create_channel("server", "client", channel1_id);
    create_channel("client", "server", channel2_id);

    // Start running the specified processes
    run_process("server", "./user/server.so");
    run_process("client", "./user/client.so");

    // Wait for the child processes to finish before leaving
    for (khint_t k = kh_begin(h); k != kh_end(process_map); ++k) {
        struct process *process = kh_value(process_map, k);
        if (waitpid(process->pid, NULL, 0) == -1) {
            printf("Error on waitpid\n");
            return -1;
        }
    }

    // Unmap and free all used heap memory
    free_shared_memory();
    free_processes();

    return 0;
}