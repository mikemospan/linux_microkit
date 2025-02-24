#include <microkit.h>
#include <handler.h>

#include <stdio.h>

extern struct process *proc;

/**
 * A simple helper function to send a message to a given channel.
 * @param ch An unsigned integer to the channel we will be sending the message to
 * @param msginfo A long pointer containing the message information used in ppc
 * @param ppc An integer set to 0 if the message is a notification, and 1 if it's a ppc
 */
static void send_message(microkit_channel ch, microkit_msginfo msginfo, int ppc) {
    khash_t(channel) *channel_id_to_process = proc->channel_id_to_process;
    khiter_t iter = kh_get(channel, channel_id_to_process, ch);
    if (kh_key(channel_id_to_process, iter) != ch) {
        printf("Channel id %u is not a valid channel\n", ch);
        exit(EXIT_FAILURE);
    }

    struct process *receiver = kh_value(channel_id_to_process, iter);

    struct message send = {.ch = ch, .msginfo = msginfo, .send_back = proc->receive_pipe[PIPE_WRITE_FD]};
    if (ppc) {
        send.ch += MICROKIT_MAX_PDS;
        memcpy(receiver->ipc_buffer, proc->ipc_buffer, msginfo);
    }

    write(receiver->send_pipe[PIPE_WRITE_FD], &send, sizeof(struct message));
}

/**
 * Sends a notification to the specified channel
 * @param ch An unsigned integer to the channel we will be sending a notification to
 */
void microkit_notify(microkit_channel ch) {
    send_message(ch, 0, 0);
}

/**
 * Creates a microkit message info struct. TODO: Complete.
 * @param label The label of the message
 * @param count The number of words in the message
 */
microkit_msginfo microkit_msginfo_new(seL4_Word label, seL4_Uint16 count) {
    return (microkit_msginfo) count;
}

/**
 * Sets the value of the provided message register.
 * @param mr The message register (ipc buffer index) to be set
 * @param value The value the message register will be set to
 */
void microkit_mr_set(seL4_Uint8 mr, seL4_Word value) {
    proc->ipc_buffer[mr] = value;
}

/**
 * Gets the value of the provided message register.
 * @param mr The message register (ipc buffer index) to be retrieved
 */
seL4_Word microkit_mr_get(seL4_Uint8 mr) {
    return proc->ipc_buffer[mr];
}

/**
 * Creates a microkit message info struct. TODO: Complete.
 * @param ch An unsigned integer to the channel we will be sending a ppc to 
 * @param msginfo The message information
 */
microkit_msginfo microkit_ppcall(microkit_channel ch, microkit_msginfo msginfo) {
    send_message(ch, msginfo, 1);

    long receive;
    read(proc->receive_pipe[PIPE_READ_FD], &receive, sizeof(long));
    
    return (microkit_msginfo) receive;
}