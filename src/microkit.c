#include <microkit.h>
#include <handler.h>

static struct process *get_sending_process(microkit_channel ch) {
    extern struct process *proc;
    khash_t(channel) *channel_map = proc->channel_map;
    khiter_t iter = kh_get(channel, channel_map, ch);
    if (kh_key(channel_map, iter) != ch) {
        printf("Channel id %u is not a valid channel\n", ch);
        exit(EXIT_FAILURE);
    }
    return kh_value(channel_map, iter);
}

void microkit_notify(microkit_channel ch) {
    write(get_sending_process(ch)->notif_pipe[PIPE_WRITE_FD], &ch, sizeof(microkit_channel));
}

void microkit_mr_set(seL4_Uint8 mr, seL4_Word value) {

}

void microkit_ppcall(microkit_channel ch) {
    struct ppc_message ppc_message;
    ppc_message.ch = ch;
    ppc_message.pid = getpid();

    write(get_sending_process(ch)->ppc_pipe[PIPE_WRITE_FD], &ppc_message, sizeof(struct ppc_message));
}