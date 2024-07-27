#include <stdio.h>

#include "linux_microkit.h"

#define SERVER_CHANNEL_ID 1

void init(void) {
    printf("==CLIENT PROCESS INITIALISED==\n");
    microkit_notify(SERVER_CHANNEL_ID);
}

void notified(microkit_channel ch) {
    printf("Notification received by client\n");
}