#include <microkit.h>
#include <handler.h>

#include <stdio.h>

static void send_message(microkit_channel ch, microkit_msginfo msginfo, int ppc) {
    extern struct process *proc;
    khash_t(channel) *channel_map = proc->channel_map;
    khiter_t iter = kh_get(channel, channel_map, ch);
    if (kh_key(channel_map, iter) != ch) {
        printf("Channel id %u is not a valid channel\n", ch);
        exit(EXIT_FAILURE);
    }

    struct message send;
    send.ch = ppc ? ch + MICROKIT_MAX_PDS : ch;
    send.msginfo = msginfo;
    send.send_back = proc->ppc_reply[PIPE_WRITE_FD];

    write(kh_value(channel_map, iter)->pipefd[PIPE_WRITE_FD], &send, sizeof(struct message));
}

void microkit_notify(microkit_channel ch) {
    send_message(ch, NULL, 0);
}

microkit_msginfo microkit_msginfo_new(seL4_Word label, seL4_Uint16 count) {
    return NULL;
}

microkit_msginfo microkit_ppcall(microkit_channel ch, microkit_msginfo msginfo) {
    send_message(ch, msginfo, 1);

    extern struct process *proc;
    long receive;
    read(proc->ppc_reply[PIPE_READ_FD], &receive, sizeof(long));
    
    return (microkit_msginfo) receive;
}