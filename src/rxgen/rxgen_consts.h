#ifndef	_RXGEN_CONSTS_
#define	_RXGEN_CONSTS_

/* These are some rxgen-based (really arbitrary) error codes... */
#define	RXGEN_SUCCESS	    0
#define	RXGEN_CC_MARSHAL    -450
#define	RXGEN_CC_UNMARSHAL  -451
#define	RXGEN_SS_MARSHAL    -452
#define	RXGEN_SS_UNMARSHAL  -453
#define	RXGEN_DECODE	    -454
#define	RXGEN_OPCODE	    -455
#define	RXGEN_SS_XDRFREE    -456
#define	RXGEN_CC_XDRFREE    -457

/* These are some rxgen defines */
#define	RXGEN_MAKECALL_TIMEOUT	0
#define	RXGEN_WAIT		1
#define	RXGEN_DONTWAIT		0

#define MAX_PACKAGES    10
#define MAX_FUNCTION_NAME_LEN 200
#define MAX_FUNCTIONS_PER_PACKAGE 500
#define MAX_FUNCTIONS_PER_INTERFACE (MAX_FUNCTIONS_PER_PACKAGE * MAX_PACKAGES)

#ifndef AFS_SGI61_ENV
/* SGI 6.1 and up do include xdr_char */
#ifdef KERNEL
/* kernel's xdr.h doesn't normally define these things, but we need them */
/* some environments turn these into macros which don't compile... */
#ifndef xdr_char
extern bool_t	xdr_char();
#endif /* xdr_char */

#ifndef xdr_u_char
extern bool_t	xdr_u_char();
#endif /* xdr_u_char */
#endif /* KERNEL */
#endif /* AFS_SGI61_ENV */
#endif /* _RXGEN_CONSTS_ */
