#ifdef AFS_NT40_ENV
#include <afs/errmap_nt.h>
#endif

#ifndef EIO
#error Cant deal with this case
#endif /* EIO */

/* Cases we know we should deal with */
#ifndef EDQUOT
#define EDQUOT ENOSPC
#endif /* EDQUOT */

/* Map everything else to EIO */
#ifndef EPERM
#define EPERM EIO
#endif /* EPERM */
#ifndef ENOENT
#define ENOENT EIO
#endif /* ENOENT */
#ifndef ESRCH
#define ESRCH EIO
#endif /* ESRCH */
#ifndef EINTR
#define EINTR EIO
#endif /* EINTR */
#ifndef ENXIO
#define ENXIO EIO
#endif /* ENXIO */
#ifndef E2BIG
#define E2BIG EIO
#endif /* E2BIG */
#ifndef ENOEXEC
#define ENOEXEC EIO
#endif /* ENOEXEC */
#ifndef EBADF
#define EBADF EIO
#endif /* EBADF */
#ifndef ECHILD
#define ECHILD EIO
#endif /* ECHILD */
#ifndef EAGAIN
#define EAGAIN EIO
#endif /* EAGAIN */
#ifndef ENOMEM
#define ENOMEM EIO
#endif /* ENOMEM */
#ifndef EACCES
#define EACCES EIO
#endif /* EACCES */
#ifndef EFAULT
#define EFAULT EIO
#endif /* EFAULT */
#ifndef ENOTBLK
#define ENOTBLK EIO
#endif /* ENOTBLK */
#ifndef EBUSY
#define EBUSY EIO
#endif /* EBUSY */
#ifndef EEXIST
#define EEXIST EIO
#endif /* EEXIST */
#ifndef EXDEV
#define EXDEV EIO
#endif /* EXDEV */
#ifndef ENODEV
#define ENODEV EIO
#endif /* ENODEV */
#ifndef ENOTDIR
#define ENOTDIR EIO
#endif /* ENOTDIR */
#ifndef EISDIR
#define EISDIR EIO
#endif /* EISDIR */
#ifndef EINVAL
#define EINVAL EIO
#endif /* EINVAL */
#ifndef ENFILE
#define ENFILE EIO
#endif /* ENFILE */
#ifndef EMFILE
#define EMFILE EIO
#endif /* EMFILE */
#ifndef ENOTTY
#define ENOTTY EIO
#endif /* ENOTTY */
#ifndef ETXTBSY
#define ETXTBSY EIO
#endif /* ETXTBSY */
#ifndef EFBIG
#define EFBIG EIO
#endif /* EFBIG */
#ifndef ENOSPC
#define ENOSPC EIO
#endif /* ENOSPC */
#ifndef ESPIPE
#define ESPIPE EIO
#endif /* ESPIPE */
#ifndef EROFS
#define EROFS EIO
#endif /* EROFS */
#ifndef EMLINK
#define EMLINK EIO
#endif /* EMLINK */
#ifndef EPIPE
#define EPIPE EIO
#endif /* EPIPE */
#ifndef EDOM
#define EDOM EIO
#endif /* EDOM */
#ifndef ERANGE
#define ERANGE EIO
#endif /* ERANGE */
#ifndef EDEADLK
#define EDEADLK EIO
#endif /* EDEADLK */
#ifndef ENAMETOOLONG
#define ENAMETOOLONG EIO
#endif /* ENAMETOOLONG */
#ifndef ENOLCK
#define ENOLCK EIO
#endif /* ENOLCK */
#ifndef ENOSYS
#define ENOSYS EIO
#endif /* ENOSYS */
#ifndef ENOTEMPTY
#define ENOTEMPTY EIO
#endif /* ENOTEMPTY */
#ifndef ELOOP
#define ELOOP EIO
#endif /* ELOOP */
#ifndef EWOULDBLOCK
#define EWOULDBLOCK EIO
#endif /* EWOULDBLOCK */
#ifndef ENOMSG
#define ENOMSG EIO
#endif /* ENOMSG */
#ifndef EIDRM
#define EIDRM EIO
#endif /* EIDRM */
#ifndef ECHRNG
#define ECHRNG EIO
#endif /* ECHRNG */
#ifndef EL2NSYNC
#define EL2NSYNC EIO
#endif /* EL2NSYNC */
#ifndef EL3HLT
#define EL3HLT EIO
#endif /* EL3HLT */
#ifndef EL3RST
#define EL3RST EIO
#endif /* EL3RST */
#ifndef ELNRNG
#define ELNRNG EIO
#endif /* ELNRNG */
#ifndef EUNATCH
#define EUNATCH EIO
#endif /* EUNATCH */
#ifndef ENOCSI
#define ENOCSI EIO
#endif /* ENOCSI */
#ifndef EL2HLT
#define EL2HLT EIO
#endif /* EL2HLT */
#ifndef EBADE
#define EBADE EIO
#endif /* EBADE */
#ifndef EBADR
#define EBADR EIO
#endif /* EBADR */
#ifndef EXFULL
#define EXFULL EIO
#endif /* EXFULL */
#ifndef ENOANO
#define ENOANO EIO
#endif /* ENOANO */
#ifndef EBADRQC
#define EBADRQC EIO
#endif /* EBADRQC */
#ifndef EBADSLT
#define EBADSLT EIO
#endif /* EBADSLT */
#ifndef EBFONT
#define EBFONT EIO
#endif /* EBFONT */
#ifndef ENOSTR
#define ENOSTR EIO
#endif /* ENOSTR */
#ifndef ENODATA
#define ENODATA EIO
#endif /* ENODATA */
#ifndef ETIME
#define ETIME EIO
#endif /* ETIME */
#ifndef ENOSR
#define ENOSR EIO
#endif /* ENOSR */
#ifndef ENONET
#define ENONET EIO
#endif /* ENONET */
#ifndef ENOPKG
#define ENOPKG EIO
#endif /* ENOPKG */
#ifndef EREMOTE
#define EREMOTE EIO
#endif /* EREMOTE */
#ifndef ENOLINK
#define ENOLINK EIO
#endif /* ENOLINK */
#ifndef EADV
#define EADV EIO
#endif /* EADV */
#ifndef ESRMNT
#define ESRMNT EIO
#endif /* ESRMNT */
#ifndef ECOMM
#define ECOMM EIO
#endif /* ECOMM */
#ifndef EPROTO
#define EPROTO EIO
#endif /* EPROTO */
#ifndef EMULTIHOP
#define EMULTIHOP EIO
#endif /* EMULTIHOP */
#ifndef EDOTDOT
#define EDOTDOT EIO
#endif /* EDOTDOT */
#ifndef EBADMSG
#define EBADMSG EIO
#endif /* EBADMSG */
#ifndef EOVERFLOW
#define EOVERFLOW EIO
#endif /* EOVERFLOW */
#ifndef ENOTUNIQ
#define ENOTUNIQ EIO
#endif /* ENOTUNIQ */
#ifndef EBADFD
#define EBADFD EIO
#endif /* EBADFD */
#ifndef EREMCHG
#define EREMCHG EIO
#endif /* EREMCHG */
#ifndef ELIBACC
#define ELIBACC EIO
#endif /* ELIBACC */
#ifndef ELIBBAD
#define ELIBBAD EIO
#endif /* ELIBBAD */
#ifndef ELIBSCN
#define ELIBSCN EIO
#endif /* ELIBSCN */
#ifndef ELIBMAX
#define ELIBMAX EIO
#endif /* ELIBMAX */
#ifndef ELIBEXEC
#define ELIBEXEC EIO
#endif /* ELIBEXEC */
#ifndef EILSEQ
#define EILSEQ EIO
#endif /* EILSEQ */
#ifndef ERESTART
#define ERESTART EIO
#endif /* ERESTART */
#ifndef ESTRPIPE
#define ESTRPIPE EIO
#endif /* ESTRPIPE */
#ifndef EUSERS
#define EUSERS EIO
#endif /* EUSERS */
#ifndef ENOTSOCK
#define ENOTSOCK EIO
#endif /* ENOTSOCK */
#ifndef EDESTADDRREQ
#define EDESTADDRREQ EIO
#endif /* EDESTADDRREQ */
#ifndef EMSGSIZE
#define EMSGSIZE EIO
#endif /* EMSGSIZE */
#ifndef EPROTOTYPE
#define EPROTOTYPE EIO
#endif /* EPROTOTYPE */
#ifndef ENOPROTOOPT
#define ENOPROTOOPT EIO
#endif /* ENOPROTOOPT */
#ifndef EPROTONOSUPPORT
#define EPROTONOSUPPORT EIO
#endif /* EPROTONOSUPPORT */
#ifndef ESOCKTNOSUPPORT
#define ESOCKTNOSUPPORT EIO
#endif /* ESOCKTNOSUPPORT */
#ifndef EOPNOTSUPP
#define EOPNOTSUPP EIO
#endif /* EOPNOTSUPP */
#ifndef EPFNOSUPPORT
#define EPFNOSUPPORT EIO
#endif /* EPFNOSUPPORT */
#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT EIO
#endif /* EAFNOSUPPORT */
#ifndef EADDRINUSE
#define EADDRINUSE EIO
#endif /* EADDRINUSE */
#ifndef EADDRNOTAVAIL
#define EADDRNOTAVAIL EIO
#endif /* EADDRNOTAVAIL */
#ifndef ENETDOWN
#define ENETDOWN EIO
#endif /* ENETDOWN */
#ifndef ENETUNREACH
#define ENETUNREACH EIO
#endif /* ENETUNREACH */
#ifndef ENETRESET
#define ENETRESET EIO
#endif /* ENETRESET */
#ifndef ECONNABORTED
#define ECONNABORTED EIO
#endif /* ECONNABORTED */
#ifndef ECONNRESET
#define ECONNRESET EIO
#endif /* ECONNRESET */
#ifndef ENOBUFS
#define ENOBUFS EIO
#endif /* ENOBUFS */
#ifndef EISCONN
#define EISCONN EIO
#endif /* EISCONN */
#ifndef ENOTCONN
#define ENOTCONN EIO
#endif /* ENOTCONN */
#ifndef ESHUTDOWN
#define ESHUTDOWN EIO
#endif /* ESHUTDOWN */
#ifndef ETOOMANYREFS
#define ETOOMANYREFS EIO
#endif /* ETOOMANYREFS */
#ifndef ETIMEDOUT
#define ETIMEDOUT EIO
#endif /* ETIMEDOUT */
#ifndef ECONNREFUSED
#define ECONNREFUSED EIO
#endif /* ECONNREFUSED */
#ifndef EHOSTDOWN
#define EHOSTDOWN EIO
#endif /* EHOSTDOWN */
#ifndef EHOSTUNREACH
#define EHOSTUNREACH EIO
#endif /* EHOSTUNREACH */
#ifndef EALREADY
#define EALREADY EIO
#endif /* EALREADY */
#ifndef EINPROGRESS
#define EINPROGRESS EIO
#endif /* EINPROGRESS */
#ifndef ESTALE
#define ESTALE EIO
#endif /* ESTALE */
#ifndef EUCLEAN
#define EUCLEAN EIO
#endif /* EUCLEAN */
#ifndef ENOTNAM
#define ENOTNAM EIO
#endif /* ENOTNAM */
#ifndef ENAVAIL
#define ENAVAIL EIO
#endif /* ENAVAIL */
#ifndef EISNAM
#define EISNAM EIO
#endif /* EISNAM */
#ifndef EREMOTEIO
#define EREMOTEIO EIO
#endif /* EREMOTEIO */
#ifndef ENOMEDIUM
#define ENOMEDIUM EIO
#endif /* ENOMEDIUM */
#ifndef EMEDIUMTYPE
#define EMEDIUMTYPE EIO
#endif /* EMEDIUMTYPE */
