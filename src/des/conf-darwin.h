#define BITS32
#define BIG
#undef BSDUNIX
#if defined(__ppc__)
#define MSBFIRST
#elif defined(__i386__)
#define LSBFIRST
#else
#error "MSBFIRST or LSBFIRST undefined"
#endif
#define MUSTALIGN
#if !defined(KERNEL) && defined(AFS_DARWIN80_ENV)
extern int _darwin_swap_long_bytes_bit_number(afs_uint32 x);
extern char *_darwin_whichstr[];
extern int _darwin_which;
#endif /* !KERNEL && AFS_DARWIN80_ENV */
