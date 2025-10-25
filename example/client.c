#include <microkit.h>

#define SERVER_CHANNEL_ID 1

char *buffer;

void init(void) {
    microkit_dbg_puts("== CLIENT PROCESS INITIALISED ==\n");
    microkit_notify(SERVER_CHANNEL_ID);
}

void notified(microkit_channel ch) {
    microkit_dbg_puts("Notification received by client.\n");
    microkit_dbg_puts("Client accessing the shared buffer: ");
    microkit_dbg_puts(buffer);
    microkit_dbg_puts("\n");
}

microkit_msginfo protected(microkit_channel ch, microkit_msginfo msginfo) {
    seL4_Word label = microkit_msginfo_get_label(msginfo);
    seL4_Word count = microkit_msginfo_get_count(msginfo);

    microkit_dbg_puts("Protected procedure call received by client with label: ");
    microkit_dbg_put32(label);
    microkit_dbg_puts(" and count: ");
    microkit_dbg_put32(count);
    microkit_dbg_puts("\n");

    for (int i = 0; i < count; i++) {
        microkit_dbg_puts("Message Register ");
        microkit_dbg_put32(i);
        microkit_dbg_puts(": ");
        microkit_dbg_put32(microkit_mr_get(i));
        microkit_dbg_puts("\n");
    }

    microkit_mr_set(0, 1);
    return microkit_msginfo_new(0, 1);
}