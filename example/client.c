#include <stdio.h>

#include <microkit.h>

#define SERVER_CHANNEL_ID 1

unsigned long buffer;

void init(void) {
    printf("==CLIENT PROCESS INITIALISED==\n");
    microkit_notify(SERVER_CHANNEL_ID);
}

void notified(microkit_channel ch) {
    printf("Notification received by client.\n");
    printf("Client accessing the shared buffer: %s\n", (char *) buffer);
}

microkit_msginfo protected(microkit_channel ch, microkit_msginfo msginfo) {
    seL4_Word word = microkit_mr_get(0);
    printf("Protected Procedure call received by client: %ld\n", word);
    return 0;
}