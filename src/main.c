#define _GNU_SOURCE

#include <handler.h>

#include <sched.h>
#include <sys/wait.h>

static void run_process(struct process *process, const char *path) {
    process->pid = clone(event_handler, process->stack_top, SIGCHLD, (void *) path);
    if (process->pid == -1) {
        printf("Error on cloning\n");
        exit(EXIT_FAILURE);
    }
}

static void add_channel(struct process *from, struct process *to, microkit_channel ch) {
    struct channel *curr_ch = channel_list;
    while (curr_ch != NULL && (curr_ch->channel_id != ch || curr_ch->to != to)) {
        curr_ch = curr_ch->next;
    }
    if (curr_ch == NULL) {
        curr_ch = create_channel(to, ch);
    }

    struct channel **temp = from->channel;
    while (*temp != NULL) {
        temp += sizeof(struct channel *);
    }
    *temp = curr_ch;
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
    add_channel(p1, p2, channel1_id);
    add_channel(p2, p1, channel2_id);

    // Start running the specified processes
    run_process(p1, "./user/server.so");
    run_process(p2, "./user/client.so");

    // Wait for the child processes to finish before leaving
    struct process *curr = process_list;
    while (curr != NULL) {
        if (waitpid(curr->pid, NULL, 0) == -1) {
            printf("Error on waitpid\n");
            return -1;
        }
        curr = curr->next;
    }

    // Unmap and free all used heap memory
    free_channels();
    free_shared_memory();
    free_processes();

    return 0;
}