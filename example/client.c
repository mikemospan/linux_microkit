#include <stdio.h>

#include <microkit.h>

#define SERVER_CHANNEL_ID 1

unsigned long buffer;

void init(void) {
    printf("== CLIENT PROCESS INITIALISED ==\n");
    microkit_notify(SERVER_CHANNEL_ID);
}

void notified(microkit_channel ch) {
    printf("Notification received by client.\n");
    printf("Client accessing the shared buffer: %s\n", (char *) buffer);
}

microkit_msginfo protected(microkit_channel ch, microkit_msginfo msginfo) {
    seL4_Word first_word = microkit_mr_get(0);
    seL4_Word second_word = microkit_mr_get(1);
    printf("Protected Procedure call received by client: [%ld, %ld]\n", first_word, second_word);

    microkit_mr_set(0, 1);
    return microkit_msginfo_new(0, 1);
}