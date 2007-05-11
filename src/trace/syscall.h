/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_TRACE_SYSCALL_H
#define _OSI_TRACE_SYSCALL_H 1



/*
 * osi_trace syscall support
 */

/*
 * syscall opcode table
 */

/* no-op syscall
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_NULL)
 *
 * returns:
 *   0 when syscall mux is loaded
 */
#define OSI_TRACE_SYSCALL_OP_NULL 0

/* 
 * read an entry out of the table 
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_READ, p1, p2, p3)
 *
 * returns:
 *   1 when a record is returned
 *   0 when no record is returned
 *   negative value when an error is encountered
 *
 * [INOUT] osi_trace_cursor_t *    p1 -- pointer to trace table cursor structure
 * [OUT]   osi_TracePoint_record * p2 -- pointer to trace record structure
 * [IN]    osi_uint32              p3 -- flags (defined below)
 */
#define OSI_TRACE_SYSCALL_OP_READ 1

/*
 * flags for OSI_TRACE_SYSCALL_OP_READ
 */
/* don't block in the kernel waiting for new trace records */
#define OSI_TRACE_SYSCALL_OP_READ_FLAG_NOWAIT  0x1


/* 
 * read several entries from the table 
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_READV, p1, p2, p3)
 *
 * returns:
 *   positive value indicating number of records returned
 *   0 when no record is returned
 *   negative value when an error is encountered
 *
 * [INOUT] osi_trace_cursor_t *    p1 -- pointer to trace table cursor structure
 * [IN]    osi_TracePoint_rvec_t * p2 -- pointer to trace record structure
 * [IN]    osi_uint32              p3 -- flags (defined below)
 */
#define OSI_TRACE_SYSCALL_OP_READV 2

/*
 * flags for OSI_TRACE_SYSCALL_OP_READV
 */
/* don't block in the kernel waiting for new trace records */
#define OSI_TRACE_SYSCALL_OP_READV_FLAG_NOWAIT  0x1


/*
 * enable all tracepoints matching a filter expression
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_ENABLE, p1, 0, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN]  char       * p1 -- pointer to probe name filter buffer
 * [OUT] osi_uint32 * p2 -- address in which to store hit count
 */
#define OSI_TRACE_SYSCALL_OP_ENABLE 3


/*
 * disable all tracepoints matching a filter expression
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_DISABLE, p1, 0, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN]  char       * p1 -- pointer to probe name filter buffer
 * [OUT] osi_uint32 * p2 -- address in which to store hit count
 */
#define OSI_TRACE_SYSCALL_OP_DISABLE 4


/* 
 * enable a tracepoint by probe id
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_ENABLE_BY_ID, p1, 0, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN] osi_trace_probe_id_t p1 -- probe id
 */
#define OSI_TRACE_SYSCALL_OP_ENABLE_BY_ID 5


/* 
 * disable a tracepoint by probe id
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_DISABLE_BY_ID, p1, 0, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN] osi_trace_probe_id_t p1 -- probe id
 */
#define OSI_TRACE_SYSCALL_OP_DISABLE_BY_ID 6


/* 
 * insert a new trace record into the kernel trace buffer
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_INSERT, p1, 0, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN] osi_TracePoint_record * p1 -- pointer to trace record structure
 */
#define OSI_TRACE_SYSCALL_OP_INSERT 7


/* 
 * lookup a probe id given its name 
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_LOOKUP_N2I, p1, p2, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN]  char *                 p1 -- pointer to probe name buffer
 * [OUT] osi_trace_probe_id_t * p2 -- probe id
 */
#define OSI_TRACE_SYSCALL_OP_LOOKUP_N2I 8


/* 
 * lookup a probe name given its id 
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_LOOKUP_I2N, p1, p2, p3)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN]  osi_trace_probe_id_t p1 -- probe id
 * [OUT] char *               p2 -- pointer to probe name buffer
 * [IN]  size_t               p3 -- size of probe name buffer
 */
#define OSI_TRACE_SYSCALL_OP_LOOKUP_I2N 9


/* 
 * get kernel trace configuration 
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_CONFIG_GET, p1, p2, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [OUT] osi_trace_syscall_config_t * p1 -- pointer to config struct
 */
#define OSI_TRACE_SYSCALL_OP_CONFIG_GET 10

typedef enum {
    OSI_TRACE_KERNEL_CONFIG_TERMINAL,
    OSI_TRACE_KERNEL_CONFIG_SIZEOF_REGISTER,
    OSI_TRACE_KERNEL_CONFIG_SIZEOF_FAST,
    OSI_TRACE_KERNEL_CONFIG_SIZEOF_LARGE,
    OSI_TRACE_KERNEL_CONFIG_SIZEOF_RECORD,
    OSI_TRACE_KERNEL_CONFIG_DATAMODEL,
    OSI_TRACE_KERNEL_CONFIG_TRACE_VERSION,
    OSI_TRACE_KERNEL_CONFIG_CTX_LOCAL_TRACE,
    OSI_TRACE_KERNEL_CONFIG_BUFFER_LEN,
    OSI_TRACE_KERNEL_CONFIG_GEN_RGY_MAX_ID,
    OSI_TRACE_KERNEL_CONFIG_MAX_ID
} osi_trace_syscall_config_key_t;

typedef struct {
    osi_uint32 key;
    osi_uint32 value;
} osi_trace_syscall_config_element_t;

typedef struct {
    osi_trace_syscall_config_element_t * buf;
    osi_uint32 nelements;
} osi_trace_syscall_config_t;

typedef struct {
    osi_uint32 buf;
    osi_uint32 nelements;
} osi_trace_syscall32_config_t;

/* 
 * register a new generator
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_REGISTER, p1, p2, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN]  osi_trace_generator_address_t * p1 -- address of generator being registered
 * [OUT] osi_trace_gen_id_t *            p2 -- id of registered generator
 */
#define OSI_TRACE_SYSCALL_OP_GEN_REGISTER 11


/* 
 * unregister a generator
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_UNREGISTER, p1, 0, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN] osi_trace_gen_id_t p1 -- generator id being unregistered
 */
#define OSI_TRACE_SYSCALL_OP_GEN_UNREGISTER 12


/*
 * lookup a generator by id
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_LOOKUP_I2A, p1, p2, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN] osi_trace_gen_id_t               p1 -- generator id being queried
 * [OUT] osi_trace_generator_address_t * p2 -- address associated with gen id p1
 */
#define OSI_TRACE_SYSCALL_OP_GEN_LOOKUP_I2A 13


/*
 * lookup a generator by address
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_LOOKUP_A2I, p1, p2, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN] osi_trace_generator_address_t  * p1 -- generator address being queried
 * [OUT] osi_trace_gen_id_t            * p2 -- generator id associated with p1
 */
#define OSI_TRACE_SYSCALL_OP_GEN_LOOKUP_A2I 14


/*
 * get a generator handle by id
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_GET, p1, 0, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN] osi_trace_gen_id_t p1 -- generator id
 */
#define OSI_TRACE_SYSCALL_OP_GEN_GET 15


/*
 * get a generator handle by address
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_GET_BY_ADDR, p1, p2, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN] osi_trace_generator_address_t  * p1 -- generator address
 * [OUT] osi_trace_gen_id_t            * p2 -- generator id associated with p1
 */
#define OSI_TRACE_SYSCALL_OP_GEN_GET_BY_ADDR 16


/*
 * put a generator handle
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_GEN_PUT, p1, 0, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN] osi_trace_gen_id_t p1 -- generator id
 */
#define OSI_TRACE_SYSCALL_OP_GEN_PUT 17


/*
 * send a message
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MAIL_SEND, p1, 0, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN] osi_trace_mail_message_t * p1 -- message to send
 */
#define OSI_TRACE_SYSCALL_OP_MAIL_SEND 18


/*
 * send a vector of messages
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MAIL_SENDV, p1, 0, 0)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN] osi_trace_mail_mvec_t * p1 -- vector of messages to send
 */
#define OSI_TRACE_SYSCALL_OP_MAIL_SENDV 19


/*
 * receive a vector of messages
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MAIL_CHECK, p1, p2, p3)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN]  osi_trace_gen_id_t         p1 -- our gen_rgy id
 * [OUT] osi_trace_mail_message_t * p2 -- message buffer to receive into
 * [IN]  osi_uint32                 p3 -- flags
 */
#define OSI_TRACE_SYSCALL_OP_MAIL_CHECK 20

/*
 * flags for OSI_TRACE_SYSCALL_OP_MAIL_CHECK
 */
#define OSI_TRACE_SYSCALL_OP_MAIL_CHECK_FLAG_NOWAIT 0x1

/*
 * receive a vector of messages
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MAIL_CHECKV, p1, p2, p3)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN]    osi_trace_gen_id_t      p1 -- our gen_rgy id
 * [INOUT] osi_trace_mail_mvec_t * p2 -- vector of message buffers to receive into
 * [IN]    osi_uint32              p3 -- flags
 */
#define OSI_TRACE_SYSCALL_OP_MAIL_CHECKV 21

/*
 * flags for OSI_TRACE_SYSCALL_OP_MAIL_CHECKV
 * NOTE: all flags for OSI_TRACE_SYSCALL_OP_MAIL_CHECK are inherited
 */

/*
 * query cm state
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_QUERY_STATE, p1, p2, p3)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN]  osi_trace_query_id_t  p1 -- query to execute
 * [OUT] void *                p2 -- opaque buffer; used to pass in query args and pass out query results
 * [IN]  size_t                p3 -- opaque buffer len
 */
#define OSI_TRACE_SYSCALL_OP_QUERY_STATE 22

/*
 * get trace module registry stats
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MODULE_INFO, p1)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [OUT] osi_trace_generator_info_t *  p1 -- gen info structure pointer
 */
#define OSI_TRACE_SYSCALL_OP_MODULE_INFO 23

/*
 * lookup trace module by name
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MODULE_LOOKUP, p1, p2)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN]  osi_trace_module_id_t      p1 -- module id to lookup
 * [OUT] osi_trace_module_info_t *  p2 -- pointer to module info structure
 */
#define OSI_TRACE_SYSCALL_OP_MODULE_LOOKUP 24

/*
 * lookup trace module by name
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MODULE_LOOKUP_BY_NAME, p1, p2)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN]  char *                     p1 -- module name to lookup
 * [OUT] osi_trace_module_info_t *  p2 -- pointer to module info structure
 */
#define OSI_TRACE_SYSCALL_OP_MODULE_LOOKUP_BY_NAME 25

/*
 * lookup trace module by name
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MODULE_LOOKUP_BY_PREFIX, p1, p2)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN]  char *                     p1 -- module prefix to lookup
 * [OUT] osi_trace_module_info_t *  p2 -- pointer to module info structure
 */
#define OSI_TRACE_SYSCALL_OP_MODULE_LOOKUP_BY_PREFIX 26

/*
 * mail tap create
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MAIL_TAP, p1, p2)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN] osi_trace_gen_id_t p1  -- gen id which will have its mailbox tapped
 * [IN] osi_trace_gen_id_t p2  -- gen id into which we'll dump data
 */
#define OSI_TRACE_SYSCALL_OP_MAIL_TAP 27

/*
 * mail tap destroy
 *
 * osi_Trace_syscall(OSI_TRACE_SYSCALL_OP_MAIL_TAP, p1, p2)
 *
 * returns:
 *   0 on success; nonzero on error
 *
 * [IN] osi_trace_gen_id_t p1  -- gen id which will have its mailbox tapped
 * [IN] osi_trace_gen_id_t p2  -- gen id into which we'll dump data
 */
#define OSI_TRACE_SYSCALL_OP_MAIL_UNTAP 28


#if defined(OSI_ENV_KERNELSPACE)
#include <trace/KERNEL/syscall.h>
#else
#include <trace/USERSPACE/syscall.h>
#endif

#ifdef UKERNEL
#include <trace/UKERNEL/syscall.h>
#endif


osi_extern osi_result osi_trace_syscall_PkgInit(void);
osi_extern osi_result osi_trace_syscall_PkgShutdown(void);

#endif /* _OSI_TRACE_SYSCALL_H */
