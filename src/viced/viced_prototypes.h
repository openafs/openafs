extern int sendBufSize;
afs_int32 sys_error_to_et(afs_int32 in);
void init_sys_error_to_et(void);

/* First 32 bits of capabilities */
#define CAPABILITY_ERRORTRANS (1<<1)

#define CAPABILITY_BITS 1
