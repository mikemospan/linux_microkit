#include <stdio.h>

#include <microkit.h>

#define SERVER_CHANNEL_ID 2

unsigned long buffer;

void init(void) {
    printf("==CLIENT PROCESS INITIALISED==\n");
    microkit_notify(SERVER_CHANNEL_ID);
}

void notified(microkit_channel ch) {
    printf("Notification received by client.\n");
    printf("Client accessing the shared buffer: %s\n", (char *) buffer);
}