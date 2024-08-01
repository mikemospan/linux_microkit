typedef unsigned int microkit_channel;

/* User provided functions */
void init(void);
void notified(microkit_channel ch);

/* Microkit API */
void microkit_notify(microkit_channel ch);