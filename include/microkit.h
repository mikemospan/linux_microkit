typedef unsigned int microkit_channel;
typedef long seL4_Word;
typedef unsigned char seL4_Uint8;

/* User provided functions */
void init(void);
void notified(microkit_channel ch);
void protected(microkit_channel ch);

/* Microkit API */
void microkit_notify(microkit_channel ch);

void microkit_ppcall(microkit_channel ch);

void microkit_mr_set(seL4_Uint8 mr, seL4_Word value);