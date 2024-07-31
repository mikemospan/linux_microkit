#include <pd_main.h>

struct process *search_process(int pid) {
    struct process *current = process_list;
    while (current != NULL && current->pid != pid) {
        current = current->next;
    }
    if (current == NULL) {
        printf("Could not find process with pid %d\n", pid);
        exit(EXIT_FAILURE);
    }
    return current;
}