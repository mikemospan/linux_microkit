#define _GNU_SOURCE

#include <handler.h>

#include <sched.h>
#include <sys/wait.h>
#include <sys/inotify.h>

#define MAX_LINE_LENGTH 512
#define MAX_FUNC_AND_ARG_LENGTH 128

static void run_process(char *proc, char *path) {
    khiter_t piter = kh_get(process, process_map, proc);
    struct process *process = kh_value(process_map, piter);

    struct info *info = malloc(sizeof(struct info));
    info->path = path;
    info->process = process;

    process->pid = clone(event_handler, process->stack_top, SIGCHLD, (void *) info);
    if (process->pid == -1) {
        printf("Error on cloning\n");
        exit(EXIT_FAILURE);
    }
}

static void run_function(char line[MAX_LINE_LENGTH]) {
    char *func = strtok(line, " ");
    if (strcmp(func, "create_shared_memory") == 0) {
        char *name = malloc(MAX_FUNC_AND_ARG_LENGTH);
        memcpy(name, strtok(NULL, " "), MAX_FUNC_AND_ARG_LENGTH);
        
        int size = strtol(strtok(NULL, " "), NULL, 0);

        create_shared_memory(name, size);
    } else if (strcmp(func, "create_process") == 0) {
        char *name = malloc(MAX_FUNC_AND_ARG_LENGTH);
        memcpy(name, strtok(NULL, " "), MAX_FUNC_AND_ARG_LENGTH);
        name[strlen(name) - 1] = '\0';

        create_process(name);
    } else if (strcmp(func, "add_shared_memory") == 0) {
        char *proc_name = malloc(MAX_FUNC_AND_ARG_LENGTH);
        memcpy(proc_name, strtok(NULL, " "), MAX_FUNC_AND_ARG_LENGTH);

        char *shm_name = malloc(MAX_FUNC_AND_ARG_LENGTH);
        memcpy(shm_name, strtok(NULL, " "), MAX_FUNC_AND_ARG_LENGTH);
        shm_name[strlen(shm_name) - 1] = '\0';

        add_shared_memory(proc_name, shm_name);
        free(proc_name);
        free(shm_name);
    } else if (strcmp(func, "create_channel") == 0) {
        char *from = malloc(MAX_FUNC_AND_ARG_LENGTH);
        memcpy(from, strtok(NULL, " "), MAX_FUNC_AND_ARG_LENGTH);

        char *to = malloc(MAX_FUNC_AND_ARG_LENGTH);
        memcpy(to, strtok(NULL, " "), MAX_FUNC_AND_ARG_LENGTH);

        int ch = strtol(strtok(NULL, " "), NULL, 0);

        create_channel(from, to, ch);
        free(from);
        free(to);
    } else if (strcmp(func, "run_process") == 0) {
        char *process = malloc(MAX_FUNC_AND_ARG_LENGTH);
        memcpy(process, strtok(NULL, " "), MAX_FUNC_AND_ARG_LENGTH);

        char *elf = malloc(MAX_FUNC_AND_ARG_LENGTH);
        memcpy(elf, strtok(NULL, " "), MAX_FUNC_AND_ARG_LENGTH);
        elf[strlen(elf) - 1] = '\0';

        run_process(process, elf);
        free(process);
    }
}

void start_loader() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/usr/bin/python3", "python3", "./src/loader.py", (char *) NULL);
        exit(EXIT_FAILURE);
    }
    
    int fd = inotify_init();
    int wd = inotify_add_watch(fd, "./src", IN_MODIFY | IN_CREATE);
    char buffer[MAX_LINE_LENGTH];
    int length = read(fd, buffer, MAX_LINE_LENGTH);

    if (length < 0) {
        perror("read");
    }

    while ((length = read(fd, buffer, sizeof(buffer))) > 0) {
        int i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &buffer[i];
            if (((event->mask & IN_CREATE) || (event->mask & IN_MODIFY)) && strcmp(event->name, "loader.txt") == 0) {
                inotify_rm_watch(fd, wd);
                close(fd);
                return;
            }
            i += sizeof(struct inotify_event) + event->len;
        }
    }
}

/* MAIN FUNCTION ACTING AS A SETUP FOR ALL NECESSARY OBJECTS */

int main(void) {
    start_loader();

    FILE *system = fopen("./src/loader.txt", "r");
    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), system) != NULL) {
        run_function(line);
    }
    fclose(system);

    // Wait for the child processes to finish before leaving
    for (khint_t k = kh_begin(h); k != kh_end(process_map); ++k) {
        struct process *process = kh_value(process_map, k);
        if (waitpid(process->pid, NULL, 0) == -1) {
            printf("Error on waitpid\n");
            return -1;
        }
    }

    // Unmap and free all used heap memory
    free_resources();

    return 0;
}