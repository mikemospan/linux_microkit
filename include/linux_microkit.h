#pragma once

#define __thread

typedef unsigned int microkit_channel;

/* User provided functions */
void init(void);
void notified(microkit_channel ch);

static inline void microkit_notify(microkit_channel ch) {
    int ret = kill(ch, SIGUSR1);
    if (ret == -1) {
        fprintf(stderr, "Could not notify\n");
    }
}