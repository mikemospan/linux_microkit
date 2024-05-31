#pragma once

#define __thread

/* User provided functions */
void init(void);
void notified(unsigned int pid);

static inline void microkit_notify(unsigned int pid) {
    int ret = kill(pid, SIGUSR1);
    if (ret == -1) {
        fprintf(stderr, "Could not notify\n");
    }
}