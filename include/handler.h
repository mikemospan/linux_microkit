#include <data_structures.h>

#include <stdio.h>

#define PAGE_SIZE 4096
#define STACK_SIZE PAGE_SIZE
#define SHARED_MEM_SIZE PAGE_SIZE

#define MICROKIT_MAX_CHANNELS 62
#define MICROKIT_MAX_SHARED_MEMORY 62

extern struct process *process_stack;
extern struct shared_memory *shared_memory_stack;

int event_handler(void *arg);