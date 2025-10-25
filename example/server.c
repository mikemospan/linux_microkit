#include <microkit.h>

#define CLIENT_CHANNEL_ID 2

unsigned long buffer;

void init(void) {
    microkit_mr_set(0, 100);
    microkit_mr_set(1, 8);
    microkit_ppcall(CLIENT_CHANNEL_ID, microkit_msginfo_new(0, 2));

    microkit_dbg_puts("== SERVER PROCESS INITIALISED WITH RESPONSE: ");
    microkit_dbg_put32(microkit_mr_get(0));
    microkit_dbg_puts(" ==\n");
}

void notified(microkit_channel ch) {
    microkit_dbg_puts("Notification received by server. Sending a reply.\n");
    
    // Copy "Hello World!" into the shared buffer
    char *hello = "Hello World!";
    for (int i = 0; i < 13; i++) {
        ((char *) buffer)[i] = hello[i];
    }

    microkit_notify(CLIENT_CHANNEL_ID);
}