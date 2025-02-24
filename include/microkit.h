typedef unsigned int microkit_channel;
typedef long seL4_Word;
typedef seL4_Word microkit_msginfo;
typedef unsigned char seL4_Uint8;
typedef unsigned short seL4_Uint16;

/* User provided functions */
void init(void);
void notified(microkit_channel ch);
microkit_msginfo protected(microkit_channel ch, microkit_msginfo msginfo);

/* Microkit API */
void microkit_notify(microkit_channel ch);

microkit_msginfo microkit_msginfo_new(seL4_Word label, seL4_Uint16 count);

void microkit_mr_set(seL4_Uint8 mr, seL4_Word value);

seL4_Word microkit_mr_get(seL4_Uint8 mr);

microkit_msginfo microkit_ppcall(microkit_channel ch, microkit_msginfo msginfo);