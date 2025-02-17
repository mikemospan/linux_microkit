#include <microkit.h>
#include <handler.h>

#include <stdio.h>

/**
 * A simple helper function to send a message to a given channel.
 * @param ch An unsigned integer to the channel we will be sending the message to
 * @param msginfo A long pointer containing the message information used in ppc
 * @param ppc An integer set to 0 if the message is a notification, and 1 if it's a ppc
 */
static void send_message(microkit_channel ch, microkit_msginfo msginfo, int ppc) {
    extern struct process *proc;
    khash_t(channel) *channel_id_to_process = proc->channel_id_to_process;
    khiter_t iter = kh_get(channel, channel_id_to_process, ch);
    if (kh_key(channel_id_to_process, iter) != ch) {
        printf("Channel id %u is not a valid channel\n", ch);
        exit(EXIT_FAILURE);
    }

    struct message send;
    send.ch = ppc ? ch + MICROKIT_MAX_PDS : ch;
    send.msginfo = msginfo;
    send.send_back = proc->ppc_reply[PIPE_WRITE_FD];

    write(kh_value(channel_id_to_process, iter)->pipefd[PIPE_WRITE_FD], &send, sizeof(struct message));
}

/**
 * Sends a notification to the specified channel
 * @param ch An unsigned integer to the channel we will be sending a notification to
 */
void microkit_notify(microkit_channel ch) {
    send_message(ch, NULL, 0);
}

/**
 * Creates a microkit message info struct. TODO: Complete.
 * @param label The label of the message
 * @param count The number of words in the message
 */
microkit_msginfo microkit_msginfo_new(seL4_Word label, seL4_Uint16 count) {
    return NULL;
}

/**
 * Creates a microkit message info struct. TODO: Complete.
 * @param ch An unsigned integer to the channel we will be sending a ppc to 
 * @param msginfo The message information
 */
microkit_msginfo microkit_ppcall(microkit_channel ch, microkit_msginfo msginfo) {
    send_message(ch, msginfo, 1);

    extern struct process *proc;
    long receive;
    read(proc->ppc_reply[PIPE_READ_FD], &receive, sizeof(long));
    
    return (microkit_msginfo) receive;
}