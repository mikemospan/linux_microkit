#include <data_structures.h>

#include <stdio.h>

#define MICROKIT_MAX_CHANNELS 62
#define MICROKIT_MAX_SHARED_MEMORY 62

extern struct process *process_list;
extern struct channel *channel_list;
extern struct shared_memory *shared_memory_list;

int child_main(void *arg);