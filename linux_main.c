#define _GNU_SOURCE

#include <stdio.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/mman.h>

#define PAGE_SIZE 4096
#define STACK_SIZE PAGE_SIZE
#define SHARED_MEM_SIZE PAGE_SIZE

void *shared_buffer;

static int receive(void *arg __attribute__((unused))) {
    printf("Child process received the message: %s\n", (char *) shared_buffer);
    return 0;
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
        fprintf(stderr, "Error on allocating shared memory\n");
        return -1;
    }
    strncpy(shared_buffer, "Hello World!", SHARED_MEM_SIZE);

    // Create the child process
    char *stack_top = stack + STACK_SIZE;
    int pid = clone(receive, stack_top, SIGCHLD, NULL);
    if (pid == -1) {
        fprintf(stderr, "Error on cloning\n");
        return -1;
    }

    // Wait for the child process to finish before leaving
    if (waitpid(pid, NULL, 0) == -1) {
        fprintf(stderr, "Error on waitpid\n");
        return -1;
    }

    return 0;
}