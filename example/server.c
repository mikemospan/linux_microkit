#include <stdio.h>
#include <string.h>

#include <microkit.h>

#define CLIENT_CHANNEL_ID 2

unsigned long buffer;

void init(void) {
    printf("==SERVER PROCESS INITIALISED==\n");
    microkit_ppcall(CLIENT_CHANNEL_ID);
}

void notified(microkit_channel ch) {
    printf("Notification received by server. Sending a reply.\n");
    memcpy((char *) buffer, "Hello World!", 13);
    microkit_notify(CLIENT_CHANNEL_ID);
}