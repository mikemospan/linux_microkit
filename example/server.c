#include <stdio.h>
#include <string.h>

#include <microkit.h>

#define CLIENT_CHANNEL_ID 2

unsigned long buffer;

void init(void) {
    microkit_mr_set(0, 100);
    microkit_mr_set(1, 8);
    microkit_ppcall(CLIENT_CHANNEL_ID, microkit_msginfo_new(0, 2));
    printf("== SERVER PROCESS INITIALISED WITH RESPONSE: %ld ==\n", microkit_mr_get(0));
}

void notified(microkit_channel ch) {
    printf("Notification received by server. Sending a reply.\n");
    memcpy((char *) buffer, "Hello World!", 13);
    microkit_notify(CLIENT_CHANNEL_ID);
}