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
