#pragma once

#include <stdint.h>

typedef uint64_t microkit_channel;
typedef uint64_t seL4_Word;
typedef seL4_Word microkit_msginfo;
typedef uint8_t seL4_Uint8;
typedef uint16_t seL4_Uint16;
typedef uint32_t seL4_Uint32;
typedef uint64_t seL4_Error;

/* User provided functions */
void init(void);
void notified(microkit_channel ch);
microkit_msginfo protected(microkit_channel ch, microkit_msginfo msginfo);

/*
 * Output a single character on the debug console.
 */
void microkit_dbg_putc(int c);

/*
 * Output a NUL terminated string to the debug console.
 */
void microkit_dbg_puts(const char *s);

/*
 * Output the decimal representation of an 8-bit integer to the debug console.
 */
void microkit_dbg_put8(seL4_Uint8 x);

/*
 * Output the decimal representation of an 32-bit integer to the debug console.
 */
void microkit_dbg_put32(seL4_Uint32 x);

static inline void microkit_internal_crash(seL4_Error err) {
    /*
     * Currently crash be dereferencing NULL page
     *
     * Actually derference 'err' which means the crash reporting will have
     * `err` as the fault address. A bit of a cute hack. Not a good long term
     * solution but good for now.
     */
    int *x = (int *)(seL4_Word) err;
    *x = 0;
}

/* Microkit API */
void microkit_notify(microkit_channel ch);

microkit_msginfo microkit_msginfo_new(seL4_Word label, seL4_Uint16 count);

void microkit_mr_set(seL4_Uint8 mr, seL4_Word value);

seL4_Word microkit_mr_get(seL4_Uint8 mr);

microkit_msginfo microkit_ppcall(microkit_channel ch, microkit_msginfo msginfo);