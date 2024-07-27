#include <stdio.h>

#include "linux_microkit.h"

void init(void) {
    printf("==CHILD PROCESS INITIALISED==\n");
}

void notified(microkit_channel ch) {
    printf("Notification received by process with id: %d, on channel: %u!\n", getpid(), ch);
}