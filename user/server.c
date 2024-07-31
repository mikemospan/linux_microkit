#include <stdio.h>
#include <string.h>

#include <linux_microkit.h>

#define CLIENT_CHANNEL_ID 1

unsigned long buffer;

void init(void) {
    printf("==SERVER PROCESS INITIALISED==\n");
}

void notified(microkit_channel ch) {
    printf("Notification received by server. Sending a reply.\n");
    memcpy((char *) buffer, "Hello World!", 13);
    microkit_notify(CLIENT_CHANNEL_ID);
}