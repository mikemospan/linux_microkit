#include <microkit.h>

#define SERVER_CHANNEL_ID 1

unsigned long buffer;

void init(void) {
    microkit_dbg_puts("== CLIENT PROCESS INITIALISED ==\n");
    microkit_notify(SERVER_CHANNEL_ID);
}

void notified(microkit_channel ch) {
    microkit_dbg_puts("Notification received by client.\n");
    microkit_dbg_puts("Client accessing the shared buffer: ");
    microkit_dbg_puts((char *) buffer);
    microkit_dbg_puts("\n");
}

microkit_msginfo protected(microkit_channel ch, microkit_msginfo msginfo) {
    seL4_Word first_word = microkit_mr_get(0);
    seL4_Word second_word = microkit_mr_get(1);
    
    microkit_dbg_puts("Protected Procedure call received by client: [");
    microkit_dbg_put32(first_word);
    microkit_dbg_puts(", ");
    microkit_dbg_put32(second_word);
    microkit_dbg_puts("]\n");

    microkit_mr_set(0, 1);
    return microkit_msginfo_new(0, 1);
}