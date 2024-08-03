#include <data_structures.h>

#include <stdio.h>

#define PAGE_SIZE 4096
#define STACK_SIZE PAGE_SIZE

#define MICROKIT_MAX_CHANNELS 62

extern khash_t(process) *process_map;
extern khash_t(shared_memory) *shared_memory_map;

int event_handler(void *arg);