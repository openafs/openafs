#define OnspSigtype void /* sighandler type */
#define STDC_HEADERS 1 /* has ANSI C headers */
#define HAVE_STDLIB_H 1 /* stdlib.h */
#define CONFIG_RXK5 1 /* (always) include rxk5 support */
#undef CONFIG_RXKAD /* disable test support for rxkad */
#define CONFIG_CLEAR 0 /* disable rxk5_clear */
#undef NO_RX_H_VOID /* rx/rx.h does not define VOID? */
#undef HAVE_KERBEROSIV_KAFS_H /* kerberosIV/kafs.h */
#define HAVE_AFS_KAUTILS_H 1 /* afs/kautils.h */
