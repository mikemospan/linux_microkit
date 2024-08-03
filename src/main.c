#define _GNU_SOURCE

#include <handler.h>

#include <sched.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sys/inotify.h>
#include <sys/mman.h>
#include <sys/stat.h>

#define function_executor(fn, arg1) fn(arg1)

static void run_process(struct process *process, const char *path) {
    struct info *info = malloc(sizeof(struct info));
    info->path = path;
    info->process = process;

    process->pid = clone(event_handler, process->stack_top, SIGCHLD, (void *) info);
    if (process->pid == -1) {
        printf("Error on cloning\n");
        exit(EXIT_FAILURE);
    }
}

static char remove_whitespace(FILE *system) {
    char c;
    while (isspace(c = fgetc(system)));
    if (c == EOF) {
        return EOF;
    }
    fseek(system, -2, SEEK_CUR);
    return fgetc(system);
}

static void match_memory_region(char line[4096]) {
    int j = 14;
    if (strncmp(line, "<memory_region", j) != 0) return;
    char new_line[4096];
    for (int i = j; line[i] != '\\' && line[i+1] != '>'; i++) {
        if (isspace(line[i]) && line[i-1] != '"') {
            j++;
        } else {
            new_line[i - j] = line[i];
            new_line[i - j + 1] = '\0';
        }
    }
    printf("%s\n", new_line);
}

static void load_line(FILE *system, char *c, char line[4096]) {
    for (int i = 0; (*c = fgetc(system)) != EOF; i++) {
        if (*c == '\n') {
            *c = remove_whitespace(system);
        }
        line[i] = *c;
        line[i + 1] = '\0';
        if (*c == '>') {
            break;
        }
    }
}

static void load_system_file(const char *path) {
    FILE *system = fopen(path, "r");
    if (system == NULL) {
        printf("Could not find system file.\n");
        exit(EXIT_FAILURE);
    }

    char c;
    while ((c = remove_whitespace(system)) != EOF) {
        char line[4096];
        load_line(system, &c, line);
        match_memory_region(line);
    }
}

static void load_python() {
    pid_t pid = fork();
    if (pid == 0) {
        execl("/usr/bin/python3", "python3", "./src/loader.py", (char *) NULL);
        exit(EXIT_FAILURE);
    }
    
    int fd = inotify_init();
    int wd = inotify_add_watch(fd, "./src", IN_MODIFY | IN_CREATE);
    char buffer[1024];
    int length = read(fd, buffer, 1024);

    if (length < 0) {
        perror("read");
    }

    while ((length = read(fd, buffer, sizeof(buffer))) > 0) {
        int i = 0;
        while (i < length) {
            struct inotify_event *event = (struct inotify_event *) &buffer[i];
            if (event->len) {
                if (((event->mask & IN_CREATE) || (event->mask & IN_MODIFY)) && strcmp(event->name, "loader.txt") == 0) {
                    FILE *system = fopen("./src/loader.txt", "r");
                    if (!system) {
                        perror("fopen");
                    }
                    char line[1024];
                    while (fgets(line, sizeof(line), system) != NULL) {
                        printf("%s", line);
                    }
                    fclose(system);
                    return;
                }
            }
            i += sizeof(struct inotify_event) + event->len;
        }
    }

    inotify_rm_watch(fd, wd);
    close(fd);
}

static void print(char *str) {
    printf("%s\n", str);
}

/* MAIN FUNCTION ACTING AS A SETUP FOR ALL NECESSARY OBJECTS */

int main(void) {
    // Create our processes to act in place of protection domains
    struct process *p1 = create_process();
    struct process *p2 = create_process();

    // Create our shared memory and initialise it
    create_shared_memory("buffer", SHARED_MEM_SIZE);
    add_shared_memory(p1, "buffer");
    add_shared_memory(p2, "buffer");

    // Create our pipes and channels for interprocess communication
    microkit_channel channel1_id = 1;
    microkit_channel channel2_id = 2;
    create_channel(p1, p2, channel1_id);
    create_channel(p2, p1, channel2_id);

    // Start running the specified processes
    run_process(p1, "./user/server.so");
    run_process(p2, "./user/client.so");

    // Wait for the child processes to finish before leaving
    struct process *curr = process_stack;
    while (curr != NULL) {
        if (waitpid(curr->pid, NULL, 0) == -1) {
            printf("Error on waitpid\n");
            return -1;
        }
        curr = curr->next;
    }

    // Unmap and free all used heap memory
    free_resources();

    return 0;
}