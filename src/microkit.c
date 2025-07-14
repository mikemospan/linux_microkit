#include <microkit.h>
#include <handler.h>
#include <stdio.h>

extern process_t *proc;

/**
 * Get the receiver process for a channel
 * @param ch Channel identifier
 */
static process_t *get_channel_receiver(microkit_channel ch) {
    khash_t(channel) *channel_id_to_process = proc->channel_id_to_process;
    khiter_t iter = kh_get(channel, channel_id_to_process, ch);
    if (kh_key(channel_id_to_process, iter) != ch) {
        fprintf(stderr, "Channel id %lu is not a valid channel\n", ch);
        exit(EXIT_FAILURE);
    }
    return kh_value(channel_id_to_process, iter);
}

/**
 * Sends a notification to the specified channel
 * @param ch An unsigned integer to the channel we will be sending a notification to
 */
void microkit_notify(microkit_channel ch) {
    write(get_channel_receiver(ch)->notification, &ch, sizeof(microkit_channel));
}

/**
 * Creates a microkit message info struct. TODO: Figure out what to do with the label.
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
 * Sends a protected procedure call across the provided channel.
 * @param ch An unsigned integer to the channel we will be sending a ppc to
 * @param msginfo The message information
 */
microkit_msginfo microkit_ppcall(microkit_channel ch, microkit_msginfo msginfo) {
    process_t *receiver = get_channel_receiver(ch);
    message_t send = {.ch = ch, .msginfo = msginfo, .send_back = proc->receive_pipe[PIPE_WRITE_FD]};
    memcpy(receiver->ipc_buffer, proc->ipc_buffer, msginfo * sizeof(seL4_Word));
    write(receiver->send_pipe[PIPE_WRITE_FD], &send, sizeof(message_t));

    microkit_msginfo receive;
    read(proc->receive_pipe[PIPE_READ_FD], &receive, sizeof(long));
    memcpy(proc->ipc_buffer, receiver->ipc_buffer, receive * sizeof(seL4_Word));
   
    return receive;
}