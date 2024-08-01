#include <linux_microkit.h>
#include <data_structures.h>

#include <stdio.h>

void microkit_notify(microkit_channel ch) {
    extern struct info *info;
    khash_t(channel) *channel_map = info->process->channel_map;
    khiter_t iter = kh_get(channel, channel_map, ch);
    if (kh_key(channel_map, iter) != ch) {
        printf("Channel id %u is not a valid channel\n", ch);
        exit(EXIT_FAILURE);
    }
    struct process *to = kh_value(channel_map, iter);
    write(to->pipefd[PIPE_WRITE_FD], &ch, sizeof(microkit_channel));
}