#define _GNU_SOURCE

#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdbool.h>

#include "include/linux_microkit.h"

#define PAGE_SIZE 4096
#define STACK_SIZE PAGE_SIZE
#define SHARED_MEM_SIZE PAGE_SIZE

void *shared_buffer;
bool *state_check;

/* USER PROVIDED FUNCTIONS */

__attribute__((weak)) void init(void) {
    printf("==CHILD PROCESS INITIALISED==\n");
}

__attribute__((weak)) void notified(unsigned int pid) {
    printf("Notification received by process with id: %d, from process id: %d!\n", getpid(), pid);
}

/* EVENT HANDLER */

static int start_child(__attribute__((unused)) void *arg) {
    init();
    printf("Child process received the message \"%s\" from shared buffer.\n", (char *) shared_buffer);
    *state_check = true;
    while (*state_check);
    return 0;
}

static void signal_handler(int signo, siginfo_t *info, void *context) {
    if (signo == SIGUSR1) {
        notified(info->si_pid);
    } else {
        fprintf(stderr, "Received unexpected signal\n");
    }
}

int main(void) {
    // Initialise the child's stack
    char *stack = malloc(STACK_SIZE);
    if (stack == NULL) {
        fprintf(stderr, "Error on allocating stack\n");
        return -1;
    }

    // Create our shared memory and initialise it
    shared_buffer = mmap(NULL, SHARED_MEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (shared_buffer == MAP_FAILED) {
        fprintf(stderr, "Error on allocating shared buffer\n");
        return -1;
    }
    strncpy(shared_buffer, "Hello World!", SHARED_MEM_SIZE);

    // Create some more shared memory which is necessary to check state of initialisation/signalling
    state_check = mmap(NULL, sizeof(bool), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (state_check == MAP_FAILED) {
        fprintf(stderr, "Error on allocating shared memory to check initialisation\n");
        return -1;
    }
    *state_check = false;

    // Initialise signal routing to signal_handler function
    struct sigaction new_action = { 0 };
    struct sigaction old_action;

    new_action.sa_sigaction = signal_handler;
    new_action.sa_flags = SA_SIGINFO;
    sigemptyset(&new_action.sa_mask);
    if (sigaction(SIGUSR1, &new_action, NULL) == -1) {
        fprintf(stderr, "Error on rerouting signal\n");
        return -1;
    }

    // Create the child process
    char *stack_top = stack + STACK_SIZE;
    int pid = clone(start_child, stack_top, SIGCHLD, NULL);
    if (pid == -1) {
        fprintf(stderr, "Error on cloning\n");
        return -1;
    }

    // Test running a notify
    while (!(*state_check));
    microkit_notify(pid);
    *state_check = false;

    // Wait for the child process to finish before leaving
    if (waitpid(pid, NULL, 0) == -1) {
        fprintf(stderr, "Error on waitpid\n");
        return -1;
    }

    munmap(state_check, sizeof(bool));
    munmap(shared_buffer, SHARED_MEM_SIZE);
    free(stack);

    return 0;
}