#include <stdio.h>

#include "linux_microkit.h"

#define CLIENT_CHANNEL_ID 2

void init(void) {
    printf("==SERVER PROCESS INITIALISED==\n");
}

void notified(microkit_channel ch) {
    printf("Notification received by server. Sending a reply.\n");
    microkit_notify(CLIENT_CHANNEL_ID);
}