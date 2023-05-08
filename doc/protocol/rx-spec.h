/*!
 * \addtogroup rxrpc-spec RX RPC Specification
 * RX RPC Specification
 * @{
 *
 * \mainpage AFS-3 Programmer's Reference: Specification for the Rx Remote
 * Procedure Call Facility
 *
 * AFS-3 Programmer's Reference:  
 * 
 * Specification for the Rx Remote Procedure Call Facility 
 * \author Edward R. Zayas 
 * Transarc Corporation 
 * \version 1.2
 * \date 28 August 1991 10:11 .cCopyright 1991 Transarc Corporation All Rights
 * Reserved FS-00-D164 
 * 
 * 	\page chap1 Chapter 1 -- Overview of the Rx RPC system                                
 *                                                                                 
 * 	\section sec1-1 Section 1.1: Introduction to Rx                                      
 * 
 * \par 
 * The Rx package provides a high-performance, multi-threaded, and secure
 * mechanism by which 
 * remote procedure calls (RPCs) may be performed between programs executing
 * anywhere in a 
 * network of computers. The Rx protocol is adaptive, conforming itself to
 * widely varying 
 * network communication media. It allows user applications to define and
 * insert their own 
 * security modules, allowing them to execute the precise end-to-end
 * authentication algorithms 
 * required to suit their needs and goals. Although pervasive throughout the
 * AFS distributed 
 * file system, all of its agents, and many of its standard application
 * programs, Rx is entirely 
 * separable from AFS and does not depend on any of its features. In fact, Rx
 * can be used to build applications engaging in RPC-style communication under
 * a variety of unix-style file systems. There are in-kernel and user-space
 * implementations of the Rx facility, with both sharing the same interface.
 * \par 
 * This document provides a comprehensive and detailed treatment of the Rx RPC
 * package. 
 * 
 * 	\section sec1-2 Section 1.2: Basic Concepts 
 * 
 * \par 
 * The Rx design operates on the set of basic concepts described in this
 * section. 
 * 
 * 	\subsection sec1-2-1 Section 1.2.1: Security
 * 
 * \par 
 * The Rx architecture provides for tight integration between the RPC mechanism
 * and methods for making this communication medium secure. As elaborated in
 * Section 5.3.1.3 and illustrated by the built-in rxkad security system
 * described in Chapter 3, Rx defines the format for a generic security module,
 * and then allows application programmers to define and activate
 * instantiations of these modules. Rx itself knows nothing about the internal
 * details of any particular security model, or the module-specific state it
 * requires. It does, however, know when to call the generic security
 * operations, and so can easily execute the security algorithm defined. Rx
 * does maintain basic state per connection on behalf of any given security
 * class. 
 * 
 * 	\subsection sec1-2-2 Section 1.2.2: Services 
 * 
 * \par 
 * An Rx-based server exports services, or specific RPC interfaces that
 * accomplish certain tasks. Services are identified by (host-address,
 * UDP-port, serviceID) triples. An Rx service is installed and initialized on
 * a given host through the use of the rx NewService() routine (See Section
 * 5.6.3). Incoming calls are stamped with the Rx service type, and must match
 * an installed service to be accepted. Internally, Rx services also carry
 * string names which identify them, which is useful for remote debugging and
 * statistics-gathering programs. The use of a service ID allows a single
 * server process to export multiple, independently-specified Rx RPC services. 
 * \par 
 * Each Rx service contains one or more security classes, as implemented by
 * individual security objects. These security objects implement end-to-end
 * security protocols. Individual peer-to-peer connections established on
 * behalf of an Rx service will select exactly one of the supported security
 * objects to define the authentication procedures followed by all calls
 * associated with the connection. Applications are not limited to using only
 * the core set of built-in security objects offered by Rx. They are free to
 * define their own security objects in order to execute the specific protocols
 * they require. 
 * \par 
 * It is possible to specify both the minimum and maximum number of lightweight
 * processes available to handle simultaneous calls directed to an Rx service.
 * In addition, certain procedures may be registered with the service and
 * called at specific times in the course of handling an RPC request. 
 * 
 * 	\subsection sec1-2-3 Section 1.2.3: Connections 
 * 
 * \par 
 * An Rx connection represents an authenticated communication path, allowing a
 * sequence of multiple asynchronous conversations (calls). Each connection is
 * identified by a connection ID. The low-order bits of the connection ID are
 * reserved so that they may be stamped with the index of a particular call
 * channel. With up to RX MAXCALLS concurrent calls (set to 4 in this
 * implementation), the bottom two bits are set aside for this purpose. The
 * connection ID is not sufficient to uniquely identify an Rx connection by
 * itself. Should a client crash and restart, it may reuse a connection ID,
 * causing inconsistent results. Included with the connection ID is the epoch,
 * or start time for the client side of the connection. After a crash, the next
 * incarnation of the client will choose a different epoch value. This will
 * differentiate the new incarnation from the orphaned connection record on the
 * server side. 
 * \par 
 * Each connection is associated with a parent service, which defines a set of
 * supported security models. At creation time, an Rx connection selects the
 * particular security protocol it will implement, referencing the associated
 * service. The connection structure maintains state for each individual call
 * simultaneously handled. 
 * 
 * 	\subsection sec1-2-4 Section 1.2.4: Peers 
 * 
 * \par 
 * For each connection, Rx maintains information describing the entity, or
 * peer, on the other side of the wire. A peer is identified by a (host,
 * UDP-port) pair, with an IP address used to identify the host. Included in
 * the information kept on this remote communication endpoint are such network
 * parameters as the maximum packet size supported by the host, current
 * readings on round trip time and retransmission delays, and packet skew (see
 * Section 1.2.7). There are also congestion control fields, including
 * retransmission statistics and descriptions of the maximum number of packets
 * that may be sent to the peer without pausing. Peer structures are shared
 * between connections whenever possible, and, hence, are reference-counted. A
 * peer object may be garbage-collected if it is not actively referenced by any
 * connection structure and a sufficient period of time has lapsed since the
 * reference count dropped to zero. 
 * 
 * 	\subsection sec1-2-5 Section 1.2.5: Calls 
 * 
 * \par 
 * An Rx call represents an individual RPC being executed on a given
 * connection. As described above, each connection may have up to RX MAXCALLS
 * calls active at any one instant. The information contained in each call
 * structure is specific to the given call. 
 * \par 
 * "Permanent" call state, such as the call number, is maintained in the
 * connection structure itself. 
 * 
 * 	\subsection sec1-2-6 Section 1.2.6: Quotas 
 * 
 * \par 
 * Each attached server thread must be able to make progress to avoid system
 * deadlock. The Rx facility ensures that it can always handle the arrival of
 * the next unacknowledged data packet for an attached call with its system of
 * packet quotas. A certain number of packets are reserved per server thread
 * for this purpose, allowing the server threads to queue up an entire window
 * full of data for an active call and still have packet buffers left over to
 * be able to read its input without blocking. 
 * 
 * 	\subsection sec1-2-7 Section 1.2.7: Packet Skew 
 * 
 * \par 
 * If a packet is received n packets later than expected (based on packet
 * serial numbers), then we define it to have a skew of n. The maximum skew
 * values allow us to decide when a packet hasn't been received yet because it
 * is out of order, as opposed to when it is likely to have been dropped. 
 * 
 * 	\subsection sec1-2-8 Section 1.2.8: Multicasting 
 * 
 * \par 
 * The rx multi.c module provides for multicast abilities, sending an RPC to
 * several targets simultaneously. While true multicasting is not achieved, it
 * is simulated by a rapid succession of packet transmissions and a collection
 * algorithm for the replies. A client program, though, may be programmed as if
 * multicasting were truly taking place. Thus, Rx is poised to take full
 * advantage of a system supporting true multicasting with minimal disruption
 * to the existing client code base. 
 * 
 * 	\section sec1-3 Section 1.3: Scope 
 * 
 * \par 
 * This paper is a member of a documentation suite providing specifications as
 * to the operation and interfaces offered by the various AFS servers and
 * agents. Rx is an integral part of the AFS environment, as it provides the
 * high-performance, secure pathway by which these system components
 * communicate across the network. Although AFS is dependent on Rx's services,
 * the reverse is not true. Rx is a fully independent RPC package, standing on
 * its own and usable in other environments. 
 * \par 
 * The intent of this work is to provide readers with a sufficiently detailed
 * description of Rx that they may proceed to write their own applications on
 * top of it. In fact, code for a sample Rx server and client are provided. 
 * \par 
 * One topic related to Rx will not be covered by this document, namely the
 * Rxgen stub generator. Rather, rxgen is addressed in a separate document. 
 * 
 * 	\section sec1-4 Section 1.4: Document Layout 
 * 
 * \par 
 * After this introduction, Chapter 2 will introduce and describe various
 * facilities and tools that support Rx. In particular, the threading and
 * locking packages used by Rx will be examined, along with a set of timer and
 * preemption tools. Chapter 3 proceeds to examine the details of one of the
 * built-in security modules offered by Rx. Based on the Kerberos system
 * developed by MIT's Project Athena, this rxkad module allows secure, ecrypted
 * communication between the server and client ends of the RPC. Chapter 5 then
 * provides the full Rx programming interface, and Chapter 6 illustrates the
 * use of this programming interface by providing a fully-operational
 * programming example employing Rx. This rxdemo suite is examined in detail,
 * ranging all the way from a step-by-step analysis of the human-authored
 * files, and the Rxgen-generated files upon which they are based, to the
 * workings of the associated Makefile. Output from the example rxdemo server
 * and client is also provided. 
 * 
 * 	\section sec1-5 Section 1.5: Related Documents 
 * 
 * \par 
 * Titles for the full suite of AFS specification documents are listed below.
 * All of the servers and agents making up the AFS computing environment,
 * whether running in the unix kernel or in user space, utilize an Rx RPC
 * interface through which they export their services. 
 * \par
 * \li 	AFS-3 Programmer's Reference: Architectural Overview: This paper
 * provides an architectual overview of the AFS distributed file system,
 * describing the full set of servers and agents in a coherent way,
 * illustrating their relationships to each other and examining their
 * interactions. 
 * \li 	AFS-3 Programmer's Reference: file Server/Cache Manager Interface: This
 * document describes the workings and interfaces of the two primary AFS
 * agents, the file Server and Cache Manager. The file Server provides a
 * centralized disk repository for sets of files, regulating access to them.
 * End users sitting on client machines rely on the Cache Manager agent,
 * running in their kernel, to act as their agent in accessing the data stored
 * on file Server machines, making those files appear as if they were really
 * housed locally. 
 * \li 	AFS-3 Programmer's Reference:Volume Server/Volume Location Server
 * Interface: This document describes the services through which "containers"
 * of related user data are located and managed. 
 * \li 	AFS-3 Programmer's Reference: Protection Server Interface: This paper
 * describes the server responsible for mapping printable user names to and
 * from their internal AFS identifiers. The Protection Server also allows users
 * to create, destroy, and manipulate "groups" of users, which are suitable for
 * placement on access control lists (ACLs). 
 * \li 	AFS-3 Programmer's Reference: BOS Server Interface: This paper
 * explicates the "nanny" service which assists in the administrability of the
 * AFS environment. 
 * \par 
 * In addition to these papers, the AFS 3.1 product is delivered with its own
 * user, system administrator, installation, and command reference documents. 
 * 
 * 	\page chap2 Chapter 2 -- The LWP Lightweight Process Package 
 * 
 * 	\section sec2-1 Section 2.1: Introduction 
 * \par 
 * This chapter describes a package allowing multiple threads of control to
 * coexist and cooperate within one unix process. Each such thread of control
 * is also referred to as a lightweight process, in contrast to the traditional
 * unix (heavyweight) process. Except for the limitations of a fixed stack size
 * and non-preemptive scheduling, these lightweight processes possess all the
 * properties usually associated with full-fledged processes in typical
 * operating systems. For the purposes of this document, the terms lightweight
 * process, LWP, and thread are completely interchangeable, and they appear
 * intermixed in this chapter. Included in this lightweight process facility
 * are various sub-packages, including services for locking, I/O control,
 * timers, fast time determination, and preemption. 
 * \par 
 * The Rx facility is not the only client of the LWP package. Other LWP clients
 * within AFS include the file Server, Protection Server, BOS Server, Volume
 * Server, Volume Location Server, and the Authentication Server, along with
 * many of the AFS application programs. 
 * 
 * 	\section sec2-2 Section 2.2: Description 
 * 
 * 	\subsection Section 2.2.1: sec2-2-1 LWP Overview 
 * 
 * \par 
 * The LWP package implements primitive functions that provide the basic
 * facilities required to enable procedures written in C to execute
 * concurrently and asynchronously. The LWP package is meant to be
 * general-purpose (note the applications mentioned above), with a heavy
 * emphasis on simplicity. Interprocess communication facilities can be built
 * on top of this basic mechanism and in fact, many different IPC mechanisms
 * could be implemented. 
 * \par 
 * In order to set up the threading support environment, a one-time invocation
 * of the LWP InitializeProcessSupport() function must precede the use of the
 * facilities described here. This initialization function carves an initial
 * process out of the currently executing C procedure and returns its thread
 * ID. For symmetry, an LWP TerminateProcessSupport() function may be used
 * explicitly to release any storage allocated by its counterpart. If this
 * function is used, it must be issued from the thread created by the original
 * LWP InitializeProcessSupport() invocation. 
 * \par 
 * When any of the lightweight process functions completes, an integer value is
 * returned to indicate whether an error condition was encountered. By
 * convention, a return value of zero indicates that the operation succeeded. 
 * \par 
 * Macros, typedefs, and manifest constants for error codes needed by the
 * threading mechanism are exported by the lwp.h include file. A lightweight
 * process is identified by an object of type PROCESS, which is defined in the
 * include file. 
 * \par 
 * The process model supported by the LWP operations is based on a
 * non-preemptive priority dispatching scheme. A priority is an integer in the
 * range [0..LWP MAX PRIORITY], where 0 is the lowest priority. Once a given
 * thread is selected and dispatched, it remains in control until it
 * voluntarily relinquishes its claim on the CPU. Control may be relinquished
 * by either explicit means (LWP_DispatchProcess()) or implicit means (through
 * the use of certain other LWP operations with this side effect). In general,
 * all LWP operations that may cause a higher-priority process to become ready
 * for dispatching preempt the process requesting the service. When this
 * occurs, the dispatcher mechanism takes over and automatically schedules the
 * highest-priority runnable process. Routines in this category, where the
 * scheduler is guaranteed to be invoked in the absence of errors, are: 
 * \li LWP_WaitProcess() 
 * \li LWP_MwaitProcess() 
 * \li LWP_SignalProcess() 
 * \li LWP_DispatchProcess() 
 * \li LWP_DestroyProcess() 
 * \par 
 * The following functions are guaranteed not to cause preemption, and so may
 * be issued with no fear of losing control to another thread: 
 * \li LWP_InitializeProcessSupport() 
 * \li LWP_NoYieldSignal() 
 * \li LWP_CurrentProcess() 
 * \li LWP_ActiveProcess() 
 * \li LWP_StackUsed() 
 * \li LWP_NewRock() 
 * \li LWP_GetRock() 
 * \par 
 * The symbol LWP NORMAL PRIORITY, whose value is (LWP MAX PRIORITY-2),
 * provides a reasonable default value to use for process priorities. 
 * \par 
 * The lwp debug global variable can be set to activate or deactivate debugging
 * messages tracing the flow of control within the LWP routines. To activate
 * debugging messages, set lwp debug to a non-zero value. To deactivate, reset
 * it to zero. All debugging output from the LWP routines is sent to stdout. 
 * \par 
 * The LWP package checks for stack overflows at each context switch. The
 * variable that controls the action of the package when an overflow occurs is
 * lwp overflowAction. If it is set to LWP SOMESSAGE, then a message will be
 * printed on stderr announcing the overflow. If lwp overflowAction is set to
 * LWP SOABORT, the abort() LWP routine will be called. finally, if lwp
 * overflowAction is set to LWP SOQUIET, the LWP facility will ignore the
 * errors. By default, the LWP SOABORT setting is used. 
 * \par 
 * Here is a sketch of a simple program (using some psuedocode) demonstrating
 * the high-level use of the LWP facility. The opening #include line brings in
 * the exported LWP definitions. Following this, a routine is defined to wait
 * on a "queue" object until something is deposited in it, calling the
 * scheduler as soon as something arrives. Please note that various LWP
 * routines are introduced here. Their definitions will appear later, in
 * Section 2.3.1. 
 * 
 * \code
 * #include <afs/lwp.h> 
 * static read_process(id) 
 * int *id; 
 * {  /* Just relinquish control for now */
 * 	LWP_DispatchProcess(); 
 * 	for 	(;;) 
 * 	{  
 * 		/* Wait until there is something in the queue */
 * 		while 	(empty(q)) LWP_WaitProcess(q); 
 * 		/* Process the newly-arrived queue entry */
 * 		LWP_DispatchProcess(); 
 * 	} 
 * } 
 * \endcode
 * 
 * \par
 * The next routine, write process(), sits in a loop, putting messages on the
 * shared queue and signalling the reader, which is waiting for activity on the
 * queue. Signalling a thread is accomplished via the LWP SignalProcess()
 * library routine. 
 * 
 * \code
 * static write_process() 
 * { ... 
 * 	/* Loop, writing data to the shared queue.  */
 * 	for 	(mesg = messages; *mesg != 0; mesg++) 
 * 	{ 
 * 		insert(q, *mesg); 
 * 		LWP_SignalProcess(q); 
 * 	} 
 * } 
 * \endcode
 * 
 * \par
 * finally, here is the main routine for this demo pseudocode. It starts by
 * calling the LWP initialization routine. Next, it creates some number of
 * reader threads with calls to LWP CreateProcess() in addition to the single
 * writer thread. When all threads terminate, they will signal the main routine
 * on the done variable. Once signalled, the main routine will reap all the
 * threads with the help of the LWP DestroyProcess() function. 
 * 
 * \code
 * main(argc, argv) 
 * int argc; 
 * char **argv; 
 * { 
 * 	PROCESS *id;  /* Initial thread ID */
 * 	/* Set up the LWP package, create the initial thread ID. */
 * 	LWP_InitializeProcessSupport(0, &id); 
 * 	/* Create a set of reader threads.  */
 * 	for (i = 0; i < nreaders; i++) 
 * 		LWP_CreateProcess(read_process, STACK_SIZE, 0, i, "Reader",
 * 		&readers[i]); 
 * 
 * 	/* Create a single writer thread.  */
 * 	LWP_CreateProcess(write_process, STACK_SIZE, 1, 0, "Writer", &writer); 
 * 	/* Wait for all the above threads to terminate.  */
 * 	for (i = 0; i <= nreaders; i++) 
 * 		LWP_WaitProcess(&done); 
 * 
 * 	/* All threads are done. Destroy them all.  */
 * 	for (i = nreaders-1; i >= 0; i--) 
 * 		LWP_DestroyProcess(readers[i]); 
 * } 
 * \endcode
 * 
 * 	\subsection sec2-2-2 Section 2.2.2: Locking 
 * \par
 * The LWP locking facility exports a number of routines and macros that allow
 * a C programmer using LWP threading to place read and write locks on shared
 * data structures.  This locking facility was also written with simplicity in
 * mind. 
 * \par
 * In order to invoke the locking mechanism, an object of type struct Lock must
 * be associated with the object. After being initialized with a call to
 * LockInit(), the lock object is used in invocations of various macros,
 * including ObtainReadLock(), ObtainWriteLock(), ReleaseReadLock(),
 * ReleaseWriteLock(), ObtainSharedLock(), ReleaseSharedLock(), and
 * BoostSharedLock(). 
 * \par
 * Lock semantics specify that any number of readers may hold a lock in the
 * absence of a writer. Only a single writer may acquire a lock at any given
 * time. The lock package guarantees fairness, legislating that each reader and
 * writer will eventually obtain a given lock. However, this fairness is only
 * guaranteed if the priorities of the competing processes are identical. Note
 * that ordering is not guaranteed by this package. 
 * \par
 * Shared locks are read locks that can be "boosted" into write locks. These
 * shared locks have an unusual locking matrix. Unboosted shared locks are
 * compatible with read locks, yet incompatible with write locks and other
 * shared locks. In essence, a thread holding a shared lock on an object has
 * effectively read-locked it, and has the option to promote it to a write lock
 * without allowing any other writer to enter the critical region during the
 * boost operation itself. 
 * \par
 * It is illegal for a process to request a particular lock more than once
 * without first releasing it. Failure to obey this restriction will cause
 * deadlock. This restriction is not enforced by the LWP code. 
 * \par
 * Here is a simple pseudocode fragment serving as an example of the available
 * locking operations. It defines a struct Vnode object, which contains a lock
 * object. The get vnode() routine will look up a struct Vnode object by name,
 * and then either read-lock or write-lock it. 
 * \par
 * As with the high-level LWP example above, the locking routines introduced
 * here will be fully defined later, in Section 2.3.2. 
 * 
 * \code
 * #include <afs/lock.h> 
 * 
 * struct Vnode { 
 * 	... 
 * 	struct Lock lock;  Used to lock this vnode  
 * ... }; 
 * 
 * #define READ 0 
 * #define WRITE 1 
 * 
 * struct Vnode *get_vnode(name, how) char *name; 
 * int how; 
 * { 
 * 	struct Vnode *v; 
 * 	v = lookup(name); 
 * 	if (how == READ) 
 * 		ObtainReadLock(&v->lock); 
 * 	else 
 * 		ObtainWriteLock(&v->lock); 
 * } 
 * \endcode
 * 
 * 
 * 	\subsection sec2-2-3 Section 2.2.3: IOMGR 
 * 
 * \par
 * The IOMGR facility associated with the LWP service allows threads to wait on
 * various unix events. The exported IOMGR Select() routine allows a thread to
 * wait on the same set of events as the unix select() call. The parameters to
 * these two routines are identical. IOMGR Select() puts the calling LWP to
 * sleep until no threads are active. At this point, the built-in IOMGR thread,
 * which runs at the lowest priority, wakes up and coalesces all of the select
 * requests together. It then performs a single select() and wakes up all
 * threads affected by the result. 
 * \par
 * The IOMGR Signal() routine allows an LWP to wait on the delivery of a unix
 * signal. The IOMGR thread installs a signal handler to catch all deliveries
 * of the unix signal. This signal handler posts information about the signal
 * delivery to a global data structure. The next time that the IOMGR thread
 * runs, it delivers the signal to any waiting LWP. 
 * \par
 * Here is a pseudocode example of the use of the IOMGR facility, providing the
 * blueprint for an implemention a thread-level socket listener. 
 * 
 * \code
 * void rpc_SocketListener() 
 * { 
 * 	int ReadfdMask, WritefdMask, ExceptfdMask, rc; 
 * 	struct timeval *tvp; 
 * 	while(TRUE) 
 * 	{ ... 
 * 		ExceptfdMask = ReadfdMask = (1 << rpc_RequestSocket); 
 * 		WritefdMask = 0; 
 * 
 * 		rc = IOMGR_Select(8*sizeof(int), &ReadfdMask, &WritefdMask,
 * 		&ExceptfdMask, tvp); 
 * 
 * 		switch(rc) 
 * 		{ 
 * 			case 0: /* Timeout */ continue; 
 * 			/* Main while loop */
 * 
 * 			case -1: /* Error */ 
 * 			SystemError("IOMGR_Select"); 
 * 			exit(-1); 
 * 
 * 			case 1: /* RPC packet arrived! */ ... 
 * 			process packet ... 
 * 			break; 
 * 
 * 			default: Should never occur 
 * 		} 
 * 	} 
 * } 
 * \endcode
 * 
 * 	\subsection sec2-2-4 Section 2.2.4: Timer 
 * \par
 * The timer package exports a number of routines that assist in manipulating
 * lists of objects of type struct TM Elem. These struct TM Elem timers are
 * assigned a timeout value by the user and inserted in a package-maintained
 * list. The time remaining to each timer's timeout is kept up to date by the
 * package under user control. There are routines to remove a timer from its
 * list, to return an expired timer from a list, and to return the next timer
 * to expire. 
 * \par
 * A timer is commonly used by inserting a field of type struct TM Elem into a
 * structure. After setting the desired timeout value, the structure is
 * inserted into a list by means of its timer field. 
 * \par
 * Here is a simple pseudocode example of how the timer package may be used.
 * After calling the package initialization function, TM Init(), the pseudocode
 * spins in a loop. first, it updates all the timers via TM Rescan() calls.
 * Then, it pulls out the first expired timer object with TM GetExpired() (if
 * any), and processes it. 
 * 
 * \code
 * static struct TM_Elem *requests; 
 * ... 
 * TM_Init(&requests); /* Initialize timer list */ ... 
 * for (;;) { 
 * 	TM_Rescan(requests);  /* Update the timers */
 * 	expired = TM_GetExpired(requests); 
 * 	if (expired == 0) 
 * 	break; 
 * 	. . . process expired element . . . 
 * 	} 
 * \endcode
 * 
 * 	\subsection sec2-2-5 Section 2.2.5: Fast Time 
 * 
 * \par
 * The fast time routines allows a caller to determine the current time of day
 * without incurring the expense of a kernel call. It works by mapping the page
 * of the kernel that holds the time-of-day variable and examining it directly.
 * Currently, this package only works on Suns. The routines may be called on
 * other architectures, but they will run more slowly. 
 * \par
 * The initialization routine for this package is fairly expensive, since it
 * does a lookup of a kernel symbol via nlist(). If the client application
 * program only runs for only a short time, it may wish to call FT Init() with
 * the notReally parameter set to TRUE in order to prevent the lookup from
 * taking place. This is useful if you are using another package that uses the
 * fast time facility. 
 * 
 * 	\section sec2-3 Section 2.3: Interface Specifications 
 * 
 * 	\subsection sec2-3-1 Section 2.3.1: LWP 
 * 
 * \par
 * This section covers the calling interfaces to the LWP package. Please note
 * that LWP macros (e.g., ActiveProcess) are also included here, rather than
 * being relegated to a different section. 
 * 
 * 	\subsubsection sec2-3-1-1 Section 2.3.1.1: LWP_InitializeProcessSupport
 * 	_ Initialize the LWP package 
 * 
 * \par
 * int LWP_InitializeProcessSupport(IN int priority; OUT PROCESS *pid) 
 * \par Description 
 * This function initializes the LWP package. In addition, it turns the current
 * thread of control into the initial process with the specified priority. The
 * process ID of this initial thread is returned in the pid parameter. This
 * routine must be called before any other routine in the LWP library. The
 * scheduler will NOT be invoked as a result of calling
 * LWP_InitializeProcessSupport(). 
 * \par Error Codes 
 * LWP EBADPRI The given priority is invalid, either negative or too large. 
 * 
 * 	 \subsubsection sec2-3-1-2 Section 2.3.1.2: LWP_TerminateProcessSupport
 * 	 _ End process support, perform cleanup 
 * 
 * \par
 * int LWP_TerminateProcessSupport() 
 * \par Description 
 * This routine terminates the LWP threading support and cleans up after it by
 * freeing any auxiliary storage used. This routine must be called from within
 * the process that invoked LWP InitializeProcessSupport(). After LWP
 * TerminateProcessSupport() has been called, it is acceptable to call LWP
 * InitializeProcessSupport() again in order to restart LWP process support. 
 * \par Error Codes 
 * ---Always succeeds, or performs an abort(). 
 * 
 * 	\subsubsection sec2-3-1-3 Section 2.3.1.3: LWP_CreateProcess _ Create a
 * 	new thread 
 * 
 * \par
 * int LWP_CreateProcess(IN int (*ep)(); IN int stacksize; IN int priority; IN
 * char *parm; IN char *name; OUT PROCESS *pid) 
 * \par Description 
 * This function is used to create a new lightweight process with a given
 * printable name. The ep argument identifies the function to be used as the
 * body of the thread. The argument to be passed to this function is contained
 * in parm. The new thread's stack size in bytes is specified in stacksize, and
 * its execution priority in priority. The pid parameter is used to return the
 * process ID of the new thread. 
 * \par
 * If the thread is successfully created, it will be marked as runnable. The
 * scheduler is called before the LWP CreateProcess() call completes, so the
 * new thread may indeed begin its execution before the completion. Note that
 * the new thread is guaranteed NOT to run before the call completes if the
 * specified priority is lower than the caller's. On the other hand, if the new
 * thread's priority is higher than the caller's, then it is guaranteed to run
 * before the creation call completes. 
 * \par Error Codes 
 * LWP EBADPRI The given priority is invalid, either negative or too large. 
 * \n LWP NOMEM Could not allocate memory to satisfy the creation request. 
 * 
 * 	\subsubsection sec2-3-1-4 Section: 2.3.1.4: LWP_DestroyProcess _ Create
 * 	a new thread 
 * 
 * \par
 * int LWP_DestroyProcess(IN PROCESS pid) 
 * \par Description 
 * This routine destroys the thread identified by pid. It will be terminated
 * immediately, and its internal storage will be reclaimed. A thread is allowed
 * to destroy itself. In this case, of course, it will only get to see the
 * return code if the operation fails. Note that a thread may also destroy
 * itself by returning from the parent C routine. 
 * \par
 * The scheduler is called by this operation, which may cause an arbitrary
 * number of threads to execute before the caller regains the processor. 
 * \par Error Codes 
 * LWP EINIT The LWP package has not been initialized. 
 * 
 * 	\subsubsection sec2-3-1-5 Section 2.3.1.5: WaitProcess _ Wait on an
 * 	event 
 * 
 * \par
 * int LWP WaitProcess(IN char *event) 
 * \par Description 
 * This routine puts the thread making the call to sleep until another LWP
 * calls the LWP SignalProcess() or LWP NoYieldSignal() routine with the
 * specified event. Note that signalled events are not queued. If a signal
 * occurs and no thread is awakened, the signal is lost. The scheduler is
 * invoked by the LWP WaitProcess() routine. 
 * \par Error Codes 
 * LWP EINIT The LWP package has not been initialized. 
 * \n LWP EBADEVENT The given event pointer is null. 
 * 
 * 	\subsubsection sec2-3-1-6 Section 2.3.1.6: MwaitProcess _ Wait on a set
 * 	of events 
 * 
 * \par
 * int LWP MwaitProcess(IN int wcount; IN char *evlist[]) 
 * \par Description 
 * This function allows a thread to wait for wcount signals on any of the items
 * in the given evlist. Any number of signals of a particular event are only
 * counted once. The evlist is a null-terminated list of events to wait for.
 * The scheduler will be invoked. 
 * \par Error Codes 
 * LWP EINIT The LWP package has not been initialized. 
 * \n LWP EBADCOUNT An illegal number of events has been supplied. 
 * 
 * 	\subsubsection sec2-3-1-7 Section 2.3.1.7: SignalProcess _ Signal an
 * 	event 
 * 
 * \par
 * int LWP SignalProcess(IN char *event) 
 * \par Description 
 * This routine causes the given event to be signalled. All threads waiting for
 * this event (exclusively) will be marked as runnable, and the scheduler will
 * be invoked. Note that threads waiting on multiple events via LWP
 * MwaitProcess() may not be marked as runnable. Signals are not queued.
 * Therefore, if no thread is waiting for the signalled event, the signal will
 * be lost. 
 * \par Error Codes 
 * LWP EINIT The LWP package has not been initialized. LWP EBADEVENT A null
 * event pointer has been provided. LWP ENOWAIT No thread was waiting on the
 * given event. 
 * 
 * 	\subsubsection sec2-3-1-8 Section 2.3.1.8: NoYieldSignal _ Signal an
 * 	event without invoking scheduler 
 * 
 * \par
 * int LWP NoYieldSignal(IN char *event) 
 * \par Description 
 * This function is identical to LWP SignalProcess() except that the scheduler
 * will not be invoked. Thus, control will remain with the signalling process. 
 * \par Error Codes 
 * LWP EINIT The LWP package has not been initialized. LWP EBADEVENT A null
 * event pointer has been provided. LWP ENOWAIT No thread was waiting on the
 * given event. 
 * 
 * 	\subsubsection sec2-3-1-9 Section 2.3.1.9: DispatchProcess _ Yield
 * 	control to the scheduler 
 * 
 * \par
 * int LWP DispatchProcess() 
 * \par Description 
 * This routine causes the calling thread to yield voluntarily to the LWP
 * scheduler. If no other thread of appropriate priority is marked as runnable,
 * the caller will continue its execution. 
 * \par Error Codes 
 * LWP EINIT The LWP package has not been initialized. 
 * 
 * 	\subsubsection sec2-3-1-10 Section 2.3.1.10: CurrentProcess _ Get the
 * 	current thread's ID 
 * 
 * \par
 * int LWP CurrentProcess(IN PROCESS *pid) 
 * \par Description 
 * This call places the current lightweight process ID in the pid parameter. 
 * \par Error Codes 
 * LWP EINIT The LWP package has not been initialized. 
 * 
 * 	\subsubsection sec2-3-1-11 Section 2.3.1.11: ActiveProcess _ Get the
 * 	current thread's ID (macro) 
 * 
 * \par
 * int LWP ActiveProcess() 
 * \par Description 
 * This macro's value is the current lightweight process ID. It generates a
 * value identical to that acquired by calling the LWP CurrentProcess()
 * function described above if the LWP package has been initialized. If no such
 * initialization has been done, it will return a value of zero. 
 * 
 * 	\subsubsection sec2-3-1-12 Section: 2.3.1.12: StackUsed _ Calculate
 * 	stack usage 
 * 
 * \par
 * int LWP StackUsed(IN PROCESS pid; OUT int *max; OUT int *used) 
 * \par Description 
 * This function returns the amount of stack space allocated to the thread
 * whose identifier is pid, and the amount actually used so far. This is
 * possible if the global variable lwp stackUseEnabled was TRUE when the thread
 * was created (it is set this way by default). If so, the thread's stack area
 * was initialized with a special pattern. The memory still stamped with this
 * pattern can be determined, and thus the amount of stack used can be
 * calculated. The max parameter is always set to the thread's stack allocation
 * value, and used is set to the computed stack usage if lwp stackUseEnabled
 * was set when the process was created, or else zero. 
 * \par Error Codes 
 * LWP NO STACK Stack usage was not enabled at thread creation time. 
 * 
 * 	\subsubsection sec2-3-1-13 Section 2.3.1.13: NewRock _ Establish
 * 	thread-specific storage 
 * 
 * \par
 * int LWP NewRock (IN int tag; IN char **value)
 * \par Description
 * This function establishes a "rock", or thread-specific information,
 * associating it with the calling LWP. The tag is intended to be any unique
 * integer value, and the value is a pointer to a character array containing
 * the given data. 
 * \par
 * Users of the LWP package must coordinate their choice of tag values. Note
 * that a tag's value cannot be changed. Thus, to obtain a mutable data
 * structure, another level of indirection is required. Up to MAXROCKS (4)
 * rocks may be associated with any given thread. 
 * \par Error Codes
 * ENOROCKS A rock with the given tag field already exists. All of the MAXROCKS
 * are in use. 
 * 
 * 
 * 	\subsubsection sec2-3-1-14 Section: 2.3.1.14: GetRock _ Retrieve
 * 	thread-specific storage 
 * 
 * \par
 * int LWP GetRock(IN int tag; OUT **value) 
 * \par Description 
 * This routine recovers the thread-specific information associated with the
 * calling process and the given tag, if any. Such a rock had to be established
 * through a LWP NewRock() call. The rock's value is deposited into value. 
 * \par Error Codes 
 * LWP EBADROCK A rock has not been associated with the given tag for this
 * thread. 
 * 
 * 	\subsection sec2-3-2 Section 2.3.2: Locking 
 * 
 * \par
 * This section covers the calling interfaces to the locking package. Many of
 * the user-callable routines are actually implemented as macros. 
 * 
 * 	\subsubsection sec2-3-2-1 Section 2.3.2.1: Lock Init _ Initialize lock
 * 	structure 
 * 
 * \par
 * void Lock Init(IN struct Lock *lock) 
 * \par Description 
 * This function must be called on the given lock object before any other
 * operations can be performed on it. 
 * \par Error Codes 
 * ---No value is returned. 
 * 
 * 	\subsubsection sec2-3-2-2 Section 2.3.2.2: ObtainReadLock _ Acquire a
 * 	read lock 
 * 
 * \par
 * void ObtainReadLock(IN struct Lock *lock) 
 * \par Description 
 * This macro obtains a read lock on the specified lock object. Since this is a
 * macro and not a function call, results are not predictable if the value of
 * the lock parameter is a side-effect producing expression, as it will be
 * evaluated multiple times in the course of the macro interpretation. 
 * Read locks are incompatible with write, shared, and boosted shared locks. 
 * \par Error Codes 
 * ---No value is returned. 
 * 
 * 	\subsubsection sec2-3-2-3 Section 2.3.2.3: ObtainWriteLock _ Acquire a
 * 	write lock 
 * 
 * \par
 * void ObtainWriteLock(IN struct Lock *lock) 
 * \par Description 
 * This macro obtains a write lock on the specified lock object. Since this is
 * a macro and not a function call, results are not predictable if the value of
 * the lock parameter is a side-effect producing expression, as it will be
 * evaluated multiple times in the course of the macro interpretation. 
 * \par
 * Write locks are incompatible with all other locks. 
 * \par Error Codes 
 * ---No value is returned. 
 * 
 * 	\subsubsection sec2-3-2-4 Section 2.3.2.4: ObtainSharedLock _ Acquire a
 * 	shared lock 
 * 
 * \par
 * void ObtainSharedLock(IN struct Lock *lock) 
 * \par Description 
 * This macro obtains a shared lock on the specified lock object. Since this is
 * a macro and not a function call, results are not predictable if the value of
 * the lock parameter is a side-effect producing expression, as it will be
 * evaluated multiple times in the course of the macro interpretation. 
 * \par
 * Shared locks are incompatible with write and boosted shared locks, but are
 * compatible with read locks. 
 * \par Error Codes 
 * ---No value is returned. 
 * 
 * 	\subsubsection sec2-3-2-5 Section 2.3.2.5: ReleaseReadLock _ Release
 * 	read lock 
 * 
 * \par
 * void ReleaseReadLock(IN struct Lock *lock) 
 * \par Description 
 * This macro releases the specified lock. The lock must have been previously
 * read-locked. Since this is a macro and not a function call, results are not
 * predictable if the value of the lock parameter is a side-effect producing
 * expression, as it will be evaluated multiple times in the course of the
 * macro interpretation. The results are also unpredictable if the lock was not
 * previously read-locked by the thread calling ReleaseReadLock(). 
 * \par Error Codes 
 * ---No value is returned. 
 * 
 * 	\subsubsection sec2-3-2-6 Section 2.3.2.6: ReleaseWriteLock _ Release
 * 	write lock 
 * 
 * \par
 * void ReleaseWriteLock(IN struct Lock *lock) 
 * \par Description 
 * This macro releases the specified lock. The lock must have been previously
 * write-locked. Since this is a macro and not a function call, results are not
 * predictable if the value of the lock parameter is a side-effect producing
 * expression, as it will be evaluated multiple times in the course of the
 * macro interpretation. The results are also unpredictable if the lock was not
 * previously write-locked by the thread calling ReleaseWriteLock(). 
 * \par Error Codes 
 * ---No value is returned. 
 * 
 * 	\subsubsection sec2-3-2-7 Section 2.3.2.7: ReleaseSharedLock _ Release
 * 	shared lock 
 * 
 * \par
 * void ReleaseSharedLock(IN struct Lock *lock) 
 * \par Description 
 * This macro releases the specified lock. The lock must have been previously
 * share-locked. Since this is a macro and not a function call, results are not
 * predictalbe if the value of the lock parameter is a side-effect producing
 * expression, as it will be evaluated multiple times in the course of the
 * macro interpretation. The results are also unpredictable if the lock was not
 * previously share-locked by the thread calling ReleaseSharedLock(). 
 * \par Error Codes 
 * ---No value is returned. 
 * 
 * 	\subsubsection sec2-3-2-8 Section 2.3.2.8: CheckLock _ Determine state
 * 	of a lock 
 * 
 * \par
 * void CheckLock(IN struct Lock *lock) 
 * \par Description 
 * This macro produces an integer that specifies the status of the indicated
 * lock. The value will be -1 if the lock is write-locked, 0 if unlocked, or
 * otherwise a positive integer that indicates the number of readers (threads
 * holding read locks). Since this is a macro and not a function call, results
 * are not predictable if the value of the lock parameter is a side-effect
 * producing expression, as it will be evaluated multiple times in the course
 * of the macro interpretation. 
 * \par Error Codes 
 * ---No value is returned. 
 * 
 * 	\subsubsection sec2-3-2-9 Section 2.3.2.9: BoostLock _ Boost a shared
 * 	lock 
 * 
 * \par
 * void BoostLock(IN struct Lock *lock) 
 * \par Description 
 * This macro promotes ("boosts") a shared lock into a write lock. Such a boost
 * operation guarantees that no other writer can get into the critical section
 * in the process. Since this is a macro and not a function call, results are
 * not predictable if the value of the lock parameter is a side-effect
 * producing expression, as it will be evaluated multiple times in the course
 * of the macro interpretation. 
 * \par Error Codes 
 * ---No value is returned. 
 * 
 * 	\subsubsection sec2-3-2-10 Section 2.3.2.10: UnboostLock _ Unboost a
 * 	shared lock 
 * 
 * \par
 * void UnboostLock(IN struct Lock *lock) 
 * \par Description 
 * This macro demotes a boosted shared lock back down into a regular shared
 * lock. Such an unboost operation guarantees that no other writer can get into
 * the critical section in the process. Since this is a macro and not a
 * function call, results are not predictable if the value of the lock
 * parameter is a side-effect producing expression, as it will be evaluated
 * multiple times in the course of the macro interpretation. 
 * \par Error Codes 
 * ---No value is returned. 
 * 
 * 	\subsection sec2-3-3 Section 2.3.3: IOMGR 
 * 
 * \par
 * This section covers the calling interfaces to the I/O management package. 
 * 
 * 	\subsubsection sec2-3-3-1 Section: 2.3.3.1: IOMGR Initialize _
 * 	Initialize the package 
 * 
 * \par
 * int IOMGR Initialize() 
 * \par Description 
 * This function initializes the IOMGR package. Its main task is to create the
 * IOMGR thread itself, which runs at the lowest possible priority (0). The
 * remainder of the lightweight processes must be running at priority 1 or
 * greater (up to a maximum of LWP MAX PRIORITY (4)) for the IOMGR package to
 * function correctly. 
 * \par Error Codes 
 * -1 The LWP and/or timer package haven't been initialized. 
 * \n <misc> Any errors that may be returned by the LWP CreateProcess()
 * routine. 
 * 
 * 	\subsubsection sec2-3-3-2 Section 2.3.3.2: IOMGR finalize _ Clean up
 * 	the IOMGR facility 
 * 
 * \par
 * int IOMGR finalize() 
 * \par Description 
 * This routine cleans up after the IOMGR package when it is no longer needed.
 * It releases all storage and destroys the IOMGR thread itself. 
 * \par Error Codes 
 * <misc> Any errors that may be returned by the LWP DestroyProcess() routine. 
 * 
 * 	\subsubsection sec2-3-3-3 Section 2.3.3.3: IOMGR Select _ Perform a
 * 	thread-level select() 
 * 
 * \par
 * int IOMGR Select (IN int numfds; IN int *rfds; IN int *wfds; IN int *xfds;
 * IN truct timeval *timeout) 
 * \par Description 
 * This routine performs an LWP version of unix select() operation. The
 * parameters have the same meanings as with the unix call. However, the return
 * values will be simplified (see below). If this is a polling select (i.e.,
 * the value of timeout is null), it is done and the IOMGR Select() function
 * returns to the user with the results. Otherwise, the calling thread is put
 * to sleep. If at some point the IOMGR thread is the only runnable process, it
 * will awaken and collect all select requests. The IOMGR will then perform a
 * single select and awaken the appropriate processes. This will force a return
 * from the affected IOMGR Select() calls. 
 * \par Error Codes 
 * -1 An error occurred.
 * \n 0 A timeout occurred.
 * \n 1 Some number of file descriptors are ready. 
 * 
 * 	\subsubsection sec2-3-3-4 Section 2.3.3.4: IOMGR Signal _ Associate
 * 	unix and LWP signals 
 * 
 * \par
 * int IOMGR Signal(IN int signo; IN char *event) 
 * \par Description 
 * This function associates an LWP signal with a unix signal. After this call,
 * when the given unix signal signo is delivered to the (heavyweight unix)
 * process, the IOMGR thread will deliver an LWP signal to the event via LWP
 * NoYieldSignal(). This wakes up any lightweight processes waiting on the
 * event. Multiple deliveries of the signal may be coalesced into one LWP
 * wakeup. The call to LWP NoYieldSignal() will happen synchronously. It is
 * safe for an LWP to check for some condition and then go to sleep waiting for
 * a unix signal without having to worry about delivery of the signal happening
 * between the check and the call to LWP WaitProcess(). 
 * \par Error Codes 
 * LWP EBADSIG The signo value is out of range. 
 * \n LWP EBADEVENT The event pointer is null. 
 * 
 * 	\subsubsection sec2-3-3-5 Section 2.3.3.5: IOMGR CancelSignal _ Cancel
 * 	unix and LWP signal association 
 * 
 * \par
 * int IOMGR CancelSignal(IN int signo) 
 * \par Description 
 * This routine cancels the association between a unix signal and an LWP event.
 * After calling this function, the unix signal signo will be handled however
 * it was handled before the corresponding call to IOMGR Signal(). 
 * \par Error Codes 
 * LWP EBADSIG The signo value is out of range. 
 * 
 * 	\subsubsection sec2-3-3-6 Section 2.3.3.6: IOMGR Sleep _ Sleep for a
 * 	given period 
 * 
 * \par
 * void IOMGR Sleep(IN unsigned seconds) 
 * \par Description 
 * This function calls IOMGR Select() with zero file descriptors and a timeout
 * structure set up to cause the thread to sleep for the given number of
 * seconds. 
 * \par Error Codes 
 * ---No value is returned. 
 * 
 * 	\subsection sec2-3-4 Section 2.3.4: Timer 
 * 
 * \par
 * This section covers the calling interface to the timer package associated
 * with the LWP facility. 
 * 
 * 	\subsubsection sec2-3-4-1 Section 2.3.4.1: TM Init _ Initialize a timer
 * 	list 
 * 
 * \par
 * int TM Init(IN struct TM Elem **list) 
 * \par Description 
 * This function causes the specified timer list to be initialized. TM Init()
 * must be called before any other timer operations are applied to the list. 
 * \par Error Codes 
 * -1 A null timer list could not be produced. 
 * 
 * 	\subsubsection sec2-3-4-2 Section 2.3.4.2: TM final _ Clean up a timer
 * 	list 
 * 
 * \par
 * int TM final(IN struct TM Elem **list) 
 * \par Description 
 * This routine is called when the given empty timer list is no longer needed.
 * All storage associated with the list is released. 
 * \par Error Codes 
 * -1 The list parameter is invalid. 
 * 
 * 	\subsubsection sec2-3-4-3 Section 2.3.4.3: TM Insert _ Insert an object
 * 	into a timer list
 * 
 * \par
 * void TM Insert(IN struct TM Elem **list; IN struct TM Elem *elem)
 * \par Description
 * This routine enters an new element, elem, into the list denoted by list.
 * Before the new element is queued, its TimeLeft field (the amount of time
 * before the object comes due) is set to the value stored in its TotalTime
 * field. In order to keep TimeLeft fields current, the TM Rescan() function
 * may be used. 
 * \par Error Codes 
 * ---No return value is generated. 
 * 
 * 	\subsubsection sec2-3-4-4 Section 2.3.4.4: TM Rescan _ Update all
 * 	timers in the list 
 * 
 * \par
 * int TM Rescan(IN struct TM Elem *list) 
 * \par Description 
 * This function updates the TimeLeft fields of all timers on the given list.
 * This is done by checking the time-of-day clock. Note: this is the only
 * routine other than TM Init() that updates the TimeLeft field in the elements
 * on the list. 
 * \par
 * Instead of returning a value indicating success or failure, TM Rescan()
 * returns the number of entries that were discovered to have timed out. 
 * \par Error Codes 
 * ---Instead of error codes, the number of entries that were discovered to
 *  have timed out is returned. 
 * 
 * 	\subsubsection sec2-3-4-5 Section 2.3.4.5: TM GetExpired _ Returns an
 * 	expired timer 
 * 
 * \par
 * struct TM Elem *TM GetExpired(IN struct TM Elem *list) 
 * \par Description 
 * This routine searches the specified timer list and returns a pointer to an
 * expired timer element from that list. An expired timer is one whose TimeLeft
 * field is less than or equal to zero. If there are no expired timers, a null
 * element pointer is returned. 
 * \par Error Codes 
 * ---Instead of error codes, an expired timer pointer is returned, or a null
 *  timer pointer if there are no expired timer objects. 
 * 
 * 	\subsubsection sec2-3-4-6 Section 2.3.4.6: TM GetEarliest _ Returns
 * 	earliest unexpired timer 
 * 
 * \par
 * struct TM Elem *TM GetEarliest(IN struct TM Elem *list) 
 * \par Description 
 * This function returns a pointer to the timer element that will be next to
 * expire on the given list. This is defined to be the timer element with the
 * smallest (positive) TimeLeft field. If there are no timers on the list, or
 * if they are all expired, this function will return a null pointer. 
 * \par Error Codes 
 * ---Instead of error codes, a pointer to the next timer element to expireis
 *  returned, or a null timer object pointer if they are all expired. 
 * 
 * 	\subsubsection sec2-3-4-7 Section 2.3.4.7: TM eql _ Test for equality
 * 	of two timestamps 
 * 
 * \par
 * bool TM eql(IN struct timemval *t1; IN struct timemval *t2) 
 * \par Description 
 * This function compares the given timestamps, t1 and t2, for equality. Note
 * that the function return value, bool, has been set via typedef to be
 * equivalent to unsigned char. 
 * \par Error Codes 
 * 0 If the two timestamps differ. 
 * \n 1 If the two timestamps are identical. 
 * 
 * 	\subsection sec2-3-5 Section 2.3.5: Fast Time 
 * \par
 * This section covers the calling interface to the fast time package
 * associated with the LWP facility. 
 * 
 * 	\subsubsection sec2-3-5-1 Section 2.3.5.1: FT Init _ Initialize the
 * 	fast time package 
 * 
 * \par
 * int FT Init(IN int printErrors; IN int notReally) 
 * \par Description 
 * This routine initializes the fast time package, mapping in the kernel page
 * containing the time-of-day variable. The printErrors argument, if non-zero,
 * will cause any errors in initalization to be printed to stderr. The
 * notReally parameter specifies whether initialization is really to be done.
 * Other calls in this package will do auto-initialization, and hence the
 * option is offered here. 
 * \par Error Codes 
 * -1 Indicates that future calls to FT GetTimeOfDay() will still work, but
 *  will not be able to access the information directly, having to make a
 *  kernel call every time. 
 * 
 * 	\subsubsection sec2-3-5-2 Section 2.3.5.2: FT GetTimeOfDay _ Initialize
 * 	the fast time package 
 * 
 * \par
 * int FT GetTimeOfDay(IN struct timeval *tv; IN struct timezone *tz) 
 * \par Description 
 * This routine is meant to mimic the parameters and behavior of the unix
 * gettimeofday() function. However, as implemented, it simply calls
 * gettimeofday() and then does some bound-checking to make sure the value is
 * reasonable. 
 * \par Error Codes 
 * <misc> Whatever value was returned by gettimeofday() internally. 
 * 
 * 	\subsection sec2-3-6 Section 2.3.6: Preemption 
 * \par
 * This section covers the calling interface to the preemption package
 * associated with the LWP facility. 
 * 
 * 	\subsubsection sec2-3-6-1 Section 2.3.6.1: PRE InitPreempt _ Initialize
 * 	the preemption package 
 * 
 * \par
 * int PRE InitPreempt(IN struct timeval *slice) 
 * \par Description 
 * This function must be called to initialize the preemption package. It must
 * appear sometime after the call to LWP InitializeProcessSupport() and
 * sometime before the first call to any other preemption routine. The slice
 * argument specifies the time slice size to use. If the slice pointer is set
 * to null in the call, then the default time slice, DEFAULTSLICE (10
 * milliseconds), will be used. This routine uses the unix interval timer and
 * handling of the unix alarm signal, SIGALRM, to implement this timeslicing. 
 * \par Error Codes 
 * LWP EINIT The LWP package hasn't been initialized. 
 * \n LWP ESYSTEM Operations on the signal vector or the interval timer have
 * failed. 
 * 
 * 	\subsubsection sec2-3-6-2 Section 2.3.6.2: PRE EndPreempt _ finalize
 * 	the preemption package 
 * 
 * \par
 * int PRE EndPreempt() 
 * \par Description 
 * This routine finalizes use of the preemption package. No further preemptions
 * will be made. Note that it is not necessary to make this call before exit.
 * PRE EndPreempt() is provided only for those applications that wish to
 * continue after turning off preemption. 
 * \par Error Codes 
 * LWP EINIT The LWP package hasn't been initialized. 
 * \n LWP ESYSTEM Operations on the signal vector or the interval timer have
 * failed. 
 * 
 * 	\subsubsection sec2-3-6-3 Section 2.3.6.3: PRE PreemptMe _ Mark thread
 * 	as preemptible 
 * 
 * \par
 * int PRE PreemptMe() 
 * \par Description 
 * This macro is used to signify the current thread as a candidate for
 * preemption. The LWP InitializeProcessSupport() routine must have been called
 * before PRE PreemptMe(). 
 * \par Error Codes 
 * ---No return code is generated. 
 * 
 * 	\subsubsection sec2-3-6-4 Section 2.3.6.4: PRE BeginCritical _ Enter
 * 	thread critical section 
 * 
 * \par
 * int PRE BeginCritical() 
 * \par Description 
 * This macro places the current thread in a critical section. Upon return, and
 * for as long as the thread is in the critical section, involuntary
 * preemptions of this LWP will no longer occur. 
 * \par Error Codes 
 * ---No return code is generated. 
 * 
 * 	\subsubsection sec2-3-6-5 Section 2.3.6.5: PRE EndCritical _ Exit
 * 	thread critical section 
 * 
 * \par
 * int PRE EndCritical() 
 * \par Description 
 * This macro causes the executing thread to leave a critical section
 * previously entered via PRE BeginCritical(). If involuntary preemptions were
 * possible before the matching PRE BeginCritical(), they are once again
 * possible. 
 * \par Error Codes 
 * ---No return code is generated. 
 * 
 * 	\page chap3 Chapter 3 -- Rxkad 
 * 
 * 
 * 	\section sec3-1 Section 3.1: Introduction 
 * 
 * \par
 * The rxkad security module is offered as one of the built-in Rx
 * authentication models. It is based on the Kerberos system developed by MIT's
 * Project Athena. Readers wishing detailed information regarding Kerberos
 * design and implementation are directed to [2]. This chapter is devoted to
 * defining how Kerberos authentication services are made available as Rx
 * components, and assumes the reader has some familiarity with Kerberos.
 * Included are descriptions of how client-side and server-side Rx security
 * objects (struct rx securityClass; see Section 5.3.1.1) implementing this
 * protocol may be generated by an Rx application. Also, a description appears
 * of the set of routines available in the associated struct rx securityOps
 * structures, as covered in Section 5.3.1.2. It is strongly recommended that
 * the reader become familiar with this section on struct rx securityOps before
 * reading on. 
 * 
 * 	\section sec3-2 Section 3.2: Definitions 
 * 
 * \par
 * An important set of definitions related to the rxkad security package is
 * provided by the rxkad.h include file. Determined here are various values for
 * ticket lifetimes, along with structures for encryption keys and Kerberos
 * principals. Declarations for the two routines required to generate the
 * different rxkad security objects also appear here. The two functions are
 * named rxkad NewServerSecurityObject() and rxkad NewClientSecurityObject().
 * In addition, type field values, encryption levels, security index
 * operations, and statistics structures may be found in this file. 
 * 	\section sec3-3 Section 3.3: Exported Objects 
 * \par
 * To be usable as an Rx security module, the rxkad facility exports routines
 * to create server-side and client-side security objects. The server
 * authentication object is incorporated into the server code when calling rx
 * NewService(). The client authentication object is incorporated into the
 * client code every time a connection is established via rx NewConnection().
 * Also, in order to implement these security objects, the rxkad module must
 * provide definitions for some subset of the generic security operations as
 * defined in the appropriate struct rx securityOps variable. 
 * 
 * 	\subsection sec3-3-1 Section 3.3.1: Server-Side Mechanisms 
 * 
 * 	\subsubsection sec3-3-1-1 Section 3.3.1.1: Security Operations 
 * 
 * \par
 * The server side of the rxkad module fills in all but two of the possible
 * routines associated with an Rx security object, as described in Section
 * 5.3.1.2. 
 * 
 * \code
 * static struct rx_securityOps rxkad_server_ops = { 
 * 	rxkad_Close,
 * 	rxkad_NewConnection,
 * 	rxkad_PreparePacket, /* Once per packet creation */
 * 	0, /* Send packet (once per retrans) */
 * 	rxkad_CheckAuthentication,
 * 	rxkad_CreateChallenge,
 * 	rxkad_GetChallenge,
 * 	0,
 * 	rxkad_CheckResponse, /* Check data packet */
 * 	rxkad_DestroyConnection,
 * 	rxkad_GetStats,
 * };
 * \endcode
 * 
 * \par
 * The rxkad service does not need to take any special action each time a
 * packet belonging to a call in an rxkad Rx connection is physically
 * transmitted. Thus, a routine is not supplied for the op SendPacket()
 * function slot. Similarly, no preparatory work needs to be done previous to
 * the reception of a response packet from a security challenge, so the op
 * GetResponse() function slot is also empty. 
 * 
 * 	\subsubsection sec3-3-1-2 Section 3.3.1.2: Security Object 
 * 
 * \par
 * The exported routine used to generate an rxkad-specific server-side security
 * class object is named rxdad NewServerSecurityObject(). It is declared with
 * four parameters, as follows: 
 * 
 * \code
 * struct rx_securityClass * 
 * rxkad_NewServerSecurityObject(a_level, a_getKeyRockP, a_getKeyP, a_userOKP) 
 * rxkad_level a_level; /* Minimum level */
 * char *a_getKeyRockP; /* Rock for get_key implementor */
 * int (*a_getKeyP)(); /* Passed kvno & addr(key) to fill */
 * int (*a_userOKP)(); /* Passed name, inst, cell => bool */
 * \endcode
 * 
 * \par
 * The first argument specifies the desired level of encryption, and may take
 * on the following values (as defined in rxkad.h): 
 * \li rxkad clear: Specifies that packets are to be sent entirely in the
 * clear, without any encryption whatsoever. 
 * \li rxkad auth: Specifies that packet sequence numbers are to be encrypted. 
 * \li rxkad crypt: Specifies that the entire data packet is to be encrypted. 
 * 
 * \par
 * The second and third parameters represent, respectively, a pointer to a
 * private data area, sometimes called a "rock", and a procedure reference that
 * is called with the key version number accompanying the Kerberos ticket and
 * returns a pointer to the server's decryption key. The fourth argument, if
 * not null, is a pointer to a function that will be called for every new
 * connection with the client's name, instance, and cell. This routine should
 * return zero if the user is not acceptable to the server. 
 * 
 * 	\subsection sec3-3-2 Section 3.3.2: Client-Side Mechanisms 
 * 
 * 	\subsubsection sec3-3-2-1 Section 3.3.2.1: Security Operations 
 * 
 * \par
 * The client side of the rxkad module fills in relatively few of the routines
 * associated with an Rx security object, as demonstrated below. The general Rx
 * security object, of which this is an instance, is described in detail in
 * Section 5.3.1.2. 
 * 
 * \code
 * static struct rx_securityOps rxkad_client_ops = { 
 * 	rxkad_Close, 
 * 	rxkad_NewConnection, /* Every new connection */
 * 	rxkad_PreparePacket, /* Once per packet creation */
 * 	0, /* Send packet (once per retrans) */
 * 	0,
 * 	0,
 * 	0,
 * 	rxkad_GetResponse, /* Respond to challenge packet */
 * 	0,
 * 	rxkad_CheckPacket, /* Check data packet */
 * 	rxkad_DestroyConnection,
 * 	rxkad_GetStats,
 * 	0,
 * 	0,
 * 	0,
 * };
 * \endcode
 * 
 * \par
 * As expected, routines are defined for use when someone destroys a security
 * object (rxkad Close()) and when an Rx connection using the rxkad model
 * creates a new connection (rxkad NewConnection()) or deletes an existing one
 * (rxkad DestroyConnection()). Security-specific operations must also be
 * performed in behalf of rxkad when packets are created (rxkad
 * PreparePacket()) and received (rxkad CheckPacket()). finally, the client
 * side of an rxkad security object must also be capable of constructing
 * responses to security challenges from the server (rxkad GetResponse()) and
 * be willing to reveal statistics on its own operation (rxkad GetStats()). 
 * 
 * 	\subsubsection sec3-3-2-2 Section 3.3.2.2: Security Object 
 * 
 * \par
 * The exported routine used to generate an rxkad-specific client-side security
 * class object is named rxkad NewClientSecurityObject(). It is declared with
 * five parameters, specified below: 
 * 
 * \code
 * struct rx_securityClass * rxkad_NewClientSecurityObject(
 * 				a_level, 
 * 				a_sessionKeyP, 
 * 				a_kvno, 
 * 				a_ticketLen, 
 * 				a_ticketP
 * 				) 
 * rxkad_level a_level; 
 * struct ktc_encryptionKey *a_sessionKeyP; 
 * long a_kvno; 
 * int a_ticketLen; 
 * char *a_ticketP; 
 * \endcode
 * 
 * \par
 * The first parameter, a level, specifies the level of encryption desired for
 * this security object, with legal choices being identical to those defined
 * for the server-side security object described in Section 3.3.1.2. The second
 * parameter, a sessionKeyP, provides the session key to use. The ktc
 * encryptionKey structure is defined in the rxkad.h include file, and consists
 * of an array of 8 characters. The third parameter, a kvno, provides the key
 * version number associated with a sessionKeyP. The fourth argument, a
 * ticketLen, communicates the length in bytes of the data stored in the fifth
 * parameter, a ticketP, which points to the Kerberos ticket to use for the
 * principal for which the security object will operate. 
 * 
 * 	\page chap4 Chapter 4 -- Rx Support Packages 
 * 
 * 	\section sec4-1 Section 4.1: Introduction 
 * \par
 * This chapter documents three packages defined directly in support of the Rx
 * facility. 
 * \li rx queue: Doubly-linked queue package. 
 * \li rx clock: Clock package, using the 4.3BSD interval timer. 
 * \li rx event: Future events package. 
 * \par
 * References to constants, structures, and functions defined by these support
 * packages will appear in the following API chapter. 
 * 
 * 	\section sec4-2 Section 4.2: The rx queue Package 
 * 
 * \par
 * This package provides a doubly-linked queue structure, along with a full
 * suite of related operations. The main concern behind the coding of this
 * facility was efficiency. All functions are implemented as macros, and it is
 * suggested that only simple expressions be used for all parameters. 
 * \par
 * The rx queue facility is defined by the rx queue.h include file. Some macros
 * visible in this file are intended for rx queue internal use only. An
 * understanding of these "hidden" macros is important, so they will also be
 * described by this document. 
 * 
 * 	\subsection sec4-2-1 Section 4.2.1: struct queue 
 * 
 * \par
 * The queue structure provides the linkage information required to maintain a
 * queue of objects. The queue structure is prepended to any user-defined data
 * type which is to be organized in this fashion. 
 * \n \b fields 
 * \li struct queue *prev - Pointer to the previous queue header. 
 * \li struct queue *next - Pointer to the next queue header. 
 * \par
 * Note that a null Rx queue consists of a single struct queue object whose
 * next and previous pointers refer to itself. 
 * 
 * 	\subsection sec4-2-2 Section 4.2.2: Internal Operations 
 * 
 * \par
 * This section describes the internal operations defined for Rx queues. They
 * will be referenced by the external operations documented in Section 4.2.3. 
 * 
 * 	\subsection sec4-2-2-1 Section 4.2.2.1: Q(): Coerce type to a queue
 * 	element 
 * 
 * \par
 * \#define _Q(x) ((struct queue *)(x)) 
 * \par
 * This operation coerces the user structure named by x to a queue element. Any
 * user structure using the rx queue package must have a struct queue as its
 * first field. 
 * 
 * 	\subsubsection sec4-2-2-2 Section 4.2.2.2: QA(): Add a queue element
 * 	before/after another element 
 * 
 * \par
 * \#define _QA(q,i,a,b) (((i->a=q->a)->b=i)->b=q, q->a=i) 
 * \par
 * This operation adds the queue element referenced by i either before or after
 * a queue element represented by q. If the (a, b) argument pair corresponds to
 * an element's (next, prev) fields, the new element at i will be linked after
 * q. If the (a, b) argument pair corresponds to an element's (prev, next)
 * fields, the new element at i will be linked before q. 
 * 
 * 	\subsubsection sec4-2-2-3 QR(): Remove a queue element 
 * 
 * \par
 * \#define _QR(i) ((_Q(i)->prev->next=_Q(i)->next)->prev=_Q(i)->prev) 
 * \par
 * This operation removes the queue element referenced by i from its queue. The
 * prev and next fields within queue element i itself is not updated to reflect
 * the fact that it is no longer part of the queue. 
 * 
 * 	\subsubsection sec4-2-2-4 QS(): Splice two queues together 
 * 
 * \par
 * \#define _QS(q1,q2,a,b) if (queue_IsEmpty(q2)); else
 * ((((q2->a->b=q1)->a->b=q2->b)->a=q1->a, q1->a=q2->a), queue_Init(q2)) 
 * \par
 * This operation takes the queues identified by q1 and q2 and splices them
 * together into a single queue. The order in which the two queues are appended
 * is determined by the a and b arguments. If the (a, b) argument pair
 * corresponds to q1's (next, prev) fields, then q2 is appended to q1. If the
 * (a, b) argument pair corresponds to q1's (prev, next) fields, then q is
 * prepended to q2. 
 * \par
 * This internal QS() routine uses two exported queue operations, namely queue
 * Init() and queue IsEmpty(), defined in Sections 4.2.3.1 and 4.2.3.16
 * respectively below. 
 * 
 * 	\subsection sec4-2-3 Section 4.2.3: External Operations 
 * 
 * 	\subsubsection sec4-2-3-1 Section 4.2.3.1: queue Init(): Initialize a
 * 	queue header 
 * 
 * \par
 * \#define queue_Init(q) (_Q(q))->prev = (_Q(q))->next = (_Q(q)) 
 * \par
 * The queue header referred to by the q argument is initialized so that it
 * describes a null (empty) queue. A queue head is simply a queue element. 
 * 
 * 	\subsubsection sec4-2-3-2 Section 4.2.3.2: queue Prepend(): Put element
 * 	at the head of a queue 
 * 
 * \par
 * \#define queue_Prepend(q,i) _QA(_Q(q),_Q(i),next,prev) 
 * \par
 * Place queue element i at the head of the queue denoted by q. The new queue
 * element, i, should not currently be on any queue. 
 * 
 * 	\subsubsection sec4-2-3-3 Section 4.2.3.3: queue Append(): Put an
 * 	element a the tail of a queue 
 * 
 * \par
 * \#define queue_Append(q,i) _QA(_Q(q),_Q(i),prev,next) 
 * \par
 * Place queue element i at the tail of the queue denoted by q. The new queue
 * element, i, should not currently be on any queue. 
 * 
 * 	\subsection sec4-2-3-4 Section 4.2.3.4: queue InsertBefore(): Insert a
 * 	queue element before another element 
 * 
 * \par
 * \#define queue_InsertBefore(i1,i2) _QA(_Q(i1),_Q(i2),prev,next) 
 * \par
 * Insert queue element i2 before element i1 in i1's queue. The new queue
 * element, i2, should not currently be on any queue. 
 * 
 * 	\subsubsection sec4-2-3-5 Section 4.2.3.5: queue InsertAfter(): Insert
 * 	a queue element after another element 
 * 
 * \par
 * \#define queue_InsertAfter(i1,i2) _QA(_Q(i1),_Q(i2),next,prev) 
 * \par
 * Insert queue element i2 after element i1 in i1's queue. The new queue
 * element, i2, should not currently be on any queue. 
 * 
 * 	\subsubsection sec4-2-3-6 Section: 4.2.3.6: queue SplicePrepend():
 * 	Splice one queue before another 
 * 
 * \par
 * \#define queue_SplicePrepend(q1,q2) _QS(_Q(q1),_Q(q2),next,prev) 
 * \par
 * Splice the members of the queue located at q2 to the beginning of the queue
 * located at q1, reinitializing queue q2. 
 * 
 * 	\subsubsection sec4-2-3-7 Section 4.2.3.7: queue SpliceAppend(): Splice
 * 	one queue after another 
 * 
 * \par
 * \#define queue_SpliceAppend(q1,q2) _QS(_Q(q1),_Q(q2),prev,next) 
 * \par
 * Splice the members of the queue located at q2 to the end of the queue
 * located at q1, reinitializing queue q2. Note that the implementation of
 * queue SpliceAppend() is identical to that of queue SplicePrepend() except
 * for the order of the next and prev arguments to the internal queue splicer,
 * QS(). 
 * 
 * 	\subsubsection sec4-2-3-8 Section 4.2.3.8: queue Replace(): Replace the
 * 	contents of a queue with that of another 
 * 
 * \par
 * \#define queue_Replace(q1,q2) (*_Q(q1) = *_Q(q2), 
 * \n _Q(q1)->next->prev = _Q(q1)->prev->next = _Q(q1), 
 * \n queue_Init(q2)) 
 * \par
 * Replace the contents of the queue located at q1 with the contents of the
 * queue located at q2. The prev and next fields from q2 are copied into the
 * queue object referenced by q1, and the appropriate element pointers are
 * reassigned. After the replacement has occurred, the queue header at q2 is
 * reinitialized. 
 * 
 * 	\subsubsection sec4-2-3-9 Section 4.2.3.9: queue Remove(): Remove an
 * 	element from its queue 
 * 
 * \par
 * \#define queue_Remove(i) (_QR(i), _Q(i)->next = 0) 
 * \par
 * This function removes the queue element located at i from its queue. The
 * next field for the removed entry is zeroed. Note that multiple removals of
 * the same queue item are not supported. 
 * 
 * 	\subsubsection sec4-2-3-10 Section 4.2.3.10: queue MoveAppend(): Move
 * 	an element from its queue to the end of another queue 
 * 
 * \par
 * \#define queue_MoveAppend(q,i) (_QR(i), queue_Append(q,i)) 
 * \par
 * This macro removes the queue element located at i from its current queue.
 * Once removed, the element at i is appended to the end of the queue located
 * at q. 
 * 
 * 	\subsubsection sec4-2-3-11 Section 4.2.3.11: queue MovePrepend(): Move
 * 	an element from its queue to the head of another queue 
 * 
 * \par
 * \#define queue_MovePrepend(q,i) (_QR(i), queue_Prepend(q,i)) 
 * \par
 * This macro removes the queue element located at i from its current queue.
 * Once removed, the element at i is inserted at the head fo the queue located
 * at q. 
 * 
 * 	\subsubsection sec4-2-3-12 Section 4.2.3.12: queue first(): Return the
 * 	first element of a queue, coerced to a particular type 
 * 
 * \par
 * \#define queue_first(q,s) ((struct s *)_Q(q)->next) 
 * \par
 * Return a pointer to the first element of the queue located at q. The
 * returned pointer value is coerced to conform to the given s structure. Note
 * that a properly coerced pointer to the queue head is returned if q is empty. 
 * 
 * 	\subsubsection sec4-2-3-13 Section 4.2.3.13: queue Last(): Return the
 * 	last element of a queue, coerced to a particular type 
 * 
 * \par
 * \#define queue_Last(q,s) ((struct s *)_Q(q)->prev) 
 * \par
 * Return a pointer to the last element of the queue located at q. The returned
 * pointer value is coerced to conform to the given s structure. Note that a
 * properly coerced pointer to the queue head is returned if q is empty. 
 * 
 * 	\subsubsection sec4-2-3-14 Section 4.2.3.14: queue Next(): Return the
 * 	next element of a queue, coerced to a particular type 
 * 
 * \par
 * \#define queue_Next(i,s) ((struct s *)_Q(i)->next) 
 * \par
 * Return a pointer to the queue element occuring after the element located at
 * i. The returned pointer value is coerced to conform to the given s
 * structure. Note that a properly coerced pointer to the queue head is
 * returned if item i is the last in its queue. 
 * 
 * 	\subsubsection sec4-2-3-15 Section 4.2.3.15: queue Prev(): Return the
 * 	next element of a queue, coerced to a particular type 
 * 
 * \par
 * \#define queue_Prev(i,s) ((struct s *)_Q(i)->prev) 
 * \par
 * Return a pointer to the queue element occuring before the element located at
 * i. The returned pointer value is coerced to conform to the given s
 * structure. Note that a properly coerced pointer to the queue head is
 * returned if item i is the first in its queue. 
 * 
 * 	\subsubsection sec4-2-3-16 Section 4.2.3.16: queue IsEmpty(): Is the
 * 	given queue empty? 
 * 
 * \par
 * \#define queue_IsEmpty(q) (_Q(q)->next == _Q(q)) 
 * \par
 * Return a non-zero value if the queue located at q does not have any elements
 * in it. In this case, the queue consists solely of the queue header at q
 * whose next and prev fields reference itself. 
 * 
 * 	\subsubsection sec4-2-3-17 Section 4.2.3.17: queue IsNotEmpty(): Is the
 * 	given queue not empty? 
 * 
 * \par
 * \#define queue_IsNotEmpty(q) (_Q(q)->next != _Q(q)) 
 * \par
 * Return a non-zero value if the queue located at q has at least one element
 * in it other than the queue header itself. 
 * 
 * 	\subsubsection sec4-2-3-18 Section 4.2.3.18: queue IsOnQueue(): Is an
 * 	element currently queued? 
 * 
 * \par
 * \#define queue_IsOnQueue(i) (_Q(i)->next != 0) 
 * \par
 * This macro returns a non-zero value if the queue item located at i is
 * currently a member of a queue. This is determined by examining its next
 * field. If it is non-null, the element is considered to be queued. Note that
 * any element operated on by queue Remove() (Section 4.2.3.9) will have had
 * its next field zeroed. Hence, it would cause a non-zero return from this
 * call. 
 * 
 * 	\subsubsection sec4-2-3-19 Section 4.2.3.19: queue Isfirst(): Is an
 * 	element the first on a queue? 
 * 
 * \par
 * \#define queue_Isfirst(q,i) (_Q(q)->first == _Q(i)) 
 * \par
 * This macro returns a non-zero value if the queue item located at i is the
 * first element in the queue denoted by q. 
 * 
 * 	\subsubsection sec4-2-3-20 Section 4.2.3.20: queue IsLast(): Is an
 * 	element the last on a queue? 
 * 
 * \par
 * \#define queue_IsLast(q,i) (_Q(q)->prev == _Q(i)) 
 * \par
 * This macro returns a non-zero value if the queue item located at i is the
 * last element in the queue denoted by q. 
 * 
 * 	\subsubsection sec4-2-3-21 Section 4.2.3.21: queue IsEnd(): Is an
 * 	element the end of a queue? 
 * 
 * \par
 * \#define queue_IsEnd(q,i) (_Q(q) == _Q(i)) 
 * \par
 * This macro returns a non-zero value if the queue item located at i is the
 * end of the queue located at q. Basically, it determines whether a queue
 * element in question is also the queue header structure itself, and thus does
 * not represent an actual queue element. This function is useful for
 * terminating an iterative sweep through a queue, identifying when the search
 * has wrapped to the queue header. 
 * 
 * 	\subsubsection sec4-2-3-22 Section 4.2.3.22: queue Scan(): for loop
 * 	test for scanning a queue in a forward direction 
 * 
 * \par
 * \#define queue_Scan(q, qe, next, s) 
 * \n (qe) = queue_first(q, s), next = queue_Next(qe, s); 
 * \n !queue_IsEnd(q, qe); 
 * \n (qe) = (next), next = queue_Next(qe, s) 
 * \par
 * This macro may be used as the body of a for loop test intended to scan
 * through each element in the queue located at q. The qe argument is used as
 * the for loop variable. The next argument is used to store the next value for
 * qe in the upcoming loop iteration. The s argument provides the name of the
 * structure to which each queue element is to be coerced. Thus, the values
 * provided for the qe and next arguments must be of type (struct s *). 
 * \par
 * An example of how queue Scan() may be used appears in the code fragment
 * below. It declares a structure named mystruct, which is suitable for
 * queueing. This queueable structure is composed of the queue pointers
 * themselves followed by an integer value. The actual queue header is kept in
 * demoQueue, and the currItemP and nextItemP variables are used to step
 * through the demoQueue. The queue Scan() macro is used in the for loop to
 * generate references in currItemP to each queue element in turn for each
 * iteration. The loop is used to increment every queued structure's myval
 * field by one. 
 * 
 * \code
 * struct mystruct { 
 * 	struct queue q; 
 * 	int myval; 
 * }; 
 * struct queue demoQueue; 
 * struct mystruct *currItemP, *nextItemP; 
 * ... 
 * for (queue_Scan(&demoQueue, currItemP, nextItemP, mystruct)) { 
 * 	currItemP->myval++; 
 * } 
 * \endcode
 * 
 * \par
 * Note that extra initializers can be added before the body of the queue
 * Scan() invocation above, and extra expressions can be added afterwards. 
 * 
 * 	\subsubsection sec4-2-3-23 Section 4.2.3.23: queue ScanBackwards(): for
 * 	loop test for scanning a queue in a reverse direction 
 * 
 * \par
 * #define queue_ScanBackwards(q, qe, prev, s) 
 * \n (qe) = queue_Last(q, s), prev = queue_Prev(qe, s); 
 * \n !queue_IsEnd(q, qe); 
 * \n (qe) = prev, prev = queue_Prev(qe, s) 
 * \par
 * This macro is identical to the queue Scan() macro described above in Section
 * 4.2.3.22 except for the fact that the given queue is scanned backwards,
 * starting at the last item in the queue. 
 * 
 * 	\section sec4-3 Section 4.3: The rx clock Package 
 * 
 * \par
 * This package maintains a clock which is independent of the time of day. It
 * uses the unix 4.3BSD interval timer (e.g., getitimer(), setitimer()) in
 * TIMER REAL mode. Its definition and interface may be found in the rx clock.h
 * include file. 
 * 
 * 	\subsection sec4-3-1 Section 4.3.1: struct clock 
 * 
 * \par
 * This structure is used to represent a clock value as understood by this
 * package. It consists of two fields, storing the number of seconds and
 * microseconds that have elapsed since the associated clock Init() routine has
 * been called. 
 * \par
 * \b fields 
 * \n long sec -Seconds since call to clock Init(). 
 * \n long usec -Microseconds since call to clock Init(). 
 * 
 * 	\subsection sec4-3-2 Section 4.3.12: clock nUpdates 
 * 
 * \par
 * The integer-valued clock nUpdates is a variable exported by the rx clock
 * facility. It records the number of times the clock value is actually
 * updated. It is bumped each time the clock UpdateTime() routine is called, as
 * described in Section 4.3.3.2. 
 * 
 * 	\subsection sec4-3-3 Section 4.3.3: Operations 
 * 
 * 	\subsubsection sec4-3-3-1 Section 4.3.3.1: clock Init(): Initialize the
 * 	clock package 
 * 
 * \par
 * This routine uses the unix setitimer() call to initialize the unix interval
 * timer. If the setitimer() call fails, an error message will appear on
 * stderr, and an exit(1) will be executed. 
 * 
 * 	\subsubsection sec4-3-3-2 Section 4.3.3.2: clock UpdateTime(): Compute
 * 	the current time 
 * 
 * \par
 * The clock UpdateTime() function calls the unix getitimer() routine in order
 * to update the current time. The exported clock nUpdates variable is
 * incremented each time the clock UpdateTime() routine is called. 
 * 
 * 	\subsubsection sec4-3-3-3 Section 4.3.3.3: clock GetTime(): Return the
 * 	current clock time 
 * 
 * \par
 * This macro updates the current time if necessary, and returns the current
 * time into the cv argument, which is declared to be of type (struct clock *). 
 * 4.3.3.4 clock Sec(): Get the current clock time, truncated to seconds 
 * This macro returns the long value of the sec field of the current time. The
 * recorded time is updated if necessary before the above value is returned. 
 * 
 * 	\subsubsection sec4-3-3-5 Section 4.3.3.5: clock ElapsedTime(): Measure
 * 	milliseconds between two given clock values 
 * 
 * \par
 * This macro returns the elapsed time in milliseconds between the two clock
 * structure pointers provided as arguments, cv1 and cv2. 
 * 
 * 	\subsubsection sec4-3-3-6 Section 4.3.3.6: clock Advance(): Advance the
 * 	recorded clock time by a specified clock value 
 * 
 * \par
 * This macro takes a single (struct clock *) pointer argument, cv, and adds
 * this clock value to the internal clock value maintined by the package. 
 * 
 * 	\subsubsection sec4-3-3-7 Section 4.3.3.7: clock Gt(): Is a clock value
 * 	greater than another? 
 * 
 * \par
 * This macro takes two parameters of type (struct clock *), a and b. It
 * returns a nonzero value if the a parameter points to a clock value which is
 * later than the one pointed to by b. 
 * 
 * 	\subsubsection sec4-3-3-8 Section 4.3.3.8: clock Ge(): Is a clock value
 * 	greater than or equal to another? 
 * 
 * \par
 * This macro takes two parameters of type (struct clock *), a and b. It
 * returns a nonzero value if the a parameter points to a clock value which is
 * greater than or equal to the one pointed to by b. 
 * 
 * 	\subsubsection sec4-3-3-9 Section 4.3.3.9: clock Gt(): Are two clock
 * 	values equal? 
 * 
 * \par
 * This macro takes two parameters of type (struct clock *), a and b. It
 * returns a non-zero value if the clock values pointed to by a and b are
 * equal. 
 * 
 * 	\subsubsection sec4.3.3.10 Section 4.3.3.10: clock Le(): Is a clock
 * 	value less than or equal to another? 
 * 
 * \par
 * This macro takes two parameters of type (struct clock *), a and b. It
 * returns a nonzero value if the a parameter points to a clock value which is
 * less than or equal to the one pointed to by b. 
 * 
 * 	\subsubsection sec4-3-3-11 Section 4.3.3.11: clock Lt(): Is a clock
 * 	value less than another? 
 * 
 * \par
 * This macro takes two parameters of type (struct clock *), a and b. It
 * returns a nonzero value if the a parameter points to a clock value which is
 * less than the one pointed to by b. 
 * 
 * 	\subsubsection sec4-3-3-12 Section 4.3.3.12: clock IsZero(): Is a clock
 * 	value zero? 
 * 
 * \par
 * This macro takes a single parameter of type (struct clock *), c. It returns
 * a non-zero value if the c parameter points to a clock value which is equal
 * to zero. 
 * 
 * 	\subsubsection sec4-3-3-13 Section 4.3.3.13: clock Zero(): Set a clock
 * 	value to zero 
 * 
 * \par
 * This macro takes a single parameter of type (struct clock *), c. It sets the
 * given clock value to zero. 
 * 	\subsubsection sec4-3-3-14 Section 4.3.3.14: clock Add(): Add two clock
 * 	values together 
 * \par
 * This macro takes two parameters of type (struct clock *), c1 and c2. It adds
 * the value of the time in c2 to c1. Both clock values must be positive. 
 * 
 * 	\subsubsection sec4-3-3-15 Section 4.3.3.15: clock Sub(): Subtract two
 * 	clock values 
 * 
 * \par
 * This macro takes two parameters of type (struct clock *), c1 and c2. It
 * subtracts the value of the time in c2 from c1. The time pointed to by c2
 * should be less than the time pointed to by c1. 
 * 
 * 	\subsubsection sec4-3-3-16 Section 4.3.3.16: clock Float(): Convert a
 * 	clock time into floating point 
 * 
 * \par
 * This macro takes a single parameter of type (struct clock *), c. It
 * expresses the given clock value as a floating point number. 
 * 
 * 	\section sec4-4 Section 4.4: The rx event Package 
 * 
 * \par
 * This package maintains an event facility. An event is defined to be
 * something that happens at or after a specified clock time, unless cancelled
 * prematurely. The clock times used are those provided by the rx clock
 * facility described in Section 4.3 above. A user routine associated with an
 * event is called with the appropriate arguments when that event occurs. There
 * are some restrictions on user routines associated with such events. first,
 * this user-supplied routine should not cause process preemption. Also, the
 * event passed to the user routine is still resident on the event queue at the
 * time of invocation. The user must not remove this event explicitly (via an
 * event Cancel(), see below). Rather, the user routine may remove or schedule
 * any other event at this time. 
 * \par
 * The events recorded by this package are kept queued in order of expiration
 * time, so that the first entry in the queue corresponds to the event which is
 * the first to expire. This interface is defined by the rx event.h include
 * file. 
 * 
 * 	\subsection sec4-4-1 Section 4.4.1: struct rxevent 
 * 
 * \par
 * This structure defines the format of an Rx event record. 
 * \par
 * \b fields 
 * \n struct queue junk -The queue to which this event belongs. 
 * \n struct clock eventTime -The clock time recording when this event comes
 * due. 
 * \n int (*func)() -The user-supplied function to call upon expiration. 
 * \n char *arg -The first argument to the (*func)() function above. 
 * \n char *arg1 -The second argument to the (*func)() function above. 
 * 
 * 	\subsection sec4-4-2 Section 4.4.2: Operations 
 * 
 * \par
 * This section covers the interface routines provided for the Rx event
 * package. 
 * 
 * 	\subsubsection sec4-4-2-1 Section 4.4.2.1: rxevent Init(): Initialize
 * 	the event package 
 * 
 * \par
 * The rxevent Init() routine takes two arguments. The first, nEvents, is an
 * integer-valued parameter which specifies the number of event structures to
 * allocate at one time. This specifies the appropriate granularity of memory
 * allocation by the event package. The second parameter, scheduler, is a
 * pointer to an integer-valued function. This function is to be called when an
 * event is posted (added to the set of events managed by the package) that is
 * scheduled to expire before any other existing event. 
 * \par
 * This routine sets up future event allocation block sizes, initializes the
 * queues used to manage active and free event structures, and recalls that an
 * initialization has occurred. Thus, this function may be safely called
 * multiple times. 
 * 
 * 	\subsubsection sec4-4-2-2 Section 4.4.2.2: rxevent Post(): Schedule an
 * 	event 
 * 
 * \par
 * This function constructs a new event based on the information included in
 * its parameters and then schedules it. The rxevent Post() routine takes four
 * parameters. The first is named when, and is of type (struct clock *). It
 * specifies the clock time at which the event is to occur. The second
 * parameter is named func and is a pointer to the integer-valued function to
 * associate with the event that will be created. When the event comes due,
 * this function will be executed by the event package. The next two arguments
 * to rxevent Post() are named arg and arg1, and are both of type (char *).
 * They serve as the two arguments thath will be supplied to the func routine
 * when the event comes due. 
 * \par
 * If the given event is set to take place before any other event currently
 * posted, the scheduler routine established when the rxevent Init() routine
 * was called will be executed. This gives the application a chance to react to
 * this new event in a reasonable way. One might expect that this scheduler
 * routine will alter sleep times used by the application to make sure that it
 * executes in time to handle the new event. 
 * 
 * 	\subsubsection sec4-4-2-3 Section 4.4.2.3: rxevent Cancel 1(): Cancel
 * 	an event (internal use) 
 * 
 * \par
 * This routine removes an event from the set managed by this package. It takes
 * a single parameter named ev of type (struct rxevent *). The ev argument
 * identifies the pending event to be cancelled. 
 * \par
 * The rxevent Cancel 1() routine should never be called directly. Rather, it
 * should be accessed through the rxevent Cancel() macro, described in Section
 * 4.4.2.4 below. 
 * 
 * 	\subsubsection sec4-4-2-4 Section 4.4.2.4: rxevent Cancel(): Cancel an
 * 	event (external use) 
 * 
 * \par
 * This macro is the proper way to call the rxevent Cancel 1() routine
 * described in Section 4.4.2.3 above. Like rxevent Cancel 1(), it takes a
 * single argument. This event ptr argument is of type (struct rxevent *), and
 * identi#es the pending event to be cancelled. This macro #rst checks to see
 * if event ptr is null. If not, it calls rxevent Cancel 1() to perform the
 * real work. The event ptr argument is zeroed after the cancellation operation
 * completes. 
 * 
 * 	\subsubsection sec4-4-2-5 Section 4.4.2.4: rxevent RaiseEvents():
 * 	Initialize the event package 
 * 
 * \par
 * This function processes all events that have expired relative to the current
 * clock time maintained by the event package. Each qualifying event is removed
 * from the queue in order, and its user-supplied routine (func()) is executed
 * with the associated arguments. 
 * \par
 * The rxevent RaiseEvents() routine takes a single output parameter named
 * next, defined to be of type (struct clock *). Upon completion of rxevent
 * RaiseEvents(), the relative time to the next event due to expire is placed
 * in next. This knowledge may be used to calculate the amount of sleep time
 * before more event processing is needed. If there is no recorded event which
 * is still pending at this point, rxevent RaiseEvents() returns a zeroed clock
 * value into next. 
 * 
 * 	\subsubsection sec4-4-2-6 Section 4.4.2.6: rxevent TimeToNextEvent():
 * 	Get amount of time until the next event expires 
 * 
 * \par
 * This function returns the time between the current clock value as maintained
 * by the event package and the the next event's expiration time. This
 * information is placed in the single output argument,interval, defined to be
 * of type (struct clock *). The rxevent TimeToNextEvent() function returns
 * integer-valued quantities. If there are no scheduled events, a zero is
 * returned. If there are one or more scheduled events, a 1 is returned. If
 * zero is returned, the interval argument is not updated. 
 * 
 * 	\page chap5 Chapter 5 -- Programming Interface 
 * 
 * 	\section sec5-1 Section 5.1: Introduction 
 * 
 * \par
 * This chapter documents the API for the Rx facility. Included are
 * descriptions of all the constants, structures, exported variables, macros,
 * and interface functions available to the application programmer. This
 * interface is identical regardless of whether the application lives within
 * the unix kernel or above it. 
 * \par
 * This chapter actually provides more information than what may be strictly
 * considered the Rx API. Many objects that were intended to be opaque and for
 * Rx internal use only are also described here. The reason driving the
 * inclusion of this "extra" information is that such exported Rx interface
 * files as rx.h make these objects visible to application programmers. It is
 * prefereable to describe these objects here than to ignore them and leave
 * application programmers wondering as to their meaning. 
 * \par
 * An example application illustrating the use of this interface, showcasing
 * code from both server and client sides, appears in the following chapter. 
 * 
 * 	\section sec5-2 Section 5.2: Constants 
 * 
 * \par
 * This section covers the basic constant definitions of interest to the Rx
 * application programmer. Each subsection is devoted to describing the
 * constants falling into the following categories: 
 * \li Configuration quantities 
 * \li Waiting options 
 * \li Connection ID operations 
 * \li Connection flags 
 * \li Connection types 
 * \li Call states 
 * \li Call flags 
 * \li Call modes 
 * \li Packet header flags 
 * \li Packet sizes 
 * \li Packet types 
 * \li Packet classes 
 * \li Conditions prompting ack packets 
 * \li Ack types 
 * \li Error codes 
 * \li Debugging values 
 * \par
 * An attempt has been made to relate these constant definitions to the objects
 * or routines that utilize them. 
 * 
 * 	\subsection sec5-2-1 Section 5.2.1: Configuration Quantities 
 * 
 * \par
 * These definitions provide some basic Rx configuration parameters, including
 * the number of simultaneous calls that may be handled on a single connection,
 * lightweight thread parameters, and timeouts for various operations. 
 * 
 * \par Name 
 * RX IDLE DEAD TIME
 * \par Value 
 * 60
 * \par Description
 * Default idle dead time for connections, in seconds.
 * 
 * \par Name 
 * RX MAX SERVICES
 * \par Value 
 * 20
 * \par Description
 * The maximum number of Rx services that may be installed within one
 * application.
 * 
 * \par Name 
 * RX PROCESS MAXCALLS
 * \par Value 
 * 4
 * \par Description
 * The maximum number of asynchronous calls active simultaneously on any given
 * Rx connection.  This value must be set to a power of two.
 * 
 * \par Name 
 * RX DEFAULT STACK SIZE
 * \par Value 
 * 16,000
 * \par Description
 * Default lightweight thread stack size, measured in bytes.  This value may be
 * overridden by calling the rx_SetStackSize() macro.
 * 
 * \par Name 
 * RX PROCESS PRIORITY
 * \par Value 
 * LWP NORMAL PRIORITY
 * \par Description
 * This is the priority under which an Rx thread should run.  There should not
 * generally be any reason to change this setting.
 * 
 * \par Name 
 * RX CHALLENGE TIMEOUT
 * \par Value 
 * 2
 * \par Description
 * The number of seconds before another authentication request packet is
 * generated.
 * 
 * \par Name 
 * RX MAXACKS
 * \par Value 
 * 255
 * \par Description
 * Maximum number of individual acknowledgements that may be carried in an Rx
 * acknowledgement packet.
 * 
 * 	\subsection sec5-2-2 Section 5.2.2: Waiting Options 
 * 
 * \par
 * These definitions provide readable values indicating whether an operation
 * should block when packet buffer resources are not available. 
 * 
 * \par Name 
 * RX DONTWAIT
 * \par Value 
 * 0
 * \par Description
 * Wait until the associated operation completes.
 * 
 * \par Name 
 * RX WAIT
 * \par Value 
 * 1
 * \par Description
 * Don't wait if the associated operation would block.
 * 
 * 	\subsection sec5-2-3 Section 5.2.3: Connection ID Operations 
 * 
 * \par
 * These values assist in extracting the call channel number from a connection
 * identifier. A call channel is the index of a particular asynchronous call
 * structure within a single Rx connection. 
 * 
 * \par Name 
 * RX CIDSHIFT
 * \par Value 
 * 2
 * \par Description
 * Number of bits to right-shift to isolate a connection ID.  Must be set to
 * the log (base two) of RX MAXCALLS.
 * 
 * \par Name 
 * RX CHANNELMASK
 * \par Value 
 * (RX MAXCALLS-1)
 * \par Description
 * Mask used to isolate a call channel from a connection ID field.
 * 
 * \par Name 
 * RX CIDMASK
 * \par Value 
 * (~RX CHANNELMASK)
 * \par Description
 * Mask used to isolate the connection ID from its field, masking out the call
 * channel information.
 * 
 * 	\subsection sec5-2-4 Section 5.2.4: Connection Flags 
 * 
 * \par
 * The values defined here appear in the flags field of Rx connections, as
 * defined by the rx connection structure described in Section 5.3.2.2. 
 * 
 * \par Name 
 * RX CONN MAKECALL WAITING
 * \par Value 
 * 1
 * \par Description
 * rx MakeCall() is waiting for a channel.
 * 
 * \par Name 
 * RX CONN DESTROY ME
 * \par Value 
 * 2
 * \par Description
 * Destroy this (client) connection after its last call completes.
 * 
 * \par Name 
 * RX CONN USING PACKET CKSUM
 * \par Value 
 * 4
 * \par Description
 * This packet is using security-related check-summing (a non-zero header,
 * spare field has been seen.)
 * 
 * 	\subsection sec5-2-5 Section 5.2.5: Connection Types 
 * 
 * \par
 * Rx stores different information in its connection structures, depending on
 * whether the given connection represents the server side (the one providing
 * the service) or the client side (the one requesting the service) of the
 * protocol. The type field within the connection structure (described in
 * Section 5.3.2.2) takes on the following values to differentiate the two
 * types of connections, and identifies the fields that are active within the
 * connection structure. 
 * 
 * \par Name 
 * RX CLIENT CONNECTION
 * \par Value 
 * 0
 * \par Description
 * This is a client-side connection.
 * 
 * \par Name 
 * CONNECTION
 * \par Value 
 * 1
 * \par Description
 * This is a server-side connection.
 * 
 * 	\subsection sec5-2-6 Section 5.2.6: Call States 
 * 
 * \par
 * An Rx call on a particular connection may be in one of several states at any
 * instant in time. The following definitions identify the range of states that
 * a call may assume. 
 * 
 * \par Name 
 * RX STATE NOTINIT
 * \par Value 
 * 0
 * \par Description
 * The call structure has never been used, and is thus still completely
 * uninitialized.
 * 
 * \par Name 
 * RX STATE PRECALL
 * \par Value 
 * 1
 * \par Description
 * A call is not yet in progress, but packets have arrived for it anyway.  This
 * only applies to calls within server-side connections.
 * 
 * \par Name 
 * RX STATE ACTIVE
 * \par Value 
 * 2
 * \par Description
 * This call is fully active, having an attached lightweight thread operating
 * on its behalf.
 * 
 * \par Name 
 * RX STATE DAILY
 * \par Value 
 * 3
 * \par Description
 * The call structure is "dallying" after its lightweight thread has completed
 * its most recent call.  This is a "hot-standby" condition, where the call
 * structure preserves state from the previous call and thus optimizes the
 * arrival of further, related calls.
 * 
 * 	\subsection sec5-2-7 Section 5.2.7: Call Flags: 
 * 
 * \par
 * These values are used within the flags field of a variable declared to be of
 * type struct rx call, as described in Section 5.3.2.4. They provide
 * additional information as to the state of the given Rx call, such as the
 * type of event for which it is waiting (if any) and whether or not all
 * incoming packets have been received in support of the call. 
 * 
 * \par Name 
 * RX CALL READER WAIT
 * \par Value 
 * 1
 * \par Description
 * Reader is waiting for next packet.
 * 
 * \par Name 
 * RX CALL WAIT WINDOW ALLOC
 * \par Value 
 * 2
 * \par Description
 * Sender is waiting for a window so that it can allocate buffers.
 * 
 * \par Name 
 * RX CALL WAIT WINDOW SEND
 * \par Value 
 * 4
 * \par Description
 * Sender is waiting for a window so that it can send buffers.
 * 
 * \par Name 
 * RX CALL WAIT PACKETS
 * \par Value 
 * 8
 * \par Description
 * Sender is waiting for packet buffers.
 * 
 * \par Name 
 * RX CALL RECEIVE DONE
 * \par Value 
 * 16
 * \par Description
 * The call is waiting for a lightweight thread to be assigned to the operation
 * it has just received.
 * 
 * \par Name 
 * RX CALL RECEIVE DONE 
 * \par Value 
 * 32
 * \par Description
 * All packets have been received on this call.
 * 
 * \par Name 
 * RX CALL CLEARED
 * \par Value 
 * 64
 * \par Description
 * The receive queue has been cleared when in precall state.
 * 
 * 	\subsection sec5-2-8 Section 5.2.8: Call Modes 
 * 
 * \par
 * These values define the modes of an Rx call when it is in the RX STATE
 * ACTIVE state, having a lightweight thread assigned to it. 
 * 
 * \par Name 
 * RX MODE SENDING
 * \par Value 
 * 1
 * \par Description
 * We are sending or ready to send.
 * 
 * \par Name 
 * RX MODE RECEIVING
 * \par Value 
 * 2
 * \par Description
 * We are receiving or ready to receive.
 * 
 * \par Name 
 * RX MODE ERROR
 * \par Value 
 * 3
 * \par Description
 * Something went wrong in the current conversation.
 * 
 * \par Name 
 * RX MODE EOF
 * \par Value 
 * 4
 * \par Description
 * The server side has flushed (or the client side has read) the last reply
 * packet.
 * 
 * 	\subsection sec5-2-9 Section 5.2.9: Packet Header Flags 
 * 
 * \par
 * Rx packets carry a flag field in their headers, providing additional
 * information regarding the packet's contents. The Rx packet header's flag
 * field's bits may take the following values: 
 * 
 * \par Name 
 * RX CLIENT INITIATED
 * \par Value 
 * 1
 * \par Description
 * Signifies that a packet has been sent/received from the client side of the
 * call.
 * 
 * \par Name 
 * RX REQUEST ACK
 * \par Value 
 * 2
 * \par Description
 * The Rx calls' peer entity requests an acknowledgement.
 * 
 * \par Name 
 * RX LAST PACKET
 * \par Value 
 * 4
 * \par Description
 * This is the final packet from this side of the call.
 * 
 * \par Name 
 * RX MORE PACKETS
 * \par Value 
 * 8
 * \par Description
 * There are more packets following this, i.e., the next sequence number seen
 * by the receiver should be greater than this one, rather than a
 * retransmission of an earlier sequence number.
 * 
 * \par Name 
 * RX PRESET FLAGS
 * \par Value 
 * (RX CLIENT INITIATED | RX LAST PACKET)
 * \par Description
 * This flag is preset once per Rx packet.  It doesn't change on retransmission
 * of the packet.
 * 
 * 	\subsection sec5-3-10 Section 5.2.10: Packet Sizes 
 * 
 * \par
 * These values provide sizing information on the various regions within Rx
 * packets. These packet sections include the IP/UDP headers and bodies as well
 * Rx header and bodies. Also covered are such values as different maximum
 * packet sizes depending on whether they are targeted to peers on the same
 * local network or a more far-flung network. Note that the MTU term appearing
 * below is an abbreviation for Maximum Transmission Unit. 
 * 
 * \par Name 
 * RX IPUDP SIZE
 * \par Value 
 * 28
 * \par Description
 * The number of bytes taken up by IP/UDP headers.
 * 
 * \par Name 
 * RX MAX PACKET SIZE
 * \par Value 
 * (1500 - RX IPUDP SIZE)
 * \par Description
 * This is the Ethernet MTU minus IP and UDP header sizes.
 * 
 * \par Name 
 * RX HEADER SIZE
 * \par Value 
 * sizeof (struct rx header)
 * \par Description
 * The number of bytes in an Rx packet header.
 * 
 * \par Name 
 * RX MAX PACKET DATA SIZE
 * \par Value 
 * (RX MAX PACKET SIZE RX - HEADER SIZE)
 * \par Description
 * Maximum size in bytes of the user data in a packet.
 * 
 * \par Name 
 * RX LOCAL PACKET SIZE
 * \par Value 
 * RX MAX PACKET SIZE
 * \par Description
 * Packet size in bytes to use when being sent to a host on the same net.
 * 
 * \par Name 
 * RX REMOTE PACKET SIZE
 * \par Value 
 * (576 - RX IPUDP SIZE)
 * \par Description
 * Packet size in bytes to use when being sent to a host on a different net.
 * 
 * 	\subsection sec5-2-11 Section 5.2.11: Packet Types 
 * 
 * \par
 * The following values are used in the packetType field within a struct rx
 * packet, and define the different roles assumed by Rx packets. These roles
 * include user data packets, different flavors of acknowledgements, busies,
 * aborts, authentication challenges and responses, and debugging vehicles. 
 * 
 * \par Name 
 * RX PACKET TYPE DATA
 * \par Value 
 * 1
 * \par Description
 * A user data packet.
 * 
 * \par Name 
 * RX PACKET TYPE ACK
 * \par Value 
 * 2
 * \par Description
 * Acknowledgement packet.
 * 
 * \par Name 
 * RX PACKET TYPE BUSY
 * \par Value 
 * 3
 * \par Description
 * Busy packet.  The server-side entity cannot accept the call at the moment,
 * but the requestor is encouraged to try again later.
 * 
 * \par Name 
 * RX PACKET TYPE ABORT 
 * \par Value 
 * 4
 * \par Description
 * Abort packet. No response is needed for this packet type.
 * 
 * \par Name 
 * RX PACKET TYPE ACKALL
 * \par Value 
 * 5
 * \par Description
 * Acknowledges receipt of all packets on a call.
 * 
 * \par Name 
 * RX PACKET TYPE CHALLENGE
 * \par Value 
 * 6
 * \par Description
 * Challenge the client's identity, requesting credentials.
 * 
 * \par Name 
 * RX PACKET TYPE RESPONSE
 * \par Value 
 * 7
 * \par Description
 * Response to a RX PACKET TYPE CHALLENGE authentication challenge packet.
 * 
 * \par Name 
 * RX PACKET TYPE DEBUG
 * \par Value 
 * 8
 * \par Description
 * Request for debugging information.
 * 
 * \par Name 
 * RX N PACKET TYPES
 * \par Value 
 * 9
 * \par Description
 * The number of Rx packet types defined above.  Note that it also includes
 * packet type 0 (which is unused) in the count.
 * 
 * \par
 * The RX PACKET TYPES definition provides a mapping of the above values to
 * human-readable string names, and is exported by the rx packetTypes variable
 * catalogued in Section 5.4.9. 
 * 
 * \code 
 * {
 * 	"data", 
 * 	"ack", 
 * 	"busy", 
 * 	"abort", 
 * 	"ackall", 
 * 	"challenge", 
 * 	"response", 
 * 	"debug" 
 * } 
 * \endcode
 * 
 * 	\subsection sec5-2-12 Section 5.2.12: Packet Classes 
 * 
 * \par
 * These definitions are used internally to manage alloction of Rx packet
 * buffers according to quota classifications. Each packet belongs to one of
 * the following classes, and its buffer is derived from the corresponding
 * pool. 
 * 
 * \par Name 
 * RX PACKET CLASS RECEIVE
 * \par Value 
 * 0
 * \par Description
 * Receive packet for user data.
 * 
 * \par Name 
 * RX PACKET CLASS SEND
 * \par Value 
 * 1
 * \par Description
 * Send packet for user data.
 * 
 * \par Name 
 * RX PACKET CLASS SPECIAL
 * \par Value 
 * 2
 * \par Description
 * A special packet that does not hold user data, such as an acknowledgement or
 * authentication challenge.
 * 
 * \par Name 
 * RX N PACKET CLASSES
 * \par Value 
 * 3
 * \par Description
 * The number of Rx packet classes defined above.
 * 
 * 	\subsection sec5-2-13 Section 5.2.13: Conditions Prompting Ack Packets 
 * 
 * \par
 * Rx acknowledgement packets are constructed and sent by the protocol
 * according to the following reasons. These values appear in the Rx packet
 * header of the ack packet itself. 
 * 
 * \par Name 
 * RX ACK REQUESTED
 * \par Value 
 * 1
 * \par Description
 * The peer has explicitly requested an ack on this packet.
 * 
 * \par Name 
 * RX ACK DUPLICATE
 * \par Value 
 * 2
 * \par Description
 * A duplicate packet has been received.
 * 
 * \par Name 
 * RX ACK OUT OF SEQUENCE
 * \par Value 
 * 3
 * \par Description
 * A packet has arrived out of sequence.
 * 
 * \par Name 
 * RX ACK EXCEEDS WINDOW
 * \par Value 
 * 4
 * \par Description
 * A packet sequence number higher than maximum value allowed by the call's
 * window has been received.
 * 
 * \par Name 
 * RX ACK NOSPACE
 * \par Value 
 * 5
 * \par Description
 * No packet buffer space is available.
 * 
 * \par Name 
 * RX ACK PING
 * \par Value 
 * 6
 * \par Description
 * Acknowledgement for keep-alive purposes.
 * 
 * \par Name 
 * RX ACK PING RESPONSE
 * \par Value 
 * 7
 * \par Description
 * Response to a RX ACK PING packet.
 * 
 * \par Name 
 * RX ACK DELAY
 * \par Value 
 * 8
 * \par Description
 * An ack generated due to a period of inactivity after normal packet
 * receptions.
 * 
 * 	\subsection 5-2-14 Section 5.2.14: Acknowledgement Types 
 * 
 * \par
 * These are the set of values placed into the acks array in an Rx
 * acknowledgement packet, whose data format is defined by struct rx ackPacket.
 * These definitions are used to convey positive or negative acknowledgements
 * for a given range of packets. 
 * 
 * \par Name 
 * RX ACK TYPE NACK
 * \par Value 
 * 0
 * \par Description
 * Receiver doesn't currently have the associated packet; it may never hae been
 * received, or received and then later dropped before processing.
 * 
 * \par Name 
 * RX ACK TYPE ACK
 * \par Value 
 * 1
 * \par Description
 * Receiver has the associated packet queued, although it may later decide to
 * discard it.
 * 
 * 	\subsection sec5-2-15 Section 5.2.15: Error Codes 
 * 
 * \par
 * Rx employs error codes ranging from -1 to -64. The Rxgen stub generator may
 * use other error codes less than -64. User programs calling on Rx, on the
 * other hand, are expected to return positive error codes. A return value of
 * zero is interpreted as an indication that the given operation completed
 * successfully. 
 * 
 * \par Name 
 * RX CALL DEAD
 * \par Value 
 * -1
 * \par Description
 * A connection has been inactive past Rx's tolerance levels and has been shut
 * down.
 * 
 * \par Name 
 * RX INVALID OPERATION
 * \par Value 
 * -2
 * \par Description
 * An invalid operation has been attempted, including such protocol errors as
 * having a client-side call send data after having received the beginning of a
 * reply from its server-side peer.
 * 
 * \par Name 
 * RX CALL TIMEOUT
 * \par Value 
 * -3
 * \par Description
 * The (optional) timeout value placed on this call has been exceeded (see
 * Sections 5.5.3.4 and 5.6.5).
 * 
 * \par Name 
 * RX EOF
 * \par Value 
 * -4
 * \par Description
 * Unexpected end of data on a read operation.
 * 
 * \par Name 
 * RX PROTOCOL ERROR
 * \par Value 
 * -5
 * \par Description
 * An unspecified low-level Rx protocol error has occurred.
 * 
 * \par Name 
 * RX USER ABORT
 * \par Value 
 * -6
 * \par Description
 * A generic user abort code, used when no more specific error code needs to be
 * communicated.  For example, Rx clients employing the multicast feature (see
 * Section 1.2.8) take advantage of this error code.
 * 
 * 	\subsection sec5-2-16 Section 5.2.16: Debugging Values 
 * 
 * \par
 * Rx provides a set of data collections that convey information about its
 * internal status and performance. The following values have been defined in
 * support of this debugging and statistics-collection feature. 
 * 
 * 	\subsubsection sec5-3-16-1 Section 5.2.16.1: Version Information 
 * 
 * \par
 * Various versions of the Rx debugging/statistics interface are in existance,
 * each defining different data collections and handling certain bugs. Each Rx
 * facility is stamped with a version number of its debugging/statistics
 * interface, allowing its clients to tailor their requests to the precise data
 * collections that are supported by a particular Rx entity, and to properly
 * interpret the data formats received through this interface. All existing Rx
 * implementations should be at revision M. 
 * 
 * \par Name 
 * RX DEBUGI VERSION MINIMUM
 * \par Value 
 * 'L'
 * \par Description
 * The earliest version of Rx statistics available.
 * 
 * \par Name 
 * RX DEBUGI VERSION
 * \par Value 
 * 'M'
 * \par Description
 * The latest version of Rx statistics available.
 * 
 * \par Name 
 * RX DEBUGI VERSION W SECSTATS
 * \par Value 
 * 'L'
 * \par Description
 * Identifies the earliest version in which statistics concerning Rx security
 * objects is available.
 * 
 * \par Name 
 * RX DEBUGI VERSION W GETALLCONN
 * \par Value 
 * 'M'
 * \par Description
 * The first version that supports getting information about all current Rx
 * connections, as specified y the RX DEBUGI GETALLCONN debugging request
 * packet opcode described below.
 * 
 * \par Name 
 * RX DEBUGI VERSION W RXSTATS
 * \par Value 
 * 'M'
 * \par Description
 * The first version that supports getting all the Rx statistics in one
 * operation, as specified by the RX DEBUGI RXSTATS debugging request packet
 * opcode described below.
 * 
 * \par Name 
 * RX DEBUGI VERSION W UNALIGNED CONN
 * \par Value 
 * 'L'
 * \par Description
 * There was an alignment problem discovered when returning Rx connection
 * information in older versions of this debugging/statistics interface.  This
 * identifies the last version that exhibited this alignment problem.
 * 
 * 	\subsubsection sec5-2-16-2 Section 5.2.16.2: Opcodes 
 * 
 * \par
 * When requesting debugging/statistics information, the caller specifies one
 * of the following supported data collections: 
 * 
 * \par Name 
 * RX DEBUGI GETSTATS
 * \par Value 
 * 1
 * \par Description
 * Get basic Rx statistics.
 * 
 * \par Name 
 * RX DEBUGI GETCONN
 * \par Value 
 * 2
 * \par Description
 * Get information on all Rx connections considered "interesting" (as defined
 * below), and no others.
 * 
 * \par Name 
 * RX DEBUGI GETALLCONN
 * \par Value 
 * 3
 * \par Description
 * Get information on all existing Rx connection structures, even
 * "uninteresting" ones.
 * 
 * \par Name 
 * RX DEBUGI RXSTATS
 * \par Value 
 * 4
 * \par Description
 * Get all available Rx stats.
 * 
 * \par
 * An Rx connection is considered "interesting" if it is waiting for a call
 * channel to free up or if it has been marked for destruction. If neither is
 * true, a connection is still considered interesting if any of its call
 * channels is actively handling a call or in its preparatory pre-call state.
 * Failing all the above conditions, a connection is still tagged as
 * interesting if any of its call channels is in either of the RX MODE SENDING
 * or RX MODE RECEIVING modes, which are not allowed when the call is not
 * active. 
 * 
 * 	\subsubsection sec5-2-16-3 Section 5.2.16.3: Queuing 
 * 
 * \par
 * These two queueing-related values indicate whether packets are present on
 * the incoming and outgoing packet queues for a given Rx call. These values
 * are only used in support of debugging and statistics-gathering operations. 
 * 
 * \par Name 
 * RX OTHER IN
 * \par Value 
 * 1
 * \par Description
 * Packets available in in queue.
 * 
 * \par Name 
 * RX OTHER OUT 
 * \par Value 
 * 2
 * \par Description
 * Packets available in out queue.
 * 
 * 	\section sec5-3 Section 5.3: Structures 
 * 
 * \par
 * This section describes the major exported Rx data structures of interest to
 * application programmers. The following categories are utilized for the
 * purpose of organizing the structure descriptions: 
 * \li Security objects 
 * \li Protocol objects 
 * \li Packet formats 
 * \li Debugging and statistics 
 * \li Miscellaneous 
 * \par
 * Please note that many fields described in this section are declared to be
 * VOID. This is defined to be char, and is used to get around some compiler
 * limitations. 
 * 	\subsection sec5-3-1 Section 5.3.1: Security Objects 
 * 
 * \par
 * As explained in Section 1.2.1, Rx provides a modular, extensible security
 * model. This allows Rx applications to either use one of the built-in
 * security/authentication protocol packages or write and plug in one of their
 * own. This section examines the various structural components used by Rx to
 * support generic security and authentication modules. 
 * 
 * 	\subsubsection sec5-3-1-1 Section 5.3.1.1: struct rx securityOps 
 * 
 * \par
 * As previously described, each Rx security object must export a fixed set of
 * interface functions, providing the full set of operations defined on the
 * object. The rx securityOps structure defines the array of functions
 * comprising this interface. The Rx facility calls these routines at the
 * appropriate times, without knowing the specifics of how any particular
 * security object implements the operation. 
 * \par
 * A complete description of these interface functions, including information
 * regarding their exact purpose, parameters, and calling conventions, may be
 * found in Section 5.5.7. 
 * \par
 * \b fields 
 * \li int (*op Close)() - React to the disposal of a security object. 
 * \li int (*op NewConnection)() - Invoked each time a new Rx connection
 * utilizing the associated security object is created. 
 * \li int (*op PreparePacket)() - Invoked each time an outgoing Rx packet is
 * created and sent on a connection using the given security object. 
 * \li int (*op SendPacket)() - Called each time a packet belonging to a call
 * in a connection using the security object is physically transmitted. 
 * \li int (*op CheckAuthentication)() - This function is executed each time it
 * is necessary to check whether authenticated calls are being perfomed on a
 * connection using the associated security object. 
 * \li int (*op CreateChallenge)() - Invoked each time a server-side challenge
 * event is created by Rx, namely when the identity of the principal associated
 * with the peer process must be determined. 
 * \li int (*op GetChallenge)() - Called each time a client-side packet is
 * constructed in response to an authentication challenge. 
 * \li int (*op GetResponse)() - Executed each time a response to a challenge
 * event must be received on the server side of a connection. 
 * \li int (*op CheckResponse)() - Invoked each time a response to an
 * authentication has been received, validating the response and pulling out
 * the required authentication information. 
 * \li int (*op CheckPacket) () - Invoked each time an Rx packet has been
 * received, making sure that the packet is properly formatted and that it
 * hasn't been altered. 
 * \li int (*op DestroyConnection)() - Called each time an Rx connection
 * employing the given security object is destroyed. 
 * \li int (*op GetStats)() - Executed each time a request for statistics on
 * the given security object has been received. 
 * \li int (*op Spare1)()-int (*op Spare3)() - Three spare function slots,
 * reserved for future use. 
 * 
 * 	\subsubsection sec5-3-1-2 Section 5.2.1.2: struct rx securityClass 
 * 
 * \par
 * Variables of type struct rx securityClass are used to represent
 * instantiations of a particular security model employed by Rx. It consists of
 * a pointer to the set of interface operations implementing the given security
 * object, along with a pointer to private storage as necessary to support its
 * operations. These security objects are also reference-counted, tracking the
 * number of Rx connections in existance that use the given security object. If
 * the reference count drops to zero, the security module may garbage-collect
 * the space taken by the unused security object. 
 * \par
 * \b fields 
 * \li struct rx securityOps *ops - Pointer to the array of interface functions
 * for the security object. 
 * \li VOID *privateData - Pointer to a region of storage used by the security
 * object to support its operations. 
 * \li int refCount - A reference count on the security object, tracking the
 * number of Rx connections employing this model. 
 * 
 * 	\subsubsection sec5-3-1-3 Section 5.3.1.3: struct rx
 * 	securityObjectStats 
 * 
 * \par
 * This structure is used to report characteristics for an instantiation of a
 * security object on a particular Rx connection, as well as performance
 * figures for that object. It is used by the debugging portions of the Rx
 * package. Every security object defines and manages fields such as level and
 * flags differently. 
 * \par
 * \b fields 
 * \li char type - The type of security object being implemented. Existing
 * values are: 
 * \li 0: The null security package. 
 * \li 1: An obsolete Kerberos-like security object. 
 * \li 2: The rxkad discipline (see Chapter 3). 
 * \li char level - The level at which encryption is utilized. 
 * \li char sparec[10] - Used solely for alignment purposes. 
 * \li long flags - Status flags regarding aspects of the connection relating
 * to the security object. 
 * \li u long expires - Absolute time when the authentication information
 * cached by the given connection expires. A value of zero indicates that the
 * associated authentication information is valid for all time. 
 * \li u long packetsReceived - Number of packets received on this particular
 * connection, and thus the number of incoming packets handled by the
 * associated security object. 
 * \li u long packetsSent - Number of packets sent on this particular
 * connection, and thus the number of outgoing packets handled by the
 * associated security object. 
 * \li u long bytesReceived - Overall number of "payload" bytes received (i.e.,
 * packet bytes not associated with IP headers, UDP headers, and the security
 * module's own header and trailer regions) on this connection. 
 * \li u long bytesSent - Overall number of "payload" bytes sent (i.e., packet
 * bytes not associated with IP headers, UDP headers, and the security module's
 * own header and trailer regions) on this connection. 
 * \li short spares[4] - Several shortword spares, reserved for future use. 
 * \li long sparel[8] - Several longword spares, reserved for future use. 
 * 
 * 	\subsection sec5-3-2 Section 5.3.2: Protocol Objects 
 * 
 * \par
 * The structures describing the main abstractions and entities provided by Rx,
 * namely services, peers, connections and calls are covered in this section. 
 * 
 * 	\subsubsection sec5-3-2-1 Section 5.3.2.1: struct rx service 
 * 
 * \par
 * An Rx-based server exports services, or specific RPC interfaces that
 * accomplish certain tasks. Services are identified by (host-address,
 * UDP-port, serviceID) triples. An Rx service is installed and initialized on
 * a given host through the use of the rx NewService() routine (See Section
 * 5.6.3). Incoming calls are stamped with the Rx service type, and must match
 * an installed service to be accepted. Internally, Rx services also carry
 * string names for purposes of identification. These strings are useful to
 * remote debugging and statistics-gathering programs. The use of a service ID
 * allows a single server process to export multiple, independently-specified
 * Rx RPC services. 
 * \par
 * Each Rx service contains one or more security classes, as implemented by
 * individual security objects. These security objects implement end-to-end
 * security protocols. Individual peer-to-peer connections established on
 * behalf of an Rx service will select exactly one of the supported security
 * objects to define the authentication procedures followed by all calls
 * associated with the connection. Applications are not limited to using only
 * the core set of built-in security objects offered by Rx. They are free to
 * define their own security objects in order to execute the specific protocols
 * they require. 
 * \par
 * It is possible to specify both the minimum and maximum number of lightweight
 * processes available to handle simultaneous calls directed to an Rx service.
 * In addition, certain procedures may be registered with the service and
 * called at set times in the course of handling an RPC request. 
 * \par
 * \b fields 
 * \li u short serviceId - The associated service number. 
 * \li u short servicePort - The chosen UDP port for this service. 
 * \li char *serviceName - The human-readable service name, expressed as a
 * character 
 * \li string. osi socket socket - The socket structure or file descriptor used
 * by this service. 
 * \li u short nSecurityObjects - The number of entries in the array of
 * supported security objects. 
 * \li struct rx securityClass **securityObjects - The array of pointers to the
 * ser
 * vice's security class objects. 
 * \li long (*executeRequestProc)() - A pointer to the routine to call when an
 * RPC request is received for this service. 
 * \li VOID (*destroyConnProc)() - A pointer to the routine to call when one of
 * the server-side connections associated with this service is destroyed. 
 * \li VOID (*newConnProc)() - A pointer to the routine to call when a
 * server-side connection associated with this service is created. 
 * \li VOID (*beforeProc)() - A pointer to the routine to call before an
 * individual RPC call on one of this service's connections is executed. 
 * \li VOID (*afterProc)() - A pointer to the routine to call after an
 * individual RPC call on one of this service's connections is executed. 
 * \li short nRequestsRunning - The number of simultaneous RPC calls currently
 * in progress for this service. 
 * \li short maxProcs - This field has two meanings. first, maxProcs limits the
 * total number of requests that may execute in parallel for any one service.
 * It also guarantees that this many requests may be handled in parallel if
 * there are no active calls for any other service. 
 * \li short minProcs - The minimum number of lightweight threads (hence
 * requests) guaranteed to be simultaneously executable. 
 * \li short connDeadTime - The number of seconds until a client of this
 * service will be declared to be dead, if it is not responding to the RPC
 * protocol. 
 * \li short idleDeadTime - The number of seconds a server-side connection for
 * this service will wait for packet I/O to resume after a quiescent period
 * before the connection is marked as dead. 
 * 
 * 	\subsubsection sec5-3-2-2 Section 5.3.2.2: struct rx connection 
 * 
 * \par
 * An Rx connection represents an authenticated communication path, allowing
 * multiple asynchronous conversations (calls). Each connection is identified
 * by a connection ID. The low-order bits of the connection ID are reserved so
 * they may be stamped with the index of a particular call channel. With up to
 * RX MAXCALLS concurrent calls (set to 4 in this implementation), the bottom
 * two bits are set aside for this purpose. The connection ID is not sufficient
 * by itself to uniquely identify an Rx connection. Should a client crash and
 * restart, it may reuse a connection ID, causing inconsistent results. In
 * addition to the connection ID, the epoch, or start time for the client side
 * of the connection, is used to identify a connection. Should the above
 * scenario occur, a different epoch value will be chosen by the client,
 * differentiating this incarnation from the orphaned connection record on the
 * server side. 
 * \par
 * Each connection is associated with a parent service, which defines a set of
 * supported security models. At creation time, an Rx connection selects the
 * particular security protocol it will implement, referencing the associated
 * service. The connection structure maintains state about the individual calls
 * being simultaneously handled. 
 * \par
 * \b fields 
 * \li struct rx connection *next - Used for internal queueing. 
 * \li struct rx peer *peer - Pointer to the connection's peer information (see
 * below). 
 * \li u long epoch - Process start time of the client side of the connection. 
 * \li u long cid - Connection identifier. The call channel (i.e., the index
 * into the connection's array of call structures) may appear in the bottom
 * bits. 
 * \li VOID *rock - Pointer to an arbitrary region of memory in support of the
 * connection's operation. The contents of this area are opaque to the Rx
 * facility in general, but are understood by any special routines used by this
 * connection. 
 * \li struct rx call *call[RX MAXCALLS] - Pointer to the call channel
 * structures, describing up to RX MAXCALLS concurrent calls on this
 * connection. 
 * \li u long callNumber[RX MAXCALLS] - The set of current call numbers on each
 * of the call channels. 
 * \li int timeout - Obsolete; no longer used. 
 * \li u char flags - Various states of the connection; see Section 5.2.4 for
 * individual bit definitions. 
 * \li u char type - Whether the connection is a server-side or client-side
 * one. See Section 5.2.5 for individual bit definitions. 
 * \li u short serviceId - The service ID that should be stamped on requests.
 * This field is only used by client-side instances of connection structures. 
 * \li struct rx service *service - A pointer to the service structure
 * associated with this connection. This field is only used by server-side
 * instances of connection structures. 
 * \li u long serial - Serial number of the next outgoing packet associated
 * with this connection. 
 * \li u long lastSerial - Serial number of the last packet received in
 * association with this connection. This field is used in computing packet
 * skew. 
 * \li u short secondsUntilDead - Maximum numer of seconds of silence that
 * should be tolerated from the connection's peer before calls will be
 * terminated with an RX CALL DEAD error. 
 * \li u char secondsUntilPing - The number of seconds between "pings"
 * (keep-alive probes) when at least one call is active on this connection. 
 * \li u char securityIndex - The index of the security object being used by
 * this connection. This number selects a slot in the security class array
 * maintained by the service associated with the connection. 
 * \li long error - Records the latest error code for calls occurring on this
 * connection. 
 * \li struct rx securityClass *securityObject - A pointer to the security
 * object used by this connection. This should coincide with the slot value
 * chosen by the securityIndex field described above. 
 * \li VOID *securityData - A pointer to a region dedicated to hosting any
 * storage required by the security object being used by this connection. 
 * \li u short securityHeaderSize - The length in bytes of the portion of the
 * packet header before the user's data that contains the security module's
 * information. 
 * \li u short securityMaxTrailerSize - The length in bytes of the packet
 * trailer, appearing after the user's data, as mandated by the connection's
 * security module. 
 * \li struct rxevent *challengeEvent -Pointer to an event that is scheduled
 * when the server side of the connection is challenging the client to
 * authenticate itself. 
 * \li int lastSendTime - The last time a packet was sent on this connection. 
 * \li long maxSerial - The largest serial number seen on incoming packets. 
 * \li u short hardDeadTime - The maximum number of seconds that any call on
 * this connection may execute. This serves to throttle runaway calls. 
 * 
 * 	\subsubsection sec5-3-2-3 Section 5.3.2.3: struct rx peer 
 * 
 * \par
 * For each connection, Rx maintains information describing the entity, or
 * peer, on the other side of the wire. A peer is identified by a (host,
 * UDP-port) pair. Included in the information kept on this remote
 * communication endpoint are such network parameters as the maximum packet
 * size supported by the host, current readings on round trip time to
 * retransmission delays, and packet skew (see Section 1.2.7). There are also
 * congestion control fields, ranging from descriptions of the maximum number
 * of packets that may be sent to the peer without pausing and retransmission
 * statistics. Peer structures are shared between connections whenever
 * possible, and hence are reference-counted. A peer object may be
 * garbage-collected if it is not actively referenced by any connection
 * structure and a sufficient period of time has lapsed since the reference
 * count dropped to zero. 
 * \par
 * \b fields 
 * \li struct rx peer *next - Use to access internal lists. 
 * \li u long host - Remote IP address, in network byte order 
 * \li u short port - Remote UDP port, in network byte order 
 * \li short packetSize - Maximum packet size for this host, if known. 
 * \li u long idleWhen - When the refCount reference count field (see below)
 * went to zero. 
 * \li short refCount - Reference count for this structure 
 * \li u char burstSize - Reinitialization size for the burst field (below). 
 * \li u char burst - Number of packets that can be transmitted immediately
 * without pausing. 
 * \li struct clock burstWait - Time delay until new burst aimed at this peer
 * is allowed. 
 * \li struct queue congestionQueue - Queue of RPC call descriptors that are
 * waiting for a non-zero burst value. 
 * \li int rtt - Round trip time to the peer, measured in milliseconds. 
 * \li struct clock timeout - Current retransmission delay to the peer. 
 * \li int nSent - Total number of distinct data packets sent, not including
 * retransmissions. 
 * \li int reSends - Total number of retransmissions for this peer since the
 * peer structure instance was created. 
 * \li u long inPacketSkew - Maximum skew on incoming packets (see Section
 * 1.2.7) 
 * \li u long outPacketSkew - Peer-reported maximum skew on outgoing packets
 * (see Section 1.2.7). 
 * 
 * 	\subsubsection sec5-3-2-4 Section 5.3.2.4: struct rx call 
 * 
 * \par
 * This structure records the state of an active call proceeding on a given Rx
 * connection. As described above, each connection may have up to RX MAXCALLS
 * calls active at any one instant, and thus each connection maintains an array
 * of RX MAXCALLS rx call structures. The information contained here is
 * specific to the given call; "permanent" call state, such as the call number,
 * is maintained in the connection structure itself. 
 * \par
 * \b fields 
 * \li struct queue queue item header - Queueing information for this
 * structure. 
 * \li struct queue tq - Queue of outgoing ("transmit") packets. 
 * \li struct queue rq - Queue of incoming ("receive") packets. 
 * \li char *bufPtr - Pointer to the next byte to fill or read in the call's
 * current packet, depending on whether it is being transmitted or received. 
 * \li u short nLeft - Number of bytes left to read in the first packet in the
 * reception queue (see field rq). 
 * \li u short nFree - Number of bytes still free in the last packet in the
 * transmission queue (see field tq). 
 * \li struct rx packet *currentPacket - Pointer to the current packet being
 * assembled or read. 
 * \li struct rx connection *conn - Pointer to the parent connection for this
 * call. 
 * \li u long *callNumber - Pointer to call number field within the call's
 * current packet. 
 * \li u char channel - Index within the parent connection's call array that
 * describes this call. 
 * \li u char dummy1, dummy2 - These are spare fields, reserved for future use. 
 * \li u char state - Current call state. The associated bit definitions appear
 * in Section 5.2.7. 
 * \li u char mode - Current mode of a call that is in RX STATE ACTIVE state.
 * The associated bit definitions appear in Section 5.2.8. 
 * \li u char flags - Flags pertaining to the state of the given call. The
 * associated bit definitions appear in Section 5.2.7. 
 * \li u char localStatus - Local user status information, sent out of band.
 * This field is currently not in use, set to zero. 
 * \li u char remoteStatus - Remote user status information, received out of
 * band.  This field is currently not in use, set to zero. 
 * \li long error - Error condition for this call. 
 * \li u long timeout - High level timeout for this call 
 * \li u long rnext - Next packet sequence number expected to be received. 
 * \li u long rprev - Sequence number of the previous packet received. This
 * number is used to decide the proper sequence number for the next packet to
 * arrive, and may be used to generate a negative acknowledgement. 
 * \li u long rwind - Width of the packet receive window for this call. The
 * peer must not send packets with sequence numbers greater than or equal to
 * rnext + rwind. 
 * \li u long tfirst - Sequence number of the first unacknowledged transmit
 * packet for this call. 
 * \li u long tnext - Next sequence number to use for an outgoing packet. 
 * \li u long twind - Width of the packet transmit window for this call. Rx
 * cannot assign a sequence number to an outgoing packet greater than or equal
 * to tfirst + twind. 
 * \li struct rxevent *resendEvent - Pointer to a pending retransmission event,
 * if any. 
 * \li struct rxevent *timeoutEvent - Pointer to a pending timeout event, if
 * any. 
 * \li struct rxevent *keepAliveEvent - Pointer to a pending keep-alive event,
 * if this is an active call. 
 * \li struct rxevent *delayedAckEvent - Pointer to a pending delayed
 * acknowledgement packet event, if any. Transmission of a delayed
 * acknowledgement packet is scheduled after all outgoing packets for a call
 * have been sent. If neither a reply nor a new call are received by the time
 * the delayedAckEvent activates, the ack packet will be sent. 
 * \li int lastSendTime - Last time a packet was sent for this call. 
 * \li int lastReceiveTime - Last time a packet was received for this call. 
 * \li VOID (*arrivalProc)() - Pointer to the procedure to call when reply is
 * received. 
 * \li VOID *arrivalProcHandle - Pointer to the handle to pass to the
 * arrivalProc as its first argument. 
 * \li VOID *arrivalProcArg - Pointer to an additional argument to pass to the
 * given arrivalProc. 
 * \li u long lastAcked - Sequence number of the last packet "hard-acked" by
 * the receiver. A packet is considered to be hard-acked if an acknowledgement
 * is generated after the reader has processed it. The Rx facility may
 * sometimes "soft-ack" a windowfull of packets before they have been picked up
 * by the receiver. 
 * \li u long startTime - The time this call started running. 
 * \li u long startWait - The time that a server began waiting for input data
 * or send quota. 
 * 
 * 	\subsection sec5-3-3 Section 5.3.3: Packet Formats 
 * 
 * \par
 * The following sections cover the different data formats employed by the
 * suite of Rx packet types, as enumerated in Section 5.2.11. A description of
 * the most commonly-employed Rx packet header appears first, immediately
 * followed by a description of the generic packet container and descriptor.
 * The formats for Rx acknowledgement packets and debugging/statistics packets
 * are also examined. 
 * 
 * 	\subsubsection sec5-3-3-1 Section 5.3.3.1: struct rx header 
 * 
 * \par
 * Every Rx packet has its own header region, physically located after the
 * leading IP/UDP headers. This header contains connection, call, security, and
 * sequencing information. Along with a type identifier, these fields allow the
 * receiver to properly interpret the packet. In addition, every client relates
 * its "epoch", or Rx incarnation date, in each packet. This assists in
 * identifying protocol problems arising from reuse of connection identifiers
 * due to a client restart. Also included in the header is a byte of
 * user-defined status information, allowing out-of-band channel of
 * communication for the higher-level application using Rx as a transport
 * mechanism. 
 * \par
 * \b fields 
 * \li u long epoch - Birth time of the client Rx facility. 
 * \li u long cid - Connection identifier, as defined by the client. The last
 * RX CIDSHIFT bits in the cid field identify which of the server-side RX
 * MAXCALLS call channels is to receive the packet. 
 * \li u long callNumber - The current call number on the chosen call channel. 
 * \li u long seq - Sequence number of this packet. Sequence numbers start with
 * 0 for each new Rx call. 
 * \li u long serial - This packet's serial number. A new serial number is
 * stamped on each packet transmitted (or retransmitted). 
 * \li u char type - What type of Rx packet this is; see Section 5.2.11 for the
 * list of legal definitions. 
 * \li u char flags - Flags describing this packet; see Section 5.2.9 for the
 * list of legal settings. 
 * \li u char userStatus - User-defined status information, uninterpreted by
 * the Rx facility itself. This field may be easily set or retrieved from Rx
 * packets via calls to the rx GetLocalStatus(), rx SetLocalStatus(), rx
 * GetRemoteStatus(), and rx SetRemoteStatus() macros. 
 * \li u char securityIndex - Index in the associated server-side service class
 * of the security object used by this call. 
 * \li u short serviceId - The server-provided service ID to which this packet
 * is directed. 
 * \li u short spare - This field was originally a true spare, but is now used
 * by the built-in rxkad security module for packet header checksums. See the
 * descriptions of the related rx IsUsingPktChecksum(), rx GetPacketCksum(),
 * and rx SetPacketCksum() macros. 
 * 
 * 	\subsubsection sec5-3-3-2 Section 5.3.3.2: struct rx packet 
 * 
 * \par
 * This structure is used to describe an Rx packet, and includes the wire
 * version of the packet contents, where all fields exist in network byte
 * order. It also includes acknowledgement, length, type, and queueing
 * information. 
 * \par
 * \b fields 
 * \li struct queue queueItemHeader - field used for internal queueing. 
 * \li u char acked - If non-zero, this field indicates that this packet has
 * been tentatively (soft-) acknowledged. Thus, the packet has been accepted by
 * the rx peer entity on the other side of the connection, but has not yet
 * necessarily been passed to the true reader. The sender is not free to throw
 * the packet away, as it might still get dropped by the peer before it is
 * delivered to its destination process. 
 * \li short length - Length in bytes of the user data section. 
 * \li u char packetType - The type of Rx packet described by this record. The
 * set of legal choices is available in Section 5.2.11. 
 * \li struct clock retryTime - The time when this packet should be
 * retransmitted next. 
 * \li struct clock timeSent - The last time this packet was transmitted. 
 * \li struct rx header header - A copy of the internal Rx packet header. 
 * \li wire - The text of the packet as it appears on the wire. This structure
 * has the following sub-fields: 
 * 	\li u long head[RX HEADER SIZE/sizeof(long)] The wire-level contents of
 * 	IP, UDP, and Rx headers. 
 * 	\li u long data[RX MAX PACKET DATA SIZE/sizeof(long)] The wire form of
 * 	the packet's "payload", namely the user data it carries. 
 * 
 * 	\subsubsection sec5-3-3-3 Section 5.3.3.3: struct rx ackPacket 
 * 
 * \par
 * This is the format for the data portion of an Rx acknowledgement packet,
 * used to inform a peer entity performing packet transmissions that a subset
 * of its packets has been properly received. 
 * \par
 * \b fields 
 * \li u short bufferSpace - Number of packet buffers available. Specifically,
 * the number of packet buffers that the ack packet's sender is willing to
 * provide for data on this or subsequent calls. This number does not have to
 * fully accurate; it is acceptable for the sender to provide an estimate. 
 * \li u short maxSkew - The maximum difference seen between the serial number
 * of the packet being acknowledged and highest packet yet received. This is an
 * indication of the degree to which packets are arriving out of order at the
 * receiver. 
 * \li u long firstPacket - The serial number of the first packet in the list
 * of acknowledged packets, as represented by the acks field below. 
 * \li u long previousPacket - The previous packet serial number received. 
 * \li u long serial - The serial number of the packet prompted the
 * acknowledgement. 
 * \li u char reason - The reason given for the acknowledgement; legal values
 * for this field are described in Section 5.2.13. 
 * \li u char nAcks - Number of acknowledgements active in the acks array
 * immediately following. 
 * \li u char acks[RX MAXACKS] - Up to RX MAXACKS packet acknowledgements. The
 * legal values for each slot in the acks array are described in Section
 * 5.2.14. Basically, these fields indicate either positive or negative
 * acknowledgements. 
 * 
 * \par
 * All packets with serial numbers prior to firstPacket are implicitly
 * acknowledged by this packet, indicating that they have been fully processed
 * by the receiver. Thus, the sender need no longer be concerned about them,
 * and may release all of the resources that they occupy. Packets with serial
 * numbers firstPacket + nAcks and higher are not acknowledged by this ack
 * packet. Packets with serial numbers in the range [firstPacket, firstPacket +
 * nAcks) are explicitly acknowledged, yet their sender-side resources must not
 * yet be released, as there is yet no guarantee that the receiver will not
 * throw them away before they can be processed there. 
 * \par
 * There are some details of importance to be noted. For one, receiving a
 * positive acknowlegement via the acks array does not imply that the
 * associated packet is immune from being dropped before it is read and
 * processed by the receiving entity. It does, however, imply that the sender
 * should stop retransmitting the packet until further notice. Also, arrival of
 * an ack packet should prompt the transmitter to immediately retransmit all
 * packets it holds that have not been explicitly acknowledged and that were
 * last transmitted with a serial number less than the highest serial number
 * acknowledged by the acks array. 
 * Note: The fields in this structure are always kept in wire format, namely in
 * network byte order. 
 * 
 * 	\subsection sec5-3-4 Section 5.3.4: Debugging and Statistics 
 * 
 * \par
 * The following structures are defined in support of the debugging and
 * statistics-gathering interfaces provided by Rx. 
 * 
 * 	\subsubsection sec5-3-4-1 Section 5.3.4.1: struct rx stats 
 * 
 * \par
 * This structure maintains Rx statistics, and is gathered by such tools as the
 * rxdebug program. It must be possible for all of the fields placed in this
 * structure to be successfully converted from their on-wire network byte
 * orderings to the host-specific ordering. 
 * \par
 * \b fields 
 * \li int packetRequests - Number of packet allocation requests processed. 
 * \li int noPackets[RX N PACKET CLASSES] - Number of failed packet requests,
 * organized per allocation class. 
 * \li int socketGreedy - Whether the SO GREEDY setting succeeded for the Rx
 * socket. 
 * \li int bogusPacketOnRead - Number of inappropriately short packets
 * received. 
 * \li int bogusHost - Contains the host address from the last bogus packet
 * received. 
 * \li int noPacketOnRead - Number of attempts to read a packet off the wire
 * when there was actually no packet there. 
 * \li int noPacketBuffersOnRead - Number of dropped data packets due to lack
 * of packet buffers. 
 * \li int selects - Number of selects waiting for a packet arrival or a
 * timeout. 
 * \li int sendSelects - Number of selects forced when sending packets. 
 * \li int packetsRead[RX N PACKET TYPES] - Total number of packets read,
 * classified by type. 
 * \li int dataPacketsRead - Number of unique data packets read off the wire. 
 * \li int ackPacketsRead - Number of ack packets read. 
 * \li int dupPacketsRead - Number of duplicate data packets read. 
 * \li int spuriousPacketsRead - Number of inappropriate data packets. 
 * \li int packetsSent[RX N PACKET TYPES] - Number of packet transmissions,
 * broken down by packet type. 
 * \li int ackPacketsSent - Number of ack packets sent. 
 * \li int pingPacketsSent - Number of ping packets sent. 
 * \li int abortPacketsSent - Number of abort packets sent. 
 * \li int busyPacketsSent - Number of busy packets sent. 
 * \li int dataPacketsSent - Number of unique data packets sent. 
 * \li int dataPacketsReSent - Number of retransmissions. 
 * \li int dataPacketsPushed - Number of retransmissions pushed early by a
 * negative acknowledgement. 
 * \li int ignoreAckedPacket - Number of packets not retransmitted because they
 * have already been acked. 
 * \li int struct clock totalRtt - Total round trip time measured for packets,
 * used to compute average time figure. 
 * \li struct clock minRtt - Minimum round trip time measured for packets. 
 * struct clock maxRtt - Maximum round trip time measured for packets. 
 * \li int nRttSamples - Number of round trip samples. 
 * \li int nServerConns - Number of server connections. 
 * \li int nClientConns - Number of client connections. 
 * \li int nPeerStructs - Number of peer structures. 
 * \li int nCallStructs - Number of call structures physically allocated (using
 * the internal storage allocator routine). 
 * \li int nFreeCallStructs - Number of call structures which were pulled from
 * the free queue, thus avoiding a call to the internal storage allocator
 * routine. 
 * \li int spares[10] - Ten integer spare fields, reserved for future use. 
 * 
 * 	\subsubsection sec5-3-4-2 Section 5.3.4.2: struct rx debugIn 
 * 
 * \par
 * This structure defines the data format for a packet requesting one of the
 * statistics collections maintained by Rx. 
 * \par
 * \b fields 
 * \li long type - The specific data collection that the caller desires. Legal
 * settings for this field are described in Section 5.2.16.2. 
 * \li long index - This field is only used when gathering information on Rx
 * connections. Choose the index of the server-side connection record of which
 * we are inquiring. This field may be used as an iterator, stepping through
 * all the connection records, one per debugging request, until they have all
 * been examined. 
 * 
 * 	\subsubsection sec5-3-4-3 Section 5.3.4.3: struct rx debugStats 
 * 
 * \par
 * This structure describes the data format for a reply to an RX DEBUGI
 * GETSTATS debugging request packet. These fields are given values indicating
 * the current state of the Rx facility. 
 * \par
 * \b fields 
 * \li long nFreePackets - Number of packet buffers currently assigned to the
 * free pool. 
 * \li long packetReclaims - Currently unused. 
 * \li long callsExecuted - Number of calls executed since the Rx facility was
 * initialized. 
 * \li char waitingForPackets - Is Rx currently blocked waiting for a packet
 * buffer to come free? 
 * \li char usedFDs - If the Rx facility is executing in the kernel, return the
 * number of unix file descriptors in use. This number is not directly related
 * to the Rx package, but rather describes the state of the machine on which Rx
 * is running. 
 * \li char version - Version number of the debugging package. 
 * \li char spare1[1] - Byte spare, reserved for future use. 
 * \li long spare2[10] - Set of 10 longword spares, reserved for future use. 
 * 
 * 	\subsubsection sec5-3-4-4 Section 5.3.4.4: struct rx debugConn 
 * 
 * \par
 * This structure defines the data format returned when a caller requests
 * information concerning an Rx connection. Thus, rx debugConn defines the
 * external packaging of interest to external parties. Most of these fields are
 * set from the rx connection structure, as defined in Section 5.3.2.2, and
 * others are obtained by indirecting through such objects as the connection's
 * peer and call structures. 
 * \par
 * \b fields 
 * \li long host - Address of the host identified by the connection's peer
 * structure. 
 * \li long cid - The connection ID. 
 * \li long serial - The serial number of the next outgoing packet associated
 * with this connection. 
 * \li long callNumber[RX MAXCALLS] - The current call numbers for the
 * individual call channels on this connection. 
 * \li long error - Records the latest error code for calls occurring on this
 * connection. 
 * \li short port - UDP port associated with the connection's peer. 
 * \li char flags - State of the connection; see Section 5.2.4 for individual
 * bit definitions. 
 * \li char type - Whether the connection is a server-side or client-side one.
 * See Section 5.2.5 for individual bit definitions. 
 * \li char securityIndex - Index in the associated server-side service class
 * of the security object being used by this call. 
 * \li char sparec[3] - Used to force alignment for later fields. 
 * \li char callState[RX MAXCALLS] - Current call state on each call channel.
 * The associated bit definitions appear in Section 5.2.7. 
 * \li char callMode[RX MAXCALLS] - Current mode of all call channels that are
 * in RX STATE ACTIVE state. The associated bit definitions appear in Section
 * 5.2.8. 
 * \li char callFlags[RX MAXCALLS] - Flags pertaining to the state of each of
 * the connection's call channels. The associated bit definitions appear in
 * Section 5.2.7. 
 * \li char callOther[RX MAXCALLS] - Flag field for each call channel, where
 * the presence of the RX OTHER IN flag indicates that there are packets
 * present on the given call's reception queue, and the RX OTHER OUT flag
 * indicates the presence of packets on the transmission queue. 
 * \li struct rx securityObjectStats secStats - The contents of the statistics
 * related to the security object selected by the securityIndex field, if any. 
 * \li long epoch - The connection's client-side incarnation time. 
 * \li long sparel[10] - A set of 10 longword fields, reserved for future use. 
 * 
 * 	\subsubsection sec5-3-4-5 Section 5.3.4.5: struct rx debugConn vL 
 * 
 * \par
 * This structure is identical to rx debugConn defined above, except for the
 * fact that it is missing the sparec field. This sparec field is used in rx
 * debugConn to fix an alignment problem that was discovered in version L of
 * the debugging/statistics interface (hence the trailing "tt vL tag in the
 * structure name). This alignment problem is fixed in version M, which
 * utilizes and exports the rx debugConn structure exclusively. Information
 * regarding the range of version-numbering values for the Rx
 * debugging/statistics interface may be found in Section 5.2.16.1. 
 * 	\section sec5-4 Section 5.4: Exported Variables 
 * 
 * \par
 * This section describes the set of variables that the Rx facility exports to
 * its applications. Some of these variables have macros defined for the sole
 * purpose of providing the caller with a convenient way to manipulate them.
 * Note that some of these exported variables are never meant to be altered by
 * application code (e.g., rx nPackets). 
 * 
 * 	\subsection sec5-4-1 Section 5.4.1: rx connDeadTime 
 * 
 * \par
 * This integer-valued variable determines the maximum number of seconds that a
 * connection may remain completely inactive, without receiving packets of any
 * kind, before it is eligible for garbage collection. Its initial value is 12
 * seconds. The rx SetRxDeadTime macro sets the value of this variable. 
 * 
 * 	\subsection sec5-4-2 Section 5.4.2: rx idleConnectionTime 
 * 
 * \par
 * This integer-valued variable determines the maximum number of seconds that a
 * server connection may "idle" (i.e., not have any active calls and otherwise
 * not have sent a packet) before becoming eligible for garbage collection. Its
 * initial value is 60 seconds. 
 * 
 * 	\subsection sec5-4-3 Section 5.4.3: rx idlePeerTime 
 * 
 * \par
 * This integer-valued variable determines the maximum number of seconds that
 * an Rx peer structure is allowed to exist without any connection structures
 * referencing it before becoming eligible for garbage collection. Its initial
 * value is 60 seconds. 
 * 
 * 	\subsection sec5-4-4 Section 5.4.4: rx extraQuota 
 * 
 * \par
 * This integer-valued variable is part of the Rx packet quota system (see
 * Section 1.2.6), which is used to avoid system deadlock. This ensures that
 * each server-side thread has a minimum number of packets at its disposal,
 * allowing it to continue making progress on active calls. This particular
 * variable records how many extra data packets a user has requested be
 * allocated. Its initial value is 0. 
 * 
 * 	\subsection sec5-4-5 Section 5.4.5: rx extraPackets 
 * 
 * \par
 * This integer-valued variable records how many additional packet buffers are
 * to be created for each Rx server thread. The caller, upon setting this
 * variable, is applying some application-specific knowledge of the level of
 * network activity expected. The rx extraPackets variable is used to compute
 * the overall number of packet buffers to reserve per server thread, namely rx
 * nPackets, described below. The initial value is 32 packets. 
 * 
 * 	\subsection sec5-4-6 Section 5.4.6: rx nPackets 
 * 
 * \par
 * This integer-valued variable records the total number of packet buffers to
 * be allocated per Rx server thread. It takes into account the quota packet
 * buffers and the extra buffers requested by the caller, if any. 
 * \note This variable should never be set directly; the Rx facility itself
 * computes its value. Setting it incorrectly may result in the service
 * becoming deadlocked due to insufficient resources. Callers wishing to
 * allocate more packet buffers to their server threads should indicate that
 * desire by setting the rx extraPackets variable described above. 
 * 
 * 	\subsection sec5-4-7 Section 5.4.7: rx nFreePackets 
 * 
 * \par
 * This integer-valued variable records the number of Rx packet buffers not
 * currently used by any call. These unused buffers are collected into a free
 * pool. 
 * 
 * 	\subsection sec5-4-8 Section 5.4.8: rx stackSize 
 * 
 * \par
 * This integer-valued variable records the size in bytes for the lightweight
 * process stack. The variable is initially set to RX DEFAULT STACK SIZE, and
 * is typically manipulated via the rx SetStackSize() macro. 
 * 
 * 	\subsection sec5-4-9 Section 5.4.9: rx packetTypes 
 * 
 * \par
 * This variable holds an array of string names used to describe the different
 * roles for Rx packets. Its value is derived from the RX PACKET TYPES
 * definition found in Section 5.2.11. 
 * 
 * 	\subsection sec5-4-10 Section 5.4.10: rx stats 
 * 
 * \par
 * This variable contains the statistics structure that keeps track of Rx
 * statistics. The struct rx stats structure it provides is defined in Section
 * 5.3.4.1. 
 * 
 * 	\section sec5-5 Section 5.5: Macros 
 * 
 * \par
 * Rx uses many macro definitions in preference to calling C functions
 * directly. There are two main reasons for doing this: 
 * \li field selection: Many Rx operations are easily realized by returning the
 * value of a particular structure's field. It is wasteful to invoke a C
 * routine to simply fetch a structure's field, incurring unnecessary function
 * call overhead. Yet, a convenient, procedure-oriented operation is still
 * provided to Rx clients for such operations by the use of macros. For
 * example, the rx ConnectionOf() macro, described in Section 5.5.1.1, simply
 * indirects through the Rx call structure pointer parameter to deliver the
 * conn field. 
 * \li Performance optimization: In some cases, a simple test or operation can
 * be performed to accomplish a particular task. When this simple,
 * straightforward operation fails, then a true C routine may be called to
 * handle to more complex (and rarer) situation. The Rx macro rx Write(),
 * described in Section 5.5.6.2, is a perfect example of this type of
 * optimization. Invoking rx Write() first checks to determine whether or not
 * the outgoing call's internal buffer has enough room to accept the specified
 * data bytes. If so, it copies them into the call's buffer, updating counts
 * and pointers as appropriate. Otherwise, rx Write() calls the rx WriteProc()
 * to do the work, which in this more complicated case involves packet
 * manipulations, dispatches, and allocations. The result is that the common,
 * simple cases are often handled in-line, with more complex (and rarer) cases
 * handled through true function invocations. 
 * \par
 * The set of Rx macros is described according to the following categories. 
 * \li field selections/assignments 
 * \li Boolean operations 
 * \li Service attributes 
 * \li Security-related operations 
 * \li Sizing operations 
 * \li Complex operation 
 * \li Security operation invocations 
 * 
 * 	\subsection sec5-5-1 Section 5.5.1: field Selections/Assignments 
 * 
 * \par
 * These macros facilitate the fetching and setting of fields from the
 * structures described Chapter 5.3. 
 * 
 * 	\subsubsection sec5-5-1-1 Section 5.5.1.1: rx ConnectionOf() 
 * 
 * \par
 * \#define rx_ConnectionOf(call) ((call)->conn) 
 * \par
 * Generate a reference to the connection field within the given Rx call
 * structure. The value supplied as the call argument must resolve into an
 * object of type (struct rx call *). An application of the rx ConnectionOf()
 * macro itself yields an object of type rx peer. 
 * 
 * 	\subsubsection sec5-5-1-2 Section 5.5.1.2: rx PeerOf() 
 * 
 * \par
 * \#define rx_PeerOf(conn) ((conn)->peer) 
 * \par
 * Generate a reference to the peer field within the given Rx call structure.
 * The value supplied as the conn argument must resolve into an object of type
 * (struct rx connection *). An instance of the rx PeerOf() macro itself
 * resolves into an object of type rx peer. 
 * 
 * 	\subsubsection sec5-5-1-3 Section 5.5.1.3: rx HostOf() 
 * 
 * \par
 * \#define rx_HostOf(peer) ((peer)->host) 
 * \par
 * Generate a reference to the host field within the given Rx peer structure.
 * The value supplied as the peer argument must resolve into an object of type
 * (struct rx peer *). An instance of the rx HostOf() macro itself resolves
 * into an object of type u long. 
 * 
 * 	\subsubsection sec5-5-1-4 Section 5.5.1.4: rx PortOf() 
 * 
 * \par
 * \#define rx_PortOf(peer) ((peer)->port) 
 * \par
 * Generate a reference to the port field within the given Rx peer structure.
 * The value supplied as the peer argument must resolve into an object of type
 * (struct rx peer *). An instance of the rx PortOf() macro itself resolves
 * into an object of type u short. 
 * 
 * 	\subsubsection sec5-5-1-5 Section 5.5.1.5: rx GetLocalStatus() 
 * 
 * \par
 * \#define rx_GetLocalStatus(call, status) ((call)->localStatus) 
 * \par
 * Generate a reference to the localStatus field, which specifies the local
 * user status sent out of band, within the given Rx call structure. The value
 * supplied as the call argument must resolve into an object of type (struct rx
 * call *). The second argument, status, is not used. An instance of the rx
 * GetLocalStatus() macro itself resolves into an object of type u char. 
 * 
 * 	\subsubsection sec5-5-1-6 Section 5.5.1.6: rx SetLocalStatus() 
 * 
 * \par
 * \#define rx_SetLocalStatus(call, status) ((call)->localStatus = (status)) 
 * \par
 * Assign the contents of the localStatus field, which specifies the local user
 * status sent out of band, within the given Rx call structure. The value
 * supplied as the call argument must resolve into an object of type (struct rx
 * call *). The second argument, status, provides the new value of the
 * localStatus field, and must resolve into an object of type u char. An
 * instance of the rx GetLocalStatus() macro itself resolves into an object
 * resulting from the assignment, namely the u char status parameter. 
 * 
 * 	\subsubsection sec5-5-1-7 Section 5.5.1.7: rx GetRemoteStatus() 
 * 
 * \par
 * \#define rx_GetRemoteStatus(call) ((call)->remoteStatus) 
 * \par
 * Generate a reference to the remoteStatus field, which specifies the remote
 * user status received out of band, within the given Rx call structure. The
 * value supplied as the call argument must resolve into an object of type
 * (struct rx call *). An instance of the rx GetRemoteStatus() macro itself
 * resolves into an object of type u char. 
 * 
 * 	\subsubsection sec5-5-1-8 Section 5.5.1.8: rx Error() 
 * 
 * \par
 * \#define rx_Error(call) ((call)->error) 
 * \par
 * Generate a reference to the error field, which specifies the current error
 * condition, within the given Rx call structure. The value supplied as the
 * call argument must resolve into an object of type (struct rx call *). An
 * instance of the rx Error() macro itself resolves into an object of type
 * long. 
 * 
 * 	\subsubsection sec5-5-1-9 Section 5.5.1.9: rx DataOf() 
 * 
 * \par
 * \#define rx_DataOf(packet) ((char *) (packet)->wire.data) 
 * \par
 * Generate a reference to the beginning of the data portion within the given
 * Rx packet as it appears on the wire. Any encryption headers will be resident
 * at this address. For Rx packets of type RX PACKET TYPE DATA, the actual user
 * data will appear at the address returned by the rx DataOf macro plus the
 * connection's security header size. The value supplied as the packet argument
 * must resolve into an object of type (struct rx packet *). An instance of the
 * rx DataOf() macro itself resolves into an object of type (u long *). 
 * 
 * 	\subsubsection sec5-5-1-10 Section 5.5.1.10: rx GetDataSize() 
 * 
 * \par
 * \#define rx_GetDataSize(packet) ((packet)->length) 
 * \par
 * Generate a reference to the length field, which specifies the number of
 * bytes of user data contained within the wire form of the packet, within the
 * given Rx packet description structure. The value supplied as the packet
 * argument must resolve into an object of type (struct rx packet *). An
 * instance of the rx GetDataSize() macro itself resolves into an object of
 * type short. 
 * 
 * 	\subsubsection sec5-5-1-11 Section 5.5.1.11: rx SetDataSize() 
 * 
 * \par
 * \#define rx_SetDataSize(packet, size) ((packet)->length = (size)) 
 * \par
 * Assign the contents of the length field, which specifies the number of bytes
 * of user data contained within the wire form of the packet, within the given
 * Rx packet description structure. The value supplied as the packet argument
 * must resolve into an object of type (struct rx packet *). The second
 * argument, size, provides the new value of the length field, and must resolve
 * into an object of type short. An instance of the rx SetDataSize() macro
 * itself resolves into an object resulting from the assignment, namely the
 * short length parameter. 
 * 
 * 	\subsubsection sec5-5-1-12 Section 5.5.1.12: rx GetPacketCksum() 
 * 
 * \par
 * \#define rx_GetPacketCksum(packet) ((packet)->header.spare) 
 * \par
 * Generate a reference to the header checksum field, as used by the built-in
 * rxkad security module (See Chapter 3), within the given Rx packet
 * description structure. The value supplied as the packet argument must
 * resolve into an object of type (struct rx packet *). An instance of the rx
 * GetPacketCksum() macro itself resolves into an object of type u short. 
 * 
 * 	\subsubsection sec5-5-1-13 Section 5.5.1.13: rx SetPacketCksum() 
 * 
 * \par
 * \#define rx_SetPacketCksum(packet, cksum) ((packet)->header.spare = (cksum)) 
 * \par
 * Assign the contents of the header checksum field, as used by the built-in
 * rxkad security module (See Chapter 3), within the given Rx packet
 * description structure. The value supplied as the packet argument must
 * resolve into an object of type (struct rx packet *). The second argument,
 * cksum, provides the new value of the checksum, and must resolve into an
 * object of type u short. An instance of the rx SetPacketCksum() macro itself
 * resolves into an object resulting from the assignment, namely the u short
 * checksum parameter. 
 * 
 * 	\subsubsection sec5-5-1-14 Section 5.5.1.14: rx GetRock() 
 * 
 * \par
 * \#define rx_GetRock(obj, type) ((type)(obj)->rock) 
 * \par
 * Generate a reference to the field named rock within the object identified by
 * the obj pointer. One common Rx structure to which this macro may be applied
 * is struct rx connection. The specified rock field is casted to the value of
 * the type parameter, which is the overall value of the rx GetRock() macro. 
 * 
 * 	\subsubsection sec5-5-1-15 Section 5.5.1.15: rx SetRock() 
 * 
 * \par
 * \#define rx_SetRock(obj, newrock) ((obj)->rock = (VOID *)(newrock)) 
 * \par
 * Assign the contents of the newrock parameter into the rock field of the
 * object pointed to by obj. The given object's rock field must be of type
 * (VOID *). An instance of the rx SetRock() macro itself resolves into an
 * object resulting from the assignment and is of type (VOID *). 
 * 
 * 	\subsubsection sec5-5-1-16 Section 5.5.1.16: rx SecurityClassOf() 
 * 
 * \par
 * \#define rx_SecurityClassOf(conn) ((conn)->securityIndex) 
 * \par
 * Generate a reference to the security index field of the given Rx connection
 * description structure. This identifies the security class used by the
 * connection. The value supplied as the conn argument must resolve into an
 * object of type (struct rx connection *). An instance of the rx
 * SecurityClassOf() macro itself resolves into an object of type u char. 
 * 
 * 	\subsubsection sec5-5-1-17 Section 5.5.1.17: rx SecurityObjectOf() 
 * 
 * \par
 * \#define rx_SecurityObjectOf(conn) ((conn)->securityObject) 
 * \par
 * Generate a reference to the security object in use by the given Rx
 * connection description structure. The choice of security object determines
 * the authentication protocol enforced by the connection. The value supplied
 * as the conn argument must resolve into an object of type (struct rx
 * connection *). An instance of the rx SecurityObjectOf() macro itself
 * resolves into an object of type (struct rx securityClass *). 
 * 
 * 	\subsection sec5-5-2 Section 5.5.2: Boolean Operations 
 * 
 * \par
 * The macros described in this section all return Boolean values. They are
 * used to query such things as the whether a connection is a server-side or
 * client-side one and if extra levels of checksumming are being used in Rx
 * packet headers. 
 * 
 * 	\subsubsection sec5-5-2-1 Section 5.5.2.1: rx IsServerConn() 
 * 
 * \par
 * \#define rx_IsServerConn(conn) ((conn)->type == RX_SERVER_CONNECTION) 
 * \par
 * Determine whether or not the Rx connection specified by the conn argument is
 * a server-side connection. The value supplied for conn must resolve to an
 * object of type struct rx connection. The result is determined by testing
 * whether or not the connection's type field is set to RX SERVER CONNECTION. 
 * \note Another macro, rx ServerConn(), performs the identical operation. 
 * 
 * 	\subsubsection sec5-5-2-2 Section 5.5.2.2: rx IsClientConn() 
 * 
 * \par
 * \#define rx_IsClientConn(conn) ((conn)->type == RX_CLIENT_CONNECTION) 
 * \par
 * Determine whether or not the Rx connection specified by the conn argument is
 * a client-side connection. The value supplied for conn must resolve to an
 * object of type struct rx connection. The result is determined by testing
 * whether or not the connection's type field is set to RX CLIENT CONNECTION. 
 * \note Another macro, rx ClientConn(), performs the identical operation. 
 * 
 * 	\subsubsection sec5-5-2-3 Section 5.5.2.2: rx IsUsingPktCksum() 
 * 
 * \par
 * \#define rx_IsUsingPktCksum(conn) ((conn)->flags &
 * RX_CONN_USING_PACKET_CKSUM) 
 * \par
 * Determine whether or not the Rx connection specified by the conn argument is
 * checksum-ming the headers of all packets on its calls. The value supplied
 * for conn must resolve to an object of type struct rx connection. The result
 * is determined by testing whether or not the connection's flags field has the
 * RX CONN USING PACKET CKSUM bit enabled. 
 * 
 * 	\subsection sec5-5-3 Section 5.5.3: Service Attributes 
 * 
 * \par
 * This section describes user-callable macros that manipulate the attributes
 * of an Rx service. Note that these macros must be called (and hence their
 * operations performed) before the given service is installed via the
 * appropriate invocation of the associated rx StartServer() function.
 * 
 * 	\subsubsection sec5-5-3-1 Section 5.5.3.1: rx SetStackSize()
 * 
 * \par
 * rx_stackSize = (((stackSize) stackSize) > rx_stackSize) ? stackSize :
 * rx_stackSize) 
 * \par
 * Inform the Rx facility of the stack size in bytes for a class of threads to
 * be created in support of Rx services. The exported rx stackSize variable
 * tracks the high-water mark for all stack size requests before the call to rx
 * StartServer(). If no calls to rx SetStackSize() are made, then rx stackSize
 * will retain its default setting of RX DEFAULT STACK SIZE. 
 * \par
 * In this macro, the first argument is not used. It was originally intended
 * that thread stack sizes would be settable on a per-service basis. However,
 * calls to rx SetStackSize() will ignore the service parameter and set the
 * high-water mark for all Rx threads created after the use of rx
 * SetStackSize(). The second argument, stackSize, specifies determines the new
 * stack size, and should resolve to an object of type int. The value placed in
 * the stackSize parameter will not be recorded in the global rx stackSize
 * variable unless it is greater than the variable's current setting. 
 * \par
 * An instance of the rx SetStackSize() macro itself resolves into the result
 * of the assignment, which is an object of type int. 
 * 
 * 	\subsubsection sec5-5-3-2 Section 5.5.3.2: rx SetMinProcs() 
 * 
 * \par
 * \#define rx_SetMinProcs(service, min) ((service)->minProcs = (min)) 
 * \par
 * Choose min as the minimum number of threads guaranteed to be available for
 * parallel execution of the given Rx service. The service parameter should
 * resolve to an object of type struct rx service. The min parameter should
 * resolve to an object of type short. An instance of the rx SetMinProcs()
 * macro itself resolves into the result of the assignment, which is an object
 * of type short. 
 * 
 * 	\subsubsection sec5-5-3-3 Section 5.5.3.3: rx SetMaxProcs() 
 * 
 * \par
 * \#define rx_SetMaxProcs(service, max) ((service)->maxProcs = (max)) 
 * \par
 * Limit the maximum number of threads that may be made available to the given
 * Rx service for parallel execution to be max. The service parameter should
 * resolve to an object of type struct rx service. The max parameter should
 * resolve to an object of type short. An instance of the rx SetMaxProcs()
 * macro itself resolves into the result of the assignment, which is an object
 * of type short. 
 * 
 * 	\subsubsection sec5-5-3-4 Section 5.5.3.4: rx SetIdleDeadTime() 
 * 
 * \par
 * \#define rx_SetIdleDeadTime(service, time) ((service)->idleDeadTime =
 * (time)) 
 * \par
 * Every Rx service has a maximum amount of time it is willing to have its
 * active calls sit idle (i.e., no new data is read or written for a call
 * marked as RX STATE ACTIVE) before unilaterally shutting down the call. The
 * expired call will have its error field set to RX CALL TIMEOUT. The operative
 * assumption in this situation is that the client code is exhibiting a
 * protocol error that prevents progress from being made on this call, and thus
 * the call's resources on the server side should be freed. The default value,
 * as recorded in the service's idleDeadTime field, is set at service creation
 * time to be 60 seconds. The rx SetIdleTime() macro allows a caller to
 * dynamically set this idle call timeout value. 
 * \par
 * The service parameter should resolve to an object of type struct rx service.
 * Also, the time parameter should resolve to an object of type short. finally,
 * an instance of the rx SetIdleDeadTime() macro itself resolves into the
 * result of the assignment, which is an object of type short. 
 * 
 * 	\subsubsection sec5-5-3-5 Section 5.5.3.5: rx SetServiceDeadTime() 
 * 
 * \par
 * \#define rx_SetServiceDeadTime(service, seconds)
 * ((service)->secondsUntilDead = (seconds)) 
 * \note This macro definition is obsolete and should NOT be used. Including it
 * in application code will generate a compile-time error, since the service
 * structure no longer has such a field defined. 
 * \par
 * See the description of the rx SetConnDeadTime() macro below to see how hard
 * timeouts may be set for situations of complete call inactivity. 
 * 
 * 	\subsubsection sec5-5-3-6 Section 5.5.3.6: rx SetRxDeadTime() 
 * 
 * \par
 * \#define rx_SetRxDeadTime(seconds) (rx_connDeadTime = (seconds)) 
 * \par
 * Inform the Rx facility of the maximum number of seconds of complete
 * inactivity that will be tolerated on an active call. The exported rx
 * connDeadTime variable tracks this value, and is initialized to a value of 12
 * seconds. The current value of rx connDeadTime will be copied into new Rx
 * service and connection records upon their creation. 
 * \par
 * The seconds argument determines the value of rx connDeadTime, and should
 * resolve to an object of type int. An instance of the rx SetRxDeadTime()
 * macro itself resolves into the result of the assignment, which is an object
 * of type int. 
 * 
 * 	\subsubsection sec5-5-3-7 Section 5.5.3.7: rx SetConnDeadTime() 
 * 
 * \par
 * \#define rx_SetConnDeadTime(conn, seconds) (rxi_SetConnDeadTime(conn,
 * seconds)) 
 * \par
 * Every Rx connection has a maximum amount of time it is willing to have its
 * active calls on a server connection sit without receiving packets of any
 * kind from its peer. After such a quiescent time, during which neither data
 * packets (regardless of whether they are properly sequenced or duplicates)
 * nor keep-alive packets are received, the call's error field is set to RX
 * CALL DEAD and the call is terminated. The operative assumption in this
 * situation is that the client making the call has perished, and thus the
 * call's resources on the server side should be freed. The default value, as
 * recorded in the connection's secondsUntilDead field, is set at connection
 * creation time to be the same as its parent service. The rx SetConnDeadTime()
 * macro allows a caller to dynamically set this timeout value. 
 * \par
 * The conn parameter should resolve to an object of type struct rx connection.
 * Also, the seconds parameter should resolve to an object of type int.
 * finally, an instance of the rx SetConnDeadTime() macro itself resolves into
 * the a call to rxi SetConnDeadTime(), whose return value is void. 
 * 
 * 	\subsubsection sec5-5-3-8 Section 5.5.3.8: rx SetConnHardDeadTime() 
 * 
 * \par
 * \#define rx_SetConnHardDeadTime(conn, seconds) ((conn)->hardDeadTime =
 * (seconds)) 
 * \par
 * It is convenient to be able to specify that calls on certain Rx connections
 * have a hard absolute timeout. This guards against protocol errors not caught
 * by other checks in which one or both of the client and server are looping.
 * The rx SetConnHardDeadTime() macro is available for this purpose. It will
 * limit calls on the connection identified by the conn parameter to execution
 * times of no more than the given number of seconds. By default, active calls
 * on an Rx connection may proceed for an unbounded time, as long as they are
 * not totally quiescent (see Section 5.5.3.7 for a description of the rx
 * SetConnDeadTime()) or idle (see Section 5.5.3.4 for a description of the rx
 * SetIdleDeadTime()). 
 * \par
 * The conn parameter should resolve to an object of type (struct rx connection
 * *). The seconds parameter should resolve to an object of type u short. An
 * instance of the rx SetConnHardDeadTime() macro itself resolves into the
 * result of the assignment, which is an object of type u short. 
 * 
 * 	\subsubsection sec5-5-3-9 Section 5.5.3.9: rx GetBeforeProc() 
 * 
 * \par
 * \#define rx_GetBeforeProc(service) ((service)->beforeProc) 
 * \par
 * Return a pointer of type (VOID *)() to the procedure associated with the
 * given Rx service that will be called immediately upon activation of a server
 * thread to handle an incoming call. The service parameter should resolve to
 * an object of type struct rx service. 
 * \par
 * When an Rx service is first created (via a call to the rx NewService()
 * function), its beforeProc field is set to a null pointer. See the
 * description of the rx SetBeforeProc() below. 
 * 
 * 	\subsubsection sec5-5-3-10 Section 5.5.3.10: rx SetBeforeProc() 
 * 
 * \par
 * \#define rx_SetBeforeProc(service, proc) ((service)->beforeProc = (proc)) 
 * \par
 * Instruct the Rx facility to call the procedure identified by the proc
 * parameter immediately upon activation of a server thread to handle an
 * incoming call. The specified procedure will be called with a single
 * parameter, a pointer of type struct rx call, identifying the call this
 * thread will now be responsible for handling. The value returned by the
 * procedure, if any, is discarded. 
 * \par
 * The service parameter should resolve to an object of type struct rx service.
 * The proc parameter should resolve to an object of type (VOID *)(). An
 * instance of the rx SetBeforeProc() macro itself resolves into the result of
 * the assignment, which is an object of type (VOID *)(). 
 * 
 * 	\subsubsection sec5-5-3-11 Section 5.5.3.11: rx GetAfterProc() 
 * 
 * \par
 * \#define rx_GetAfterProc(service) ((service)->afterProc) 
 * \par
 * Return a pointer of type (VOID *)() to the procedure associated with the
 * given Rx service that will be called immediately upon completion of the
 * particular Rx call for which a server thread was activated. The service
 * parameter should resolve to an object of type struct rx service. 
 * \par
 * When an Rx service is first created (via a call to the rx NewService()
 * function), its afterProc field is set to a null pointer. See the description
 * of the rx SetAfterProc() below. 
 * 
 * 	\subsubsection sec5-5-3-12 Section 5.5.3.12: rx SetAfterProc() 
 * 
 * \par
 * \#define rx_SetAfterProc(service, proc) ((service)->afterProc = (proc)) 
 * \par
 * Instruct the Rx facility to call the procedure identified by the proc
 * parameter immediately upon completion of the particular Rx call for which a
 * server thread was activated. The specified procedure will be called with a
 * single parameter, a pointer of type struct rx call, identifying the call
 * this thread just handled. The value returned by the procedure, if any, is
 * discarded. 
 * \par
 * The service parameter should resolve to an object of type struct rx service.
 * The proc parameter should resolve to an object of type (VOID *)(). An
 * instance of the rx SetAfterProc() macro itself resolves into the result of
 * the assignment, which is an object of type (VOID *)(). 
 * 
 * 	\subsubsection sec5-5-3-13 Section 5.5.3.13: rx SetNewConnProc() 
 * 
 * \par
 * \#define rx_SetNewConnProc(service, proc) ((service)->newConnProc = (proc)) 
 * \par
 * Instruct the Rx facility to call the procedure identified by the proc
 * parameter as the last step in the creation of a new Rx server-side
 * connection for the given service. The specified procedure will be called
 * with a single parameter, a pointer of type (struct rx connection *),
 * identifying the connection structure that was just built. The value returned
 * by the procedure, if any, is discarded. 
 * \par
 * The service parameter should resolve to an object of type struct rx service.
 * The proc parameter should resolve to an object of type (VOID *)(). An
 * instance of the rx SetNewConnProc() macro itself resolves into the result of
 * the assignment, which is an object of type (VOID *)(). 
 * \note There is no access counterpart defined for this macro, namely one that
 * returns the current setting of a service's newConnProc. 
 * 
 * 	\subsubsection sec5-5-3-14 Section 5.5.3.14: rx SetDestroyConnProc() 
 * 
 * \par
 * \#define rx_SetDestroyConnProc(service, proc) ((service)->destroyConnProc =
 * (proc)) 
 * \par
 * Instruct the Rx facility to call the procedure identified by the proc
 * parameter just before a server connection associated with the given Rx
 * service is destroyed. The specified procedure will be called with a single
 * parameter, a pointer of type (struct rx connection *), identifying the
 * connection about to be destroyed. The value returned by the procedure, if
 * any, is discarded. 
 * \par
 * The service parameter should resolve to an object of type struct rx service.
 * The proc parameter should resolve to an object of type (VOID *)(). An
 * instance of the rx SetDestroyConnProc() macro itself resolves into the
 * result of the assignment, which is an object of type (VOID *)(). 
 * \note There is no access counterpart defined for this macro, namely one that
 * returns the current setting of a service's destroyConnProc. 
 * 
 * 	\subsection sec5-5-4 Section 5.5.4: Security-Related Operations 
 * 
 * \par
 * The following macros are callable by Rx security modules, and assist in
 * getting and setting header and trailer lengths, setting actual packet size,
 * and finding the beginning of the security header (or data). 
 * 
 * 	\subsubsection sec5-5-4-1 Section 5.5.4.1: rx GetSecurityHeaderSize() 
 * 
 * \par
 * \#define rx_GetSecurityHeaderSize(conn) ((conn)->securityHeaderSize) 
 * \par
 * Generate a reference to the field in an Rx connection structure that records
 * the length in bytes of the associated security module's packet header data. 
 * \par
 * The conn parameter should resolve to an object of type struct rx connection.
 * An instance of the rx GetSecurityHeaderSize() macro itself resolves into an
 * object of type u short. 
 * 
 * 	\subsubsection sec5-5-4-2 Section 5.5.4.2: rx SetSecurityHeaderSize() 
 * 
 * \par
 * \#define rx_SetSecurityHeaderSize(conn, length) ((conn)->securityHeaderSize
 * = (length)) 
 * \par
 * Set the field in a connection structure that records the length in bytes of
 * the associated security module's packet header data. 
 * \par
 * The conn parameter should resolve to an object of type struct rx connection.
 * The length parameter should resolve to an object of type u short. An
 * instance of the rx SetSecurityHeaderSize() macro itself resolves into the
 * result of the assignment, which is an object of type u short. 
 * 
 * 	\subsubsection sec5-5-4-3 Section 5.5.4.3: rx
 * 	GetSecurityMaxTrailerSize() 
 * 
 * \par
 * \#define rx_GetSecurityMaxTrailerSize(conn) ((conn)->securityMaxTrailerSize) 
 * \par
 * Generate a reference to the field in an Rx connection structure that records
 * the maximum length in bytes of the associated security module's packet
 * trailer data. 
 * \par
 * The conn parameter should resolve to an object of type struct rx connection.
 * An instance of the rx GetSecurityMaxTrailerSize() macro itself resolves into
 * an object of type u short. 
 * 
 * 	\subsubsection sec5-5-4-4 Section 5.5.4.4: rx
 * 	SetSecurityMaxTrailerSize() 
 * 
 * \par
 * \#define rx_SetSecurityMaxTrailerSize(conn, length)
 * ((conn)->securityMaxTrailerSize = (length)) 
 * \par
 * Set the field in a connection structure that records the maximum length in
 * bytes of the associated security module's packet trailer data. 
 * \par
 * The conn parameter should resolve to an object of type struct rx connection.
 * The length parameter should resolve to an object of type u short. An
 * instance of the rx SetSecurityHeaderSize() macro itself resolves into the
 * result of the assignment, which is an object of type u short. 
 * 
 * 	\subsection sec5-5-5 Section 5.5.5: Sizing Operations 
 * 
 * \par
 * The macros described in this section assist the application programmer in
 * determining the sizes of the various Rx packet regions, as well as their
 * placement within a packet buffer. 
 * 
 * 	\subsubsection sec5-5-5-1 Section 5.5.5.1: rx UserDataOf() 
 * 
 * \par
 * \#define rx_UserDataOf(conn, packet) (((char *) (packet)->wire.data) +
 * (conn)->securityHeaderSize) 
 * \par
 * Generate a pointer to the beginning of the actual user data in the given Rx
 * packet, that is associated with the connection described by the conn
 * pointer. User data appears immediately after the packet's security header
 * region, whose length is determined by the security module used by the
 * connection. The conn parameter should resolve to an object of type struct rx
 * connection. The packet parameter should resolve to an object of type struct
 * rx packet. An instance of the rx UserDataOf() macro itself resolves into an
 * object of type (char *). 
 * 
 * 	\subsubsection sec5-5-5-2 Section 5.5.5.2: rx MaxUserDataSize() 
 * 
 * \par
 * \#define rx_MaxUserDataSize(conn) 
 * \n ((conn)->peer->packetSize 
 * \n -RX_HEADER_SIZE 
 * \n -(conn)->securityHeaderSize 
 * \n -(conn)->securityMaxTrailerSize) 
 * \par
 * Return the maximum number of user data bytes that may be carried by a packet
 * on the Rx connection described by the conn pointer. The overall packet size
 * is reduced by the IP, UDP, and Rx headers, as well as the header and trailer
 * areas required by the connection's security module. 
 * \par
 * The conn parameter should resolve to an object of type struct rx connection.
 * An instance of the rx MaxUserDataSize() macro itself resolves into the an
 * object of type (u short). 
 * 
 * 	\subsection sec5-5-6 Section 5.5.6: Complex Operations 
 * 
 * \par
 * Two Rx macros are designed to handle potentially complex operations, namely
 * reading data from an active incoming call and writing data to an active
 * outgoing call. Each call structure has an internal buffer that is used to
 * collect and cache data traveling through the call. This buffer is used in
 * conjunction with reading or writing to the actual Rx packets traveling on
 * the wire in support of the call. The rx Read() and rx Write() macros allow
 * their caller to simply manipulate the internal data buffer associated with
 * the Rx call structures whenever possible, thus avoiding the overhead
 * associated with a function call. When buffers are either filled or drained
 * (depending on the direction of the data flow), these macros will then call
 * functions to handle the more complex cases of generating or receiving
 * packets in support of the operation. 
 * 
 * 	\subsubsection sec5-5-6-1 Section 5.5.6.1: rx Read() 
 * 
 * \par
 * \#define rx_Read(call, buf, nbytes) 
 * \n ((call)->nLeft > (nbytes) ? 
 * \n memcpy((buf), (call)->bufPtr, (nbytes)),
 * \n (call)->nLeft -= (nbytes), (call)->bufPtr += (nbytes), (nbytes) 
 * \n : rx_ReadProc((call), (buf), (nbytes))) 
 * \par
 * Read nbytes of data from the given Rx call into the buffer to which buf
 * points. If the call's internal buffer has at least nbytes bytes already
 * filled, then this is done in-line with a copy and some pointer and counter
 * updates within the call structure. If the call's internal buffer doesn't
 * have enough data to satisfy the request, then the rx ReadProc() function
 * will handle this more complex situation. 
 * \par
 * In either case, the rx Read() macro returns the number of bytes actually
 * read from the call, resolving to an object of type int. If rx Read() returns
 * fewer than nbytes bytes, the call status should be checked via the rx
 * Error() macro. 
 * 
 * 	\subsubsection sec5-5-6-2 Section 5.5.6.2: rx Write() 
 * 
 * \par
 * \#define rx_Write(call, buf, nbytes) 
 * \n ((call)->nFree > (nbytes) ? 
 * \n memcpy((call)->bufPtr, (buf), (nbytes)),
 * \n (call)->nFree -= (nbytes), 
 * \n (call)->bufPtr += (nbytes), (nbytes) 
 * \n : rx_WriteProc((call), (buf), (nbytes))) 
 * \par
 * Write nbytes of data from the buffer pointed to by buf into the given Rx
 * call. If the call's internal buffer has at least nbytes bytes free, then
 * this is done in-line with a copy and some pointer and counter updates within
 * the call structure. If the call's internal buffer doesn't have room, then
 * the rx WriteProc() function will handle this more complex situation. 
 * \par
 * In either case, the rx Write() macro returns the number of bytes actually
 * written to the call, resolving to an object of type int. If zero is
 * returned, the call status should be checked via the rx Error() macro. 
 * 
 * 	\subsection sec5-5-7 Section 5.5.7: Security Operation Invocations 
 * 
 * \par
 * Every Rx security module is required to implement an identically-named set
 * of operations, through which the security mechanism it defines is invoked.
 * This characteristic interface is reminiscent of the vnode interface defined
 * and popularized for file systems by Sun Microsystems [4]. The structure
 * defining this function array is described in Section 5.3.1.1. 
 * \par
 * These security operations are part of the struct rx securityClass, which
 * keeps not only the ops array itself but also any private data they require
 * and a reference count. Every Rx service contains an array of these security
 * class objects, specifying the range of security mechanisms it is capable of
 * enforcing. Every Rx connection within a service is associated with exactly
 * one of that service's security objects, and every call issued on the
 * connection will execute the given security protocol. 
 * \par
 * The macros described below facilitate the execution of the security module
 * interface functions. They are covered in the same order they appear in the
 * struct rx securityOps declaration. 
 * 
 * 	\subsubsection sec5-5-7-1 Section 5.5.7.1: RXS OP() 
 * 
 * \code
 * #if defined(__STDC__) && !defined(__HIGHC__) 
 * 	#define RXS_OP(obj, op, args) 
 * 		((obj->ops->op_ ## op) ? (*(obj)->ops->op_ ## op)args : 0) 
 * #else 
 * 	#define RXS_OP(obj, op, args) 
 * 		((obj->ops->op_op) ? (*(obj)->ops->op_op)args : 0) 
 * #endif 
 * \endcode
 * 
 * \par
 * The RXS OP macro represents the workhorse macro in this group, used by all
 * the others. It takes three arguments, the first of which is a pointer to the
 * security object to be referenced. This obj parameter must resolve to an
 * object of type (struct rx securityOps *). The second parameter identifies
 * the specific op to be performed on this security object. The actual text of
 * this op argument is used to name the desired opcode function. The third and
 * final argument, args, specifies the text of the argument list to be fed to
 * the chosen security function. Note that this argument must contain the
 * bracketing parentheses for the function call's arguments. In fact, note that
 * each of the security function access macros defined below provides the
 * enclosing parentheses to this third RXS OP() macro. 
 * 
 * 	\subsubsection sec5-5-7-2 Section 5.5.7.1: RXS Close() 
 * 
 * \par
 * \#define RXS_Close(obj) RXS_OP(obj, Close, (obj)) 
 * \par
 * This macro causes the execution of the interface routine occupying the op
 * Close() slot in the Rx security object identified by the obj pointer. This
 * interface function is invoked by Rx immediately before a security object is
 * discarded. Among the responsibilities of such a function might be
 * decrementing the object's refCount field, and thus perhaps freeing up any
 * space contained within the security object's private storage region,
 * referenced by the object's privateData field. 
 * \par
 * The obj parameter must resolve into an object of type (struct rx securityOps
 * *). In generating a call to the security object's op Close() routine, the
 * obj pointer is used as its single parameter. An invocation of the RXS
 * Close() macro results in a return value identical to that of the op Close()
 * routine, namely a value of type int. 
 * 
 * 	\subsubsection sec5-5-7-3 Section 5.5.7.3: RXS NewConnection() 
 * 
 * \par
 * \#define RXS_NewConnection(obj, conn) RXS_OP(obj, NewConnection, (obj,
 * conn)) 
 * \par
 * This macro causes the execution of the interface routine in the op
 * NewConnection() slot in the Rx security object identified by the obj
 * pointer. This interface function is invoked by Rx immediately after a
 * connection using the given security object is created. Among the
 * responsibilities of such a function might be incrementing the object's
 * refCount field, and setting any per-connection information based on the
 * associated security object's private storage region, as referenced by the
 * object's privateData field. 
 * \par
 * The obj parameter must resolve into an object of type (struct rx securityOps
 * *). The conn argument contains a pointer to the newly-created connection
 * structure, and must resolve into an object of type (struct rx connection *). 
 * \par
 * In generating a call to the routine located at the security object's op
 * NewConnection() slot, the obj and conn pointers are used as its two
 * parameters. An invocation of the RXS NewConnection() macro results in a
 * return value identical to that of the op NewConnection() routine, namely a
 * value of type int. 
 * 
 * 	\subsubsection sec5-5-7-4 Section 5.5.7.4: RXS PreparePacket() 
 * 
 * \par
 * \#define RXS_PreparePacket(obj, call, packet) 
 * \n RXS_OP(obj, PreparePacket, (obj, call, packet)) 
 * \par
 * This macro causes the execution of the interface routine in the op
 * PreparePacket() slot in the Rx security object identified by the obj
 * pointer. This interface function is invoked by Rx each time it prepares an
 * outward-bound packet. Among the responsibilities of such a function might be
 * computing information to put into the packet's security header and/or
 * trailer. 
 * \par
 * The obj parameter must resolve into an object of type (struct rx securityOps
 * *). The call argument contains a pointer to the Rx call to which the given
 * packet belongs, and must resolve to an object of type (struct rx call *).
 * The final argument, packet, contains a pointer to the packet itself. It
 * should resolve to an object of type (struct rx packet *). 
 * \par
 * In generating a call to the routine located at the security object's op
 * PreparePacket() slot, the obj, call, and packet pointers are used as its
 * three parameters. An invocation of the RXS PreparePacket() macro results in
 * a return value identical to that of the op PreparePacket() routine, namely a
 * value of type int. 
 * 
 * 	\subsubsection sec5-5-7-5 Section 5.5.7.5: RXS SendPacket() 
 * 
 * \par
 * \#define RXS_SendPacket(obj, call, packet) RXS_OP(obj, SendPacket, (obj,
 * call, packet)) 
 * \par
 * This macro causes the execution of the interface routine occupying the op
 * SendPacket() slot in the Rx security object identified by the obj pointer.
 * This interface function is invoked by Rx each time it physically transmits
 * an outward-bound packet. Among the responsibilities of such a function might
 * be recomputing information in the packet's security header and/or trailer. 
 * \par
 * The obj parameter must resolve into an object of type (struct rx securityOps
 * *). The call argument contains a pointer to the Rx call to which the given
 * packet belongs, and must resolve to an object of type (struct rx call *).
 * The final argument, packet, contains a pointer to the packet itself. It
 * should resolve to an object of type (struct rx packet *). 
 * \par
 * In generating a call to the routine located at the security object's op
 * SendPacket() slot, the obj, call, and packet pointers are used as its three
 * parameters. An invocation of the RXS SendPacket() macro results in a return
 * value identical to that of the op SendPacket() routine, namely a value of
 * type int. 
 * 
 * 	\subsubsection sec5-5-7-6 Section 5.5.7.6: RXS CheckAuthentication() 
 * 
 * \par
 * \#define RXS_CheckAuthentication(obj, conn) RXS_OP(obj, CheckAuthentication,
 * (obj, conn)) 
 * \par
 * This macro causes the execution of the interface routine in the op
 * CheckAuthentication() slot in the Rx security object identified by the obj
 * pointer. This interface function is invoked by Rx each time it needs to
 * check whether the given connection is one on which authenticated calls are
 * being performed. Specifically, a value of 0 is returned if authenticated
 * calls are not being executed on this connection, and a value of 1 is
 * returned if they are. 
 * \par
 * The obj parameter must resolve into an object of type (struct rx securityOps
 * *). The conn argument contains a pointer to the Rx connection checked as to
 * whether authentication is being performed, and must resolve to an object of
 * type (struct rx connection *). 
 * \par
 * In generating a call to the routine in the security object's op
 * CheckAuthentication() slot, the obj and conn pointers are used as its two
 * parameters. An invocation of the RXS CheckAuthentication() macro results in
 * a return value identical to that of the op CheckAuthentication() routine,
 * namely a value of type int. 
 * 
 * 	\subsubsection sec5-5-7-7 Section 5.5.7.7: RXS CreateChallenge() 
 * 
 * \par
 * \#define RXS_CreateChallenge(obj, conn) RXS_OP(obj, CreateChallenge, (obj,
 * conn)) 
 * \par
 * This macro causes the execution of the interface routine in the op
 * CreateChallenge() slot in the Rx security object identified by the obj
 * pointer. This interface function is invoked by Rx each time a challenge
 * event is constructed for a given connection. Among the responsibilities of
 * such a function might be marking the connection as temporarily
 * unauthenticated until the given challenge is successfully met. 
 * \par
 * The obj parameter must resolve into an object of type (struct rx securityOps
 * *). The conn argument contains a pointer to the Rx connection for which the
 * authentication challenge is being constructed, and must resolve to an object
 * of type (struct rx connection *). 
 * \par
 * In generating a call to the routine located at the security object's op
 * CreateChallenge() slot, the obj and conn pointers are used as its two
 * parameters. An invocation of the RXS CreateChallenge() macro results in a
 * return value identical to that of the op CreateChallenge() routine, namely a
 * value of type int. 
 * 
 * 	\subsubsection sec5-5-7-8 Section 5.5.7.8: RXS GetChallenge() 
 * 
 * \par
 * \#define RXS_GetChallenge(obj, conn, packet) RXS_OP(obj, GetChallenge, (obj,
 * conn, packet)) 
 * \par
 * This macro causes the execution of the interface routine occupying the op
 * GetChallenge() slot in the Rx security object identified by the obj pointer.
 * This interface function is invoked by Rx each time a challenge packet is
 * constructed for a given connection. Among the responsibilities of such a
 * function might be constructing the appropriate challenge structures in the
 * area of packet dedicated to security matters. 
 * \par
 * The obj parameter must resolve into an object of type (struct rx securityOps
 * *). The conn argument contains a pointer to the Rx connection to which the
 * given challenge packet belongs, and must resolve to an object of type
 * (struct rx connection *). The final argument, packet, contains a pointer to
 * the challenge packet itself. It should resolve to an object of type (struct
 * rx packet *). 
 * \par
 * In generating a call to the routine located at the security object's op
 * GetChallenge() slot, the obj, conn, and packet pointers are used as its
 * three parameters. An invocation of the RXS GetChallenge() macro results in a
 * return value identical to that of the op GetChallenge() routine, namely a
 * value of type int. 
 * 
 * 	\subsubsection sec5-5-7-9 Section 5.5.7.9: RXS GetResponse() 
 * 
 * \par
 * \#define RXS_GetResponse(obj, conn, packet) RXS_OP(obj, GetResponse, (obj,
 * conn, packet)) 
 * \par
 * This macro causes the execution of the interface routine occupying the op
 * GetResponse() slot in the Rx security object identified by the obj pointer.
 * This interface function is invoked by Rx on the server side each time a
 * response to a challenge packet must be received. 
 * \par
 * The obj parameter must resolve into an object of type (struct rx securityOps
 * *). The conn argument contains a pointer to the Rx client connection that
 * must respond to the authentication challenge, and must resolve to a (struct
 * rx connection *) object. The final argument, packet, contains a pointer to
 * the packet to be built in response to the challenge. It should resolve to an
 * object of type (struct rx packet *). 
 * \par
 * In generating a call to the routine located at the security object's op
 * GetResponse() slot, the obj, conn, and packet pointers are used as its three
 * parameters. An invocation of the RXS GetResponse() macro results in a return
 * value identical to that of the op GetResponse() routine, namely a value of
 * type int. 
 * 
 * 	\subsubsection sec5-5-7-10 Section 5.5.7.10: RXS CheckResponse() 
 * 
 * \par
 * \#define RXS_CheckResponse(obj, conn, packet) RXS_OP(obj, CheckResponse,
 * (obj, conn, packet)) 
 * \par
 * This macro causes the execution of the interface routine in the op
 * CheckResponse() slot in the Rx security object identified by the obj
 * pointer. This interface function is invoked by Rx on the server side each
 * time a response to a challenge packet is received for a given connection.
 * The responsibilities of such a function might include verifying the
 * integrity of the response, pulling out the necessary security information
 * and storing that information within the affected connection, and otherwise
 * updating the state of the connection. 
 * \par
 * The obj parameter must resolve into an object of type (struct rx securityOps
 * *). The conn argument contains a pointer to the Rx server connection to
 * which the given challenge response is directed. This argument must resolve
 * to an object of type (struct rx connection *). The final argument, packet,
 * contains a pointer to the packet received in response to the challenge
 * itself. It should resolve to an object of type (struct rx packet *). 
 * \par
 * In generating a call to the routine located at the security object's op
 * CheckResponse() slot, the obj, conn, and packet pointers are ued as its
 * three parameters. An invocation of the RXS CheckResponse() macro results in
 * a return value identical to that of the op CheckResponse() routine, namely a
 * value of type int. 
 * 
 * 	\subsubsection sec5-5-7-11 Section 5.5.7.11: RXS CheckPacket() 
 * 
 * \par
 * \#define RXS_CheckPacket(obj, call, packet) RXS_OP(obj, CheckPacket, (obj,
 * call, packet)) 
 * \par
 * This macro causes the execution of the interface routine occupying the op
 * CheckPacket() slot in the Rx security object identified by the obj pointer.
 * This interface function is invoked by Rx each time a packet is received. The
 * responsibilities of such a function might include verifying the integrity of
 * given packet, detecting any unauthorized modifications or tampering. 
 * \par
 * The obj parameter must resolve into an object of type (struct rx securityOps
 * *). The conn argument contains a pointer to the Rx connection to which the
 * given challenge response is directed, and must resolve to an object of type
 * (struct rx connection *). The final argument, packet, contains a pointer to
 * the packet received in response to the challenge itself. It should resolve
 * to an object of type (struct rx packet *). 
 * \par
 * In generating a call to the routine located at the security object's op
 * CheckPacket() slot, the obj, conn, and packet pointers are used as its three
 * parameters. An invocation of the RXS CheckPacket() macro results in a return
 * value identical to that of the op CheckPacket() routine, namely a value of
 * type int. 
 * \par
 * Please note that any non-zero return will cause Rx to abort all calls on the
 * connection. Furthermore, the connection itself will be marked as being in
 * error in such a case, causing it to reject any further incoming packets. 
 * 
 * 	\subsubsection sec5-5-7-12 Section 5.5.7.12: RXS DestroyConnection() 
 * 
 * \par
 * \#define RXS_DestroyConnection(obj, conn) RXS_OP(obj, DestroyConnection,
 * (obj, conn)) 
 * \par
 * This macro causes the execution of the interface routine in the op
 * DestroyConnection() slot in the Rx security object identified by the obj
 * pointer. This interface function is invoked by Rx each time a connection
 * employing the given security object is being destroyed. The responsibilities
 * of such a function might include deleting any private data maintained by the
 * security module for this connection. 
 * \par
 * The obj parameter must resolve into an object of type (struct rx securityOps
 * *). The conn argument contains a pointer to the Rx connection being reaped,
 * and must resolve to a (struct rx connection *) object. 
 * \par
 * In generating a call to the routine located at the security object's op
 * DestroyConnection() slot, the obj and conn pointers are used as its two
 * parameters. An invocation of the RXS DestroyConnection() macro results in a
 * return value identical to that of the op DestroyConnection() routine, namely
 * a value of type int. 
 * 
 * 	\subsubsection sec5-5-7-13 Section 5.5.7.13: RXS GetStats() 
 * 
 * \par
 * \#define RXS_GetStats(obj, conn, stats) RXS_OP(obj, GetStats, (obj, conn,
 * stats)) 
 * \par
 * This macro causes the execution of the interface routine in the op
 * GetStats() slot in the Rx security object identified by the obj pointer.
 * This interface function is invoked by Rx each time current statistics
 * concerning the given security object are desired. 
 * \par
 * The obj parameter must resolve into an object of type (struct rx securityOps
 * *). The conn argument contains a pointer to the Rx connection using the
 * security object to be examined, and must resolve to an object of type
 * (struct rx connection *). The final argument, stats, contains a pointer to a
 * region to be filled with the desired statistics. It should resolve to an
 * object of type (struct rx securityObjectStats *). 
 * \par
 * In generating a call to the routine located at the security object's op
 * GetStats() slot, the obj, conn, and stats pointers are used as its three
 * parameters. An invocation of the RXS GetStats() macro results in a return
 * value identical to that of the op GetStats() routine, namely a value of type
 * int. 
 * 
 * 	\section sec5-6 Section 5.6: Functions 
 * 
 * \par
 * Rx exports a collection of functions that, in conjuction with the macros
 * explored in Section 5.5, allows its clients to set up and export services,
 * create and tear down connections to these services, and execute remote
 * procedure calls along these connections. 
 * \par
 * This paper employs two basic categorizations of these Rx routines. One set
 * of functions is meant to be called directly by clients of the facility, and
 * are referred to as the exported operations. The individual members of the
 * second set of functions are not meant to be called directly by Rx clients,
 * but rather are called by the collection of defined macros, so they must
 * still be lexically visible. These indirectly-executed routines are referred
 * to here as the semi-exported operations. 
 * \par
 * All Rx routines return zero upon success. The range of error codes employed
 * by Rx is defined in Section 5.2.15. 
 * 
 * 	\subsection sec5-6-1 Section 5.6.1: Exported Operations 
 * 
 * 	\subsection sec5-6-2 Section 5.6.2: rx Init _ Initialize Rx 
 * 
 * \par
 * int rx Init(IN int port) 
 * \par Description 
 * Initialize the Rx facility. If a non-zero port number is provided, it
 * becomes the default port number for any service installed later. If 0 is
 * provided for the port, a random port will be chosen by the system. The rx
 * Init() function sets up internal tables and timers, along with starting up
 * the listener thread. 
 * \par Error Codes 
 * RX ADDRINUSE The port provided has already been taken. 
 * 
 * 	\subsection sec5-6-3 Section 5.6.3: rx NewService _ Create and install
 * 	a new service 
 * 
 * \par
 * struct rx service *rx NewService(IN u short port; IN u short serviceId; IN
 * char *serviceName; IN struct rx securityClass **securityObjects; IN int
 * nSecurityObjects; IN long (*serviceProc)()) 
 * \par Description 
 * Create and advertise a new Rx service. A service is uniquely named by a UDP
 * port number plus a non-zero 16-bit serviceId on the given host. The port
 * argument may be set to zero if rx Init() was called with a non-zero port
 * number, in which case that original port will be used. A serviceName must
 * also be provided, to be used for identification purposes (e.g., the service
 * name might be used for probing for statistics). A pointer to an array of
 * nSecurityObjects security objects to be associated with the new service is
 * given in . securityObjects. The service's executeRequestProc() pointer is
 * set to serviceProc. 
 * \par
 * The function returns a pointer to a descriptor for the requested Rx service.
 * A null return value indicates that the new service could not be created.
 * Possible reasons include: 
 * \li The serviceId parameter was found to be zero. 
 * \li A port value of zero was specified at Rx initialization time (i.e., when
 * rx init() was called), requiring a non-zero value for the port parameter
 * here. 
 * \li Another Rx service is already using serviceId. 
 * \li Rx has already created the maximum RX MAX SERVICES Rx services (see
 * Section 5.2.1). 
 * \par Error Codes 
 * (struct rx service *) NULL The new Rx service could not be created, due to
 * one of the errors listed above. 
 * 
 * 	\subsection sec5-6-4 Section 5.6.4: rx NewConnection _ Create a new
 * 	connection to a given service 
 * 
 * \par
 * struct rx connection *rx NewConnection( IN u long shost, IN u short sport,
 * IN u short sservice, IN struct rx securityClass *securityObject, IN int
 * service SecurityIndex) 
 * \par Description 
 * Create a new Rx client connection to service sservice on the host whose IP
 * address is contained in shost and to that host's sport UDP port. The
 * corresponding Rx service identifier is expected in sservice. The caller also
 * provides a pointer to the security object to use for the connection in
 * securityObject, along with that object's serviceSecurityIndex among the
 * security objects associated with service sservice via a previous rx
 * NewService() call (see Section 5.6.3). 
 * \note It is permissible to provide a null value for the securityObject
 * parameter if the chosen serviceSecurityIndex is zero. This corresponds to
 * the pre-defined null security object, which does not engage in authorization
 * checking of any kind. 
 * \par Error Codes 
 * --- A pointer to an initialized Rx connection is always returned, unless osi
 *  Panic() is called due to memory allocation failure. 
 * 
 * 	\subsection sec5-6-5 Section 5.6.5: rx NewCall _ Start a new call on
 * 	the given connection 
 * 
 * \par
 * struct rx call *rx NewCall( IN struct rx connection *conn) 
 * \par Description 
 * Start a new Rx remote procedure call on the connection specified by the conn
 * parameter. The existing call structures (up to RX MAXCALLS of them) are
 * examined in order. The first non-active call encountered (i.e., either
 * unused or whose call->state is RX STATE DALLY) will be appropriated and
 * reset if necessary. If all call structures are in active use, the RX CONN
 * MAKECALL WAITING flag is set in the conn->flags field, and the thread
 * handling this request will sleep until a call structure comes free. Once a
 * call structure has been reserved, the keep-alive protocol is enabled for it. 
 * \par
 * The state of the given connection determines the detailed behavior of the
 * function. The conn->timeout field specifies the absolute upper limit of the
 * number of seconds this particular call may be in operation. After this time
 * interval, calls to such routines as rx SendData() or rx ReadData() will fail
 * with an RX CALL TIMEOUT indication. 
 * \par Error Codes 
 * --- A pointer to an initialized Rx call is always returned, unless osi
 *  Panic() is called due to memory allocation failure. 
 * 
 * 	\subsection sec5-6-6 Section 5.6.6: rx EndCall _ Terminate the given
 * 	call 
 * 
 * \par
 * int rx EndCall(
 * \param IN struct rx call *call,
 * \param IN long rc
 * \n ) 
 * \par Description
 * Indicate that the Rx call described by the structure located at call is
 * finished, possibly prematurely. The value passed in the rc parameter is
 * returned to the peer, if appropriate. The final error code from processing
 * the call will be returned as rx EndCall()'s value. The given call's state
 * will be set to RX STATE DALLY, and threads waiting to establish a new call
 * on this connection are signalled (see the description of the rx NewCall() in
 * Section 5.6.5). 
 * \par Error Codes 
 * -1 Unspecified error has occurred. 
 * 
 * 	\subsection sec5-6-7 Section 5.6.7: rx StartServer _ Activate installed
 * 	rx service(s) 
 * 
 * \par
 * void rx StartServer( IN int donateMe) 
 * \par Description 
 * This function starts server threads in support of the Rx services installed
 * via calls to rx NewService() (see Section 5.6.3). This routine first
 * computes the number of server threads it must create, governed by the
 * minProcs and maxProcs fields in the installed service descriptors. The
 * minProcs field specifies the minimum number of threads that are guaranteed
 * to be concurrently available to the given service. The maxProcs field
 * specifies the maximum number of threads that may ever be concurrently
 * assigned to the particular service, if idle threads are available. Using
 * this information, rx StartServer() computes the correct overall number of
 * threads as follows: For each installed service, minProcs threads will be
 * created, enforcing the minimality guarantee. Calculate the maximum
 * difference between the maxProcs and minProcs fields for each service, and
 * create this many additional server threads, enforcing the maximality
 * guarantee. 
 * \par
 * If the value placed in the donateMe argument is zero, then rx StartServer()
 * will simply return after performing as described above. Otherwise, the
 * thread making the rx StartServer() call will itself begin executing the
 * server thread loop. In this case, the rx StartServer() call will never
 * return. 
 * \par Error Codes 
 * ---None. 
 * 
 * 	\subsection sec5-6-8 Section 5.6.8: rx PrintStats -- Print basic
 * 	statistics to a file
 * 
 * \par
 * void rx PrintStats( IN FILE *file)
 * \par Description
 * Prints Rx statistics (basically the contents of the struct rx stats holding
 * the statistics for the Rx facility) to the open file descriptor identified
 * by file. The output is ASCII text, and is intended for human consumption. 
 * \note This function is available only if the Rx package has been compiled
 * with the RXDEBUG flag. 
 * \par Error Codes 
 * ---None. 
 * 
 * 	\subsection sec5-6-9 Section 5.6.9: rx PrintPeerStats _ Print peer
 * 	statistics to a file 
 * \par
 * void rx PrintPeerStats( IN FILE *file, IN struct rx peer *peer)
 * \par Description
 * Prints the Rx peer statistics found in peer to the open file descriptor
 * identified by file. The output is in normal ASCII text, and is intended for
 * human consumption. 
 * \note This function is available only if the Rx package has been compiled
 * with the RXDEBUG flag. 
 * \par Error Codes 
 * ---None. 
 * 
 * 	\subsection sec5-6-10 Section 5.6.10: rx finalize _ Shut down Rx
 * 	gracefully 
 * 
 * \par
 * void rx finalize() 
 * \par Description 
 * This routine may be used to shut down the Rx facility for either server or
 * client applications. All of the client connections will be gracefully
 * garbage-collected after their active calls are cleaned up. The result of
 * calling rx finalize() from a client program is that the server-side entity
 * will be explicitly advised that the client has terminated. This notification
 * frees the server-side application from having to probe the client until its
 * records eventually time out, and also allows it to free resources currently
 * assigned to that client's support. 
 * \par Error Codes 
 * ---None. 
 * 
 * 	\subsection sec5-6-11 Section 5.6.11: Semi-Exported Operations 
 * 
 * \par
 * As described in the introductory text in Section 5.6, entries in this
 * lexically-visible set of Rx functions are not meant to be called directly by
 * client applications, but rather are invoked by Rx macros called by users. 
 * 
 * 	\subsection sec5-6-12 Section 5.6.12: rx WriteProc _ Write data to an
 * 	outgoing call 
 * 
 * \par
 * int rx WriteProc( IN struct rx call *call, IN char *buf, IN int nbytes)
 * \par Description 
 * Write nbytes of data from buffer buf into the Rx call identified by the call
 * parameter. The value returned by rx WriteProc() reports the number of bytes
 * actually written into the call. If zero is returned, then the rx Error()
 * macro may be used to obtain the call status. 
 * \par
 * This routine is called by the rx Write() macro, which is why it must be
 * exported by the Rx facility. 
 * \par Error Codes 
 * Indicates error in the given Rx call; use the rx Error() macro to determine
 * the call status. 
 * 
 * 	\subsection sec5-6-13 Section 5.6.13: rx ReadProc _ Read data from an
 * 	incoming call 
 * 
 * \par
 * int rx ReadProc( IN struct rx call *call, IN char *buf, IN int nbytes)
 * \par Description 
 * Read up to nbytes of data from the Rx call identified by the call parameter
 * into the buf buffer. The value returned by rx ReadProc() reports the number
 * of bytes actually read from the call. If zero is returned, then the rx
 * Error() macro may be used to obtain the call status. 
 * \par
 * This routine is called by the rx Read() macro, which is why it must be
 * exported by the Rx facility. 
 * \par Error Codes 
 * Indicates error in the given Rx call; use the rx Error() macro to determine
 * the call status. 
 * 
 * 	\subsection sec5-6-1 Section 5.6.1: rx FlushWrite -- Flush buffered
 * 	data on outgoing call
 * 
 * \par
 * void rx FlushWrite( IN struct rx call *call)
 * \par Description
 * Flush any buffered data on the given Rx call to the stream. If the call is
 * taking place on a server connection, the call->mode is set to RX MODE EOF.
 * If the call is taking place on a client connection, the call->mode is set to
 * RX MODE RECEIVING. 
 * \par Error Codes 
 * ---None. 
 * 
 * 	\subsection sec5-6-15 Section 5.6.15: rx SetArrivalProc _ Set function
 * 	to invoke upon call packet arrival 
 * 
 * \par
 * void rx SetArrivalProc( IN struct rx call *call, IN VOID (*proc)(), IN VOID
 * *handle, IN VOID *arg) 
 * \par Description 
 * Establish a procedure to be called when a packet arrives for a call. This
 * routine will be called at most once after each call, and will also be called
 * if there is an error condition on the call or the call is complete. The rx
 * SetArrivalProc() function is used by multicast Rx routines to build a
 * selection function that determines which of several calls is likely to be a
 * good one to read from. The implementor's comments in the Rx code state that,
 * due to the current implementation, it is probably only reasonable to use rx
 * SetArrivalProc() immediately after an rx NewCall(), and to only use it once. 
 * \par Error Codes 
 * ---None. 
 * 
 * 	\page chap6 Chapter 6 -- Example Server and Client 
 * 
 * 	\section sec6-1 Section 6.1: Introduction 
 * 
 * \par
 * This chapter provides a sample program showing the use of Rx. Specifically,
 * the rxdemo application, with all its support files, is documented and
 * examined. The goal is to provide the reader with a fully-developed and
 * operational program illustrating the use of both regular Rx remote procedure
 * calls and streamed RPCs. The full text of the rxdemo application is
 * reproduced in the sections below, along with additional commentary. 
 * \par
 * Readers wishing to directly experiment with this example Rx application are
 * encouraged to examine the on-line version of rxdemo. Since it is a program
 * of general interest, it has been installed in the usr/contrib tree in the
 * grand.central.org cell. This area contains user-contributed software for the
 * entire AFS community. At the top of this tree is the
 * /afs/grand.central.org/darpa/usr/contrib directory. Both the server-side and
 * client-side rxdemo binaries (rxdemo server and rxdemo client, respectively)
 * may be found in the bin subdirectory. The actual sources reside in the
 * .site/grand.central.org/rxdemo/src subdirectory. 
 * \par
 * The rxdemo code is composed of two classes of files, namely those written by
 * a human programmer and those generated from the human-written code by the
 * Rxgen tool. Included in the first group of files are: 
 * \li 	rxdemo.xg This is the RPC interface definition file, providing
 * high-level definitions of the supported calls. 
 * \li 	rxdemo client.c: This is the rxdemo client program, calling upon the
 * associated server to perform operations defined by rxdemo.xg. 
 * \li 	rxdemo server.c: This is the rxdemo server program, implementing the
 * operations promised in rxdemo.xg. 
 * \li 	Makefile: This is the file that directs the compilation and
 * installation of the rxdemo code. 
 * \par
 * The class of automatically-generated files includes the following items: 
 * \li rxdemo.h: This header file contains the set of constant definitions
 * present in rxdemo.xg, along with information on the RPC opcodes defined for
 * this Rx service. 
 * \li rxdemo.cs.c: This client-side stub file performs all the marshalling and
 * unmarshalling of the arguments for the RPC routines defined in rxdemo.xg. 
 * \li rxdemo.ss.c: This stub file similarly defines all the marshalling and
 * unmarshalling of arguments for the server side of the RPCs, invokes the
 * routines defined within rxdemo server.c to implement the calls, and also
 * provides the dispatcher function. 
 * \li rxdemo.xdr.c: This module defines the routines required to convert
 * complex user-defined data structures appearing as arguments to the Rx RPC
 * calls exported by rxdemo.xg into network byte order, so that correct
 * communication is guaranteed between clients and server with different memory
 * organizations. 
 * \par
 * The chapter concludes with a section containing sample output from running
 * the rxdemo server and client programs. 
 * 
 * 	\section sec6-2 Section 6.2: Human-Generated files 
 * 
 * \par
 * The rxdemo application is based on the four human-authored files described
 * in this section. They provide the basis for the construction of the full set
 * of modules needed to implement the specified Rx service. 
 * 
 * 	\subsection sec6-2-1 Section 6.2.1: Interface file: rxdemo.xg 
 * 
 * \par
 * This file serves as the RPC interface definition file for this application.
 * It defines various constants, including the Rx service port to use and the
 * index of the null security object (no encryption is used by rxdemo). It
 * defines the RXDEMO MAX and RXDEMO MIN constants, which will be used by the
 * server as the upper and lower bounds on the number of Rx listener threads to
 * run. It also defines the set of error codes exported by this facility.
 * finally, it provides the RPC function declarations, namely Add() and
 * Getfile(). Note that when building the actual function definitions, Rxgen
 * will prepend the value of the package line in this file, namely "RXDEMO ",
 * to the function declarations. Thus, the generated functions become RXDEMO
 * Add() and RXDEMO Getfile(), respectively. Note the use of the split keyword
 * in the RXDEMO Getfile() declaration, which specifies that this is a streamed
 * call, and actually generates two client-side stub routines (see Section
 * 6.3.1). 
 * 
 * \code
 * /*======================================================================= 
 * * Interface for an example Rx server/client application, using both * * 
 * standard and streamed calls.  * ** * Edward R. Zayas * * Transarc 
 * Corporation * ** ** * The United States Government has rights in this 
 * work pursuant * * to contract no. MDA972-90-C-0036 between the United 
 * States Defense * * Advanced Research Projects Agency and Transarc 
 * Corporation.  * ** * (C) Copyright 1991 Transarc Corporation * ** * 
 * Redistribution and use in source and binary forms are permitted * 
 * provided that: (1) source distributions retain this entire copy- * * 
 * right notice and comment, and (2) distributions including binaries * * 
 * display the following acknowledgement: * ** * ''This product includes 
 * software developed by Transarc * * Corporation and its contributors'' * 
 * ** * in the documentation or other materials mentioning features or * * 
 * use of this software. Neither the name of Transarc nor the names * * of 
 * its contributors may be used to endorse or promote products * * derived 
 * from this software without specific prior written * * permission.  * ** 
 * * THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED * 
 * * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF * 
 * * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 * =======================================================================*/
 * 
 * package RXDEMO_ 
 * %#include <rx/rx.h> 
 * %#include <rx/rx_null.h> 
 * %#define RXDEMO_SERVER_PORT 8000 /* Service port to advertise */
 * %#define RXDEMO_SERVICE_PORT 0 /* User server's port */
 * %#define RXDEMO_SERVICE_ID 4 /* Service ID */
 * %#define RXDEMO_NULL_SECOBJ_IDX 0 /* Index of null security object */
 * 
 * /* Maximum number of requests that will be handled by this service 
 *  * simultaneously. This number will be guaranteed to execute in 
 *  * parallel if other service's results are being processed. */
 * 
 * %#define RXDEMO_MAX 3 
 * 
 * /* Minimum number of requests that are guaranteed to be 
 *  * handled simultaneously. */
 * 
 * %#define RXDEMO_MIN 2 
 * 
 * /* Index of the "null" security class in the sample service. */
 * 
 * %#define RXDEMO_NULL 0 
 * 
 * /* Maximum number of characters in a file name (for demo purposes). */
 * 
 * %#define RXDEMO_NAME_MAX_CHARS 64 
 * 
 * /* Define the max number of bytes to transfer at one shot. */
 * 
 * %#define RXDEMO_BUFF_BYTES 512 
 * 
 * /* Values returned by the RXDEMO_Getfile() call. 
 *  * RXDEMO_CODE_SUCCESS : Everything went fine. 
 *  * RXDEMO_CODE_CANT_OPEN : Can't open named file. 
 *  * RXDEMO_CODE_CANT_STAT : Can't stat open file. 
 *  * RXDEMO_CODE_CANT_READ : Error reading the open file. 
 *  * RXDEMO_CODE_WRITE_ERROR : Error writing the open file. */
 * 
 * /* ------------Interface calls defined for this service ----------- */
 * %#define RXDEMO_CODE_SUCCESS 0 
 * %#define RXDEMO_CODE_CANT_OPEN 1 
 * %#define RXDEMO_CODE_CANT_STAT 2 
 * %#define RXDEMO_CODE_CANT_READ 3 
 * %#define RXDEMO_CODE_WRITE_ERROR 4 
 * /* -------------------------------------------------------------------
 * * RXDEMO_Add * 
 * *	
 * * Summary: 
 * *	Add the two numbers provided and return the result. * 
 * * Parameters: 
 * *	int a_first : first operand. 
 * *	int a_second : Second operand. 
 * *	int *a_result : Sum of the above. * 
 * *	Side effects: None.  
 * *-------------------------------------------------------------------- */
 * 
 * Add(IN int a, int b, OUT int *result) = 1; 
 * /*-------------------------------------------------------------------
 * * RXDEMO_Getfile * 
 * * Summary: 
 * * 	Return the contents of the named file in the server's environment. 
 * * Parameters: 
 * * 	STRING a_nameToRead : Name of the file whose contents are to be 
 * *	fetched. 
 * * 	int *a_result : Set to the result of opening and reading the file 
 * * 	on the server side. * 
 * * 	Side effects: None. 
 * *-------------------------------------------------------------------- */
 * 
 * Getfile(IN string a_nameToRead<RXDEMO_NAME_MAX_CHARS>, OUT int *a_result) 
 * 	split = 2; 
 * \endcode
 * 
 * 	\subsection sec6-2-2 Section 6.2.2: Client Program: rxdemo client.c 
 * 
 * \par
 * The rxdemo client program, rxdemo client, calls upon the associated server
 * to perform operations defined by rxdemo.xg. After its header, it defines a
 * private GetIPAddress() utility routine, which given a character string host
 * name will return its IP address. 
 * 
 * \code
 * /*======================================================================= 
 * % Client side of an example Rx application, using both standard and % % 
 * streamed calls.  % %% % Edward R. Zayas % % Transarc Corporation % %% 
 * %% % The United States Government has rights in this work pursuant % % 
 * to contract no. MDA972-90-C-0036 between the United States Defense % % 
 * Advanced Research Projects Agency and Transarc Corporation.  % %% % (C) 
 * Copyright 1991 Transarc Corporation % %% % Redistribution and use in source 
 * and binary forms are permitted % % provided that: (1) source distributions 
 * retain this entire copy- % % right notice and comment, and (2) distributions 
 * including binaries % % display the following acknowledgement: % %% % 
 * ''This product includes software developed by Transarc % % Corporation and 
 * its contributors'' % %% % in the documentation or other materials mentioning 
 * features or % % use of this software. Neither the name of Transarc nor the 
 * names % % of its contributors may be used to endorse or promote products % % 
 * derived from this software without specific prior written % % permission. 
 * % %% % THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED 
 * % % WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF % % 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. 
 * % %=======================================================================
 * */
 * 
 * #include <sys/types.h> 
 * #include <netdb.h> 
 * #include <stdio.h> 
 * #include "rxdemo.h" 
 * static char pn[] = "rxdemo"; /* Program name */
 * static u_long GetIpAddress(a_hostName) char *a_hostName; 
 * { /* GetIPAddress */
 * 	static char rn[] = "GetIPAddress"; /* Routine name */
 * 	struct hostent *hostEntP; /* Ptr to host descriptor */
 * 	u_long hostIPAddr; /* Host IP address */
 * 	hostEntP = gethostbyname(a_hostName); 
 * 	if (hostEntP == (struct hostent *)0) { 
 * 		printf("[%s:%s] Host '%s' not found\n", 
 * 		pn, rn, a_hostName); 
 * 		exit(1); 
 * 	} 
 * 	if (hostEntP->h_length != sizeof(u_long)) { 
 * 		printf("[%s:%s] Wrong host address length (%d bytes instead of
 * 		%d)", 
 * 		pn, rn, hostEntP->h_length, sizeof(u_long)); 
 * 		exit(1); 
 * 	} 
 * 	memcpy(&hostIPAddr, hostEntP->h_addr, sizeof(hostIPAddr));
 * 	return(hostIPAddr); 
 * } /* GetIpAddress */
 * \endcode
 * 
 * \par
 * The main program section of the client code, after handling its command line
 * arguments, starts off by initializing the Rx facility. 
 * 
 * \code
 * main(argc, argv) 
 * int argc; 
 * char **argv; 
 * { /* Main */
 * 	struct rx_connection *rxConnP; /* Ptr to server connection */
 * 	struct rx_call *rxCallP; /* Ptr to Rx call descriptor */
 * 	u_long hostIPAddr; /* IP address of chosen host */
 * 	int demoUDPPort; /* UDP port of Rx service */
 * 	struct rx_securityClass *nullSecObjP; /* Ptr to null security object */
 * 	int operand1, operand2; /* Numbers to add int sum; Their sum */
 * 	int code; /* Return code */
 * 	char fileName[64]; /* Buffer for desired file's name */
 * 	long fileDataBytes; /* Num bytes in file to get */
 * 	char buff[RXDEMO_BUFF_BYTES+1]; /* Read buffer */
 * 	int currBytesToRead; /* Num bytes to read in one iteration */
 * 	int maxBytesToRead; /* Max bytes to read in one iteration */
 * 	int bytesReallyRead; /* Num bytes read off Rx stream */
 * 	int getResults; /* Results of the file fetch */
 * 	printf("\n%s: Example Rx client process\n\n", pn); 
 * 	if ((argc < 2) || (argc > 3)) { 
 * 		printf("Usage: rxdemo <HostName> [PortToUse]"); 
 * 		exit(1); 
 * 	} 
 * 	hostIPAddr = GetIpAddress(argv[1]); 
 * 	if (argc > 2) 
 * 		demoUDPPort = atoi(argv[2]); 
 * 	else 
 * 		demoUDPPort = RXDEMO_SERVER_PORT; 
 * 	/* Initialize the Rx facility. */
 * 	code = rx_Init(htons(demoUDPPort)); 
 * 	if (code) { 
 * 		printf("** 	Error calling rx_Init(); code is %d\n", code); 
 * 		exit(1); 
 * 	} 
 * 	/* Create a client-side null security object. */
 * 	nullSecObjP = rxnull_NewClientSecurityObject(); 
 * 	if (nullSecObjP == (struct rx_securityClass *)0) { 
 * 		printf("%s: Can't create a null client-side security
 * 		object!\n", pn); 
 * 		exit(1); 
 * 	} 
 * 	/* Set up a connection to the desired Rx service, telling it to use
 * 	* the null security object we just created.  */
 * 	printf("Connecting to Rx server on '%s', IP address 0x%x, UDP port
 * 	%d\n", argv[1], hostIPAddr, demoUDPPort); 
 * 	rxConnP = rx_NewConnection(hostIPAddr, RXDEMO_SERVER_PORT,
 * 	RXDEMO_SERVICE_ID, nullSecObjP, RXDEMO_NULL_SECOBJ_IDX); 
 * 	if (rxConnP == (struct rx_connection *)0) { 
 * 		printf("rxdemo: Can't create connection to server!\n"); 
 * 		exit(1); 
 * 	} else 
 * 		printf(" ---> Connected.\n"); 
 * \endcode
 * 
 * \par
 * The rx Init() invocation initializes the Rx library and defines the desired
 * service UDP port (in network byte order). The rxnull
 * NewClientSecurityObject() call creates a client-side Rx security object that
 * does not perform any authentication on Rx calls. Once a client
 * authentication object is in hand, the program calls rx NewConnection(),
 * specifying the host, UDP port, Rx service ID, and security information
 * needed to establish contact with the rxdemo server entity that will be
 * providing the service. 
 * \par
 * With the Rx connection in place, the program may perform RPCs. The first one
 * to be invoked is RXDEMO Add(): 
 *  
 * \code
 * /* Perform our first, simple remote procedure call. */
 * operand1 = 1; 
 * operand2 = 2; 
 * printf("Asking server to add %d and %d: ", operand1, operand2); 
 * code = RXDEMO_Add(rxConnP, operand1, operand2, &sum); 
 * if (code) { 
 * 	printf("  // ** Error in the RXDEMO_Add RPC: code is %d\n", code); 
 * 	exit(1); 
 * } 
 * printf("Reported sum is %d\n", sum); 
 * \endcode
 * 
 * \par
 * The first argument to RXDEMO Add() is a pointer to the Rx connection
 * established above. The client-side body of the RXDEMO Add() function was
 * generated from the rxdemo.xg interface file, and resides in the rxdemo.cs.c
 * file (see Section 6.3.1). It gives the appearance of being a normal C
 * procedure call. 
 * \par
 * The second RPC invocation involves the more complex, streamed RXDEMO
 * Getfile() function. More of the internal Rx workings are exposed in this
 * type of call. The first additional detail to consider is that we must
 * manually create a new Rx call on the connection. 
 *  
 * \code
 * /* Set up for our second, streamed procedure call. */
 * printf("Name of file to read from server: "); 
 * scanf("%s", fileName); 
 * maxBytesToRead = RXDEMO_BUFF_BYTES; 
 * printf("Setting up an Rx call for RXDEMO_Getfile..."); 
 * rxCallP = rx_NewCall(rxConnP); 
 * if (rxCallP == (struct rx_call *)0) { 
 * 	printf("** Can't create call\n"); 
 * 	exit(1); 
 * } 
 * printf("done\n"); 
 * \endcode
 * 
 * \par
 * Once the Rx call structure has been created, we may begin executing the call
 * itself. Having been declared to be split in the interface file, Rxgen
 * creates two function bodies for rxdemo Getfile() and places them in
 * rxdemo.cs.c. The first, StartRXDEMO Getfile(), is responsible for
 * marshalling the outgoing arguments and issuing the RPC. The second,
 * EndRXDEMO Getfile(), takes care of unmarshalling the non-streamed OUT
 * function parameters. The following code fragment illustrates how the RPC is
 * started, using the StartRXDEMO Getfile() routine to pass the call parameters
 * to the server. 
 *  
 * \code
 * /* Sending IN parameters for the streamed call. */
 * code = StartRXDEMO_Getfile(rxCallP, fileName); 
 * if (code) { 
 * 	printf("** 	Error calling StartRXDEMO_Getfile(); code is %d\n",
 * 	code); 
 * 	exit(1); 
 * } 
 * \endcode
 * 
 * \par
 * Once the call parameters have been shipped, the server will commence
 * delivering the "stream" data bytes back to the client on the given Rx call
 * structure. The first longword to come back on the stream specifies the
 * number of bytes to follow. 
 *  
 * \par
 * Begin reading the data being shipped from the server in response to * our
 * setup call. The first longword coming back on the Rx call is 
 * the number of bytes to follow. It appears in network byte order, 
 * so we have to fix it up before referring to it. 
 * 
 * \code
 * bytesReallyRead = rx_Read(rxCallP, &fileDataBytes, sizeof(long)); 
 * if (bytesReallyRead != sizeof(long)) { 
 * 	printf("** Only %d bytes read for file length; should have been %d\n",
 * 	bytesReallyRead, sizeof(long)); 
 * 	exit(1); 
 * } 
 * fileDataBytes = ntohl(fileDataBytes); 
 * \endcode
 * 
 * \par
 * Once the client knows how many bytes will be sent, it runs a loop in which
 * it reads a buffer at a time from the Rx call stream, using rx Read() to
 * accomplish this. In this application, all that is done with each
 * newly-acquired buffer of information is printing it out. 
 * 
 * \code 
 * /* Read the file bytes via the Rx call, a buffer at a time. */
 * printf("[file contents (%d bytes) fetched over the Rx call appear
 * below]\n\n", fileDataBytes); 
 * while (fileDataBytes > 0) 
 * { 
 * 	currBytesToRead = (fileDataBytes > maxBytesToRead ?  maxBytesToRead :
 * 	fileDataBytes); 
 * 	bytesReallyRead = rx_Read(rxCallP, buff, currBytesToRead); 
 * 	if (bytesReallyRead != currBytesToRead)
 * 	{ 
 * 		printf("\nExpecting %d bytes on this read, got %d instead\n",
 * 		currBytesToRead, bytesReallyRead); 
 * 		exit(1); 
 * 	}  
 * 	/* Null-terminate the chunk before printing it. */
 * 	buff[currBytesToRead] = 0; 
 * 	printf("%s", buff); 
 * 	/* Adjust the number of bytes left to read. */
 * 	fileDataBytes -= currBytesToRead; 
 * } /* Read one bufferful of the file */
 * \endcode
 * 
 * \par
 * After this loop terminates, the Rx stream has been drained of all data. The
 * Rx call is concluded by invoking the second of the two
 * automatically-generated functions, EndRXDEMO Getfile(), which retrieves the
 * call's OUT parameter from the server. 
 * 
 * \code
 * /* finish off the Rx call, getting the OUT parameters. */
 * printf("\n\n[End of file data]\n"); 
 * code = EndRXDEMO_Getfile(rxCallP, &getResults); 
 * if (code) 
 * { 
 * 	printf("** 	Error getting file transfer results; code is %d\n",
 * 	code); 
 * 	exit(1); 
 * } 
 * \endcode
 * 
 * \par
 * With both normal and streamed Rx calls accomplished, the client demo code
 * concludes by terminating the Rx call it set up earlier. With that done, the
 * client exits. 
 * 
 * \code 
 * /* finish off the Rx call. */
 * code = rx_EndCall(rxCallP, code); 
 * if (code) 
 * 	printf("Error 	in calling rx_EndCall(); code is %d\n", code); 
 * 
 * printf("\n\nrxdemo complete.\n"); 
 * \endcode
 * 
 * 	\subsection sec6-2-3 Server Program: rxdemo server.c 
 * 
 * \par
 * The rxdemo server program, rxdemo server, implements the operations promised
 * in the rxdemo.xg interface file. 
 * \par
 * After the initial header, the external function RXDEMO ExecuteRequest() is
 * declared. The RXDEMO ExecuteRequest() function is generated automatically by
 * rxgen from the interface file and deposited in rxdemo.ss.c. The main program
 * listed below will associate this RXDEMO ExecuteRequest() routine with the Rx
 * service to be instantiated. 
 * 
 * \code
 * /*======================================================================
 * % % Advanced Research Projects Agency and Transarc Corporation.  % %% % 
 * (C) Copyright 1991 Transarc Corporation % %% % Redistribution and use in 
 * source and binary forms are permitted % % provided that: (1) source 
 * distributions retain this entire copy- % % right notice and comment, and 
 * (2) distributions including binaries % % display the following 
 * acknowledgement: % %% % ''This product includes software developed by 
 * Transarc % % Corporation and its contributors'' % %% % in the documentation 
 * or other materials mentioning features or % % use of this software. Neither 
 * the name of Transarc nor the names % % of its contributors may be used to 
 * endorse or promote products % % derived from this software without specific 
 * prior written % % permission.  % %% % THIS SOFTWARE IS PROVIDED "AS IS" AND 
 * WITHOUT ANY EXPRESS OR IMPLIED % % WARRANTIES, INCLUDING, WITHOUT
 * LIMITATION, 
 * THE IMPLIED WARRANTIES OF % % MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE.  % %
 * ====================================================================== */
 * 
 * /* Server portion of the example RXDEMO application, using both % 
 * standard and streamed calls. % % Edward R. Zayas % Transarc Corporation % 
 * % % The United States Government has rights in this work pursuant % 
 * to contract no. MDA972-90-C-0036 between the United States Defense % */
 * 
 * #include <sys/types.h> 
 * #include <sys/stat.h> 
 * #include <sys/file.h> 
 * #include <netdb.h> 
 * #include <stdio.h> 
 * #include "rxdemo.h" 
 * #define N_SECURITY_OBJECTS 1 
 * extern RXDEMO_ExecuteRequest(); 
 * \endcode
 * 
 * \par
 * After choosing either the default or user-specified UDP port on which the Rx
 * service will be established, rx Init() is called to set up the library. 
 * 
 * \code
 * main(argc, argv) 
 * 	int argc; 
 * 	char **argv; 
 * { /* Main */
 * 	static char pn[] = "rxdemo_server"; /* Program name */
 * 	struct rx_securityClass 
 * 	(securityObjects[1]); /* Security objs */
 * 	struct rx_service *rxServiceP; /* Ptr to Rx service descriptor */
 * 	struct rx_call *rxCallP; /* Ptr to Rx call descriptor */
 * 	int demoUDPPort; /* UDP port of Rx service */
 * 	int fd; /* file descriptor */
 * 	int code; /* Return code */
 * 	printf("\n%s: Example Rx server process\n\n", pn); 
 * 	if (argc >2) { 
 * 		printf("Usage: rxdemo [PortToUse]"); 
 * 		exit(1); 
 * 	} 
 * 	if (argc > 1) 
 * 		demoUDPPort = atoi(argv[1]); 
 * 	else 
 * 		demoUDPPort = RXDEMO_SERVER_PORT; 
 * 
 * 	/* Initialize the Rx facility, telling it the UDP port number this 
 * 	* server will use for its single service.  */
 * 
 * 	printf("Listening on UDP port %d\n", demoUDPPort); 
 * 	code = rx_Init(demoUDPPort); 
 * 	if (code) { 
 * 		printf("** 	Error calling rx_Init(); code is %d\n", code); 
 * 		exit(1); 
 * 	} 
 * \endcode
 *  
 * \par
 * A security object specific to the server side of an Rx conversation is
 * created in the next code fragment. As with the client side of the code, a
 * "null" server security object, namely one that does not perform any
 * authentication at all, is constructed with the rxnull
 * NewServerSecurityObject() function. 
 *  
 * \code
 * 	/* Create a single server-side security object. In this case, the 
 * 	* null security object (for unauthenticated connections) will be used 
 * 	* to control security on connections made to this server. */
 * 
 * 	securityObjects[RXDEMO_NULL_SECOBJ_IDX] =
 * 	rxnull_NewServerSecurityObject(); 
 * 	if (securityObjects[RXDEMO_NULL_SECOBJ_IDX] == (struct rx_securityClass
 * 	*) 0) { 
 * 		printf("** Can't create server-side security object\n"); 
 * 		exit(1); 
 * 	} 
 * \endcode
 * 
 * \par
 * The rxdemo server program is now in a position to create the desired Rx
 * service, primed to recognize exactly those interface calls defined in
 * rxdemo.xg. This is accomplished by calling the rx NewService() library
 * routine, passing it the security object created above and the generated Rx
 * dispatcher routine. 
 * 
 * \code 
 * /* Instantiate a single sample service. The rxgen-generated procedure 
 * * called to dispatch requests is passed in (RXDEMO_ExecuteRequest).  */
 * 
 * 	rxServiceP = rx_NewService(	0, 
 * 					RXDEMO_SERVICE_ID, 
 * 					"rxdemo", 
 * 					securityObjects, 
 * 					1, 
 * 					RXDEMO_ExecuteRequest
 * 				); 
 * 	if (rxServiceP == (struct rx_service *) 0) { 
 * 		printf("** Can't create Rx service\n"); 
 * 		exit(1); 
 * 	} 
 * \endcode
 * 
 * \par
 * The final step in this main routine is to activate servicing of calls to the
 * exported Rx interface. Specifically, the proper number of threads are
 * created to handle incoming interface calls. Since we are passing a non-zero
 * argument to the rx StartServer() call, the main program will itself begin
 * executing the server thread loop, never returning from the rx StartServer()
 * call. The print statement afterwards should never be executed, and its
 * presence represents some level of paranoia, useful for debugging
 * malfunctioning thread packages. 
 * 
 * \code 
 * 	/* Start up Rx services, donating this thread to the server pool. */
 * 	rx_StartServer(1); 
 * 	/* We should never return from the previous call. */
 * 	printf("** rx_StartServer() returned!!\n"); exit(1); 
 * } /* Main */
 * \endcode
 * 
 * \par
 * Following the main procedure are the functions called by the
 * automatically-generated routines in the rxdemo.ss.c module to implement the
 * specific routines defined in the Rx interface. 
 * \par
 * The first to be defined is the RXDEMO Add() function. The arguments for this
 * routine are exactly as they appear in the interface definition, with the
 * exception of the very first. The a rxCallP parameter is a pointer to the Rx
 * structure describing the call on which this function was activated. All
 * user-supplied routines implementing an interface function are required to
 * have a pointer to this structure as their first parameter. Other than
 * printing out the fact that it has been called and which operands it
 * received, all that RXDEMO Add() does is compute the sum and place it in the
 * output parameter. 
 * \par
 * Since RXDEMO Add() is a non-streamed function, with all data travelling
 * through the set of parameters, this is all that needs to be done. To mark a
 * successful completion, RXDEMO Add() returns zero, which is passed all the
 * way through to the RPC's client. 
 * 
 * \code
 * int RXDEMO_Add(a_rxCallP, a_operand1, a_operand2, a_resultP) 
 * 	struct rx_call *a_rxCallP; 
 * int a_operand1, a_operand2; 
 * int *a_resultP; 
 * { /* RXDEMO_Add */
 * 	printf("\t[Handling call to RXDEMO_Add(%d, %d)]\n", 
 * 		a_operand1, a_operand2); 
 * 	*a_resultP = a_operand1 + a_operand2; 
 * 	return(0); 
 * } /* RXDEMO_Add */
 * \endcode
 * 
 * \par
 * The next and final interface routine defined in this file is RXDEMO
 * Getfile(). Declared as a split function in the interface file, RXDEMO
 * Getfile() is an example of a streamed Rx call. As with RXDEMO Add(), the
 * initial parameter is required to be a pointer to the Rx call structure with
 * which this routine is associated, Similarly, the other parameters appear
 * exactly as in the interface definition, and are handled identically. 
 * \par
 * The difference between RXDEMO Add() and RXDEMO Getfile() is in the use of
 * the rx Write() library routine by RXDEMO Getfile() to feed the desired
 * file's data directly into the Rx call stream. This is an example of the use
 * of the a rxCallP argument, providing all the information necessary to
 * support the rx Write() activity. 
 * \par
 * The RXDEMO Getfile() function begins by printing out the fact that it's been
 * called and the name of the requested file. It will then attempt to open the
 * requested file and stat it to determine its size. 
 * 
 * \code
 * int RXDEMO_Getfile(a_rxCallP, a_nameToRead, a_resultP) 
 * 	struct rx_call *a_rxCallP; 
 * char *a_nameToRead; 
 * int *a_resultP; 
 * { /* RXDEMO_Getfile */
 * 	struct stat fileStat; /* Stat structure for file */
 * 	long fileBytes; /* Size of file in bytes */
 * 	long nbofileBytes; /* file bytes in network byte order */
 * 	int code; /* Return code */
 * 	int bytesReallyWritten; /* Bytes written on Rx channel */
 * 	int bytesToSend; /* Num bytes to read & send this time */
 * 	int maxBytesToSend; /* Max num bytes to read & send ever */
 * 	int bytesRead; /* Num bytes read from file */
 * 	char buff[RXDEMO_BUFF_BYTES+1]; /* Read buffer */
 * 	int fd; /* file descriptor */
 * 	maxBytesToSend = RXDEMO_BUFF_BYTES; 
 * 	printf("\t[Handling call to RXDEMO_Getfile(%s)]\n", a_nameToRead); 
 * 	fd = open(a_nameToRead, O_RDONLY, 0444); 
 * 	if (fd <0) { 
 * 		printf("\t\t[**Can't open file '%s']\n", a_nameToRead); 
 * 		*a_resultP = RXDEMO_CODE_CANT_OPEN; 
 * 		return(1); 
 * 	} else 
 * 		printf("\t\t[file opened]\n"); 
 * 	/* Stat the file to find out how big it is. */
 * 	code = fstat(fd, &fileStat); 
 * 	if (code) { 
 * 		a_resultP = RXDEMO_CODE_CANT_STAT; 
 * 		printf("\t\t[file closed]\n"); 
 * 		close(fd); 
 * 		return(1); 
 * 	} 
 * 	fileBytes = fileStat.st_size; 
 * 	printf("\t\t[file has %d bytes]\n", fileBytes); 
 * \endcode
 * 
 * \par
 * Only standard unix operations have been used so far. Now that the file is
 * open, we must first feed the size of the file, in bytes, to the Rx call
 * stream. With this information, the client code can then determine how many
 * bytes will follow on the stream. As with all data that flows through an Rx
 * stream, the longword containing the file size, in bytes, must be converted
 * to network byte order before being sent. This insures that the recipient may
 * properly interpret the streamed information, regardless of its memory
 * architecture. 
 * 
 * \code
 * 	nbofileBytes = htonl(fileBytes); 
 * 	/* Write out the size of the file to the Rx call. */
 * 	bytesReallyWritten = rx_Write(a_rxCallP, &nbofileBytes, sizeof(long)); 
 * 	if (bytesReallyWritten != sizeof(long)) { 
 * 		printf("** %d bytes written instead of %d for file length\n", 
 * 		bytesReallyWritten, sizeof(long)); 
 * 		*a_resultP = RXDEMO_CODE_WRITE_ERROR; 
 * 		printf("\t\t[file closed]\n"); 
 * 		close(fd); 
 * 		return(1); 
 * 	} 
 * \endcode
 * 
 * \par
 * Once the number of file bytes has been placed in the stream, the RXDEMO
 * Getfile() routine runs a loop, reading a buffer's worth of the file and then
 * inserting that buffer of file data into the Rx stream at each iteration.
 * This loop executes until all of the file's bytes have been shipped. Notice
 * there is no special end-of-file character or marker inserted into the
 * stream. 
 * \par
 * The body of the loop checks for both unix read() and rx Write errors. If
 * there is a problem reading from the unix file into the transfer buffer, it
 * is reflected back to the client by setting the error return parameter
 * appropriately. Specifically, an individual unix read() operation could fail
 * to return the desired number of bytes. Problems with rx Write() are handled
 * similarly. All errors discovered in the loop result in the file being
 * closed, and RXDEMO Getfile() exiting with a non-zero return value. 
 *  
 * \code
 * 	/* Write out the contents of the file, one buffer at a time.  */
 * 	while (fileBytes > 0) {  
 * 		/* figure out the number of bytes to 
 * 		* read (and send) this time.  */
 * 		bytesToSend = (fileBytes > maxBytesToSend ? 
 * 				maxBytesToSend : fileBytes); 
 * 		bytesRead = read(fd, buff, bytesToSend); 
 * 		if (bytesRead != bytesToSend) { 
 * 			printf("Read %d instead of %d bytes from the file\n", 
 * 				bytesRead, bytesToSend); 
 * 			*a_resultP = RXDEMO_CODE_WRITE_ERROR; 
 * 			printf("\t\t[file closed]\n"); 
 * 			close(fd); 
 * 			return(1); 
 * 		} 
 * 		/* Go ahead and send them. */
 * 		bytesReallyWritten = rx_Write(a_rxCallP, buff, bytesToSend); 
 * 		if (bytesReallyWritten != bytesToSend) { 
 * 			printf("%d file bytes written instead of %d\n", 
 * 				bytesReallyWritten, bytesToSend); 
 * 			*a_resultP = RXDEMO_CODE_WRITE_ERROR; 
 * 			printf("\t\t[file closed]\n"); 
 * 			close(fd); 
 * 			return(1); 
 * 		} 
 * 		/* Update the number of bytes left to go. */
 * 		fileBytes -= bytesToSend; 
 * 	} /* Write out the file to our caller */
 * \endcode
 * 
 * \par
 * Once all of the file's bytes have been shipped to the remote client, all
 * that remains to be done is to close the file and return successfully. 
 * 
 * \code
 * 	/* Close the file, then return happily. */
 * 	*a_resultP = RXDEMO_CODE_SUCCESS; 
 * 	printf("\t\t[file closed]\n"); 
 * 	close(fd); 
 * 	return(0); 
 * } /* RXDEMO_Getfile */
 * \endcode
 * 
 * 	\subsection sec6-2-4 Section 6.2.4: Makefile 
 * 
 * \par
 * This file directs the compilation and installation of the rxdemo code. It
 * specifies the locations of libraries, include files, sources, and such tools
 * as Rxgen and install, which strips symbol tables from executables and places
 * them in their target directories. This Makefile demostrates cross-cell
 * software development, with the rxdemo sources residing in the
 * grand.central.org cell and the AFS include files and libraries accessed from
 * their locations in the transarc.com cell. 
 * \par
 * In order to produce and install the rxdemo server and rxdemo client
 * binaries, the system target should be specified on the command line when
 * invoking make: 
 * \code
 * 		make system 
 * \endcode
 * \par
 * A note of caution is in order concerning generation of the rxdemo binaries.
 * While tools exist that deposit the results of all compilations to other
 * (architecture-specific) directories, and thus facilitate multiple
 * simultaneous builds across a variety of machine architectures (e.g.,
 * Transarc's washtool), the assumption is made here that compilations will
 * take place directly in the directory containing all the rxdemo sources.
 * Thus, a user will have to execute a make clean command to remove all
 * machine-specific object, library, and executable files before compiling for
 * a different architecture. Note, though, that the binaries are installed into
 * a directory specifically reserved for the current machine type.
 * Specifically, the final pathname component of the ${PROJ DIR}bin
 * installation target is really a symbolic link to ${PROJ DIR}.bin/@sys. 
 * \par
 * Two libraries are needed to support the rxdemo code. The first is obvious,
 * namely the Rx librx.a library. The second is the lightweight thread package
 * library, liblwp.a, which implements all the threading operations that must
 * be performed. The include files are taken from the unix /usr/include
 * directory, along with various AFS-specific directories. Note that for
 * portability reasons, this Makefile only contains fully-qualified AFS
 * pathnames and "standard" unix pathnames (such as /usr/include). 
 * 
 * \code
 * /*#=======================================================================# 
 * # The United States Government has rights in this work pursuant # # to 
 * contract no. MDA972-90-C-0036 between the United States Defense # # Advanced 
 * Research Projects Agency and Transarc Corporation. # # # # (C) Copyright
 * 1991 
 * Transarc Corporation # # # # Redistribution and use in source and binary
 * forms 
 * are permitted # # provided that: (1) source distributions retain this entire 
 * copy-# # right notice and comment, and (2) distributions including binaries
 * # 
 * # display the following acknowledgement: # # # # ''This product includes 
 * software developed by Transarc # # Corporation and its contributors'' # # #
 * # 
 * in the documentation or other materials mentioning features or # # use of
 * this 
 * software. Neither the name of Transarc nor the names # # of its contributors 
 * may be used to endorse or promote products # # derived from this software 
 * without specific prior written # # permission. # # # # THIS SOFTWARE IS 
 * PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED # # WARRANTIES,
 * INCLUDING, 
 * WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF # # MERCHANTABILITY AND
 * FITNESS 
 * FOR A PARTICULAR PURPOSE. # 
 * #=======================================================================# */
 * 
 * SHELL = /bin/sh 
 * TOOL_CELL = grand.central.org 
 * AFS_INCLIB_CELL = transarc.com 
 * USR_CONTRIB = /afs/${TOOL_CELL}/darpa/usr/contrib/ 
 * PROJ_DIR = ${USR_CONTRIB}.site/grand.central.org/rxdemo/ 
 * AFS_INCLIB_DIR = /afs/${AFS_INCLIB_CELL}/afs/dest/ 
 * RXGEN = ${AFS_INCLIB_DIR}bin/rxgen 
 * INSTALL = ${AFS_INCLIB_DIR}bin/install 
 * LIBS = 	${AFS_INCLIB_DIR}lib/librx.a \ ${AFS_INCLIB_DIR}lib/liblwp.a 
 * CFLAGS = -g \ 
 * 	-I. \ 
 * 	-I${AFS_INCLIB_DIR}include \ 
 * 	-I${AFS_INCLIB_DIR}include/afs \ 
 * 	-I${AFS_INCLIB_DIR} \ 
 * 	-I/usr/include 
 * 
 * system: install 
 * 
 * install: all 
 * 	${INSTALL} rxdemo_client 
 * 	${PROJ_DIR}bin 
 * 	${INSTALL} rxdemo_server 
 * 	${PROJ_DIR}bin 
 * 
 * all: rxdemo_client rxdemo_server 
 * 
 * rxdemo_client: rxdemo_client.o ${LIBS} rxdemo.cs.o ${CC} ${CFLAGS} 
 * 		-o rxdemo_client rxdemo_client.o rxdemo.cs.o ${LIBS} 
 * 
 * rxdemo_server: rxdemo_server.o rxdemo.ss.o ${LIBS} ${CC} ${CFLAGS} 
 * 		-o rxdemo_server rxdemo_server.o rxdemo.ss.o ${LIBS} 
 * 
 * rxdemo_client.o: rxdemo.h 
 * 
 * rxdemo_server.o: rxdemo.h 
 * 
 * rxdemo.cs.c rxdemo.ss.c rxdemo.er.c rxdemo.h: rxdemo.xg rxgen rxdemo.xg 
 * 
 * clean: rm -f *.o rxdemo.cs.c rxdemo.ss.c rxdemo.xdr.c rxdemo.h \ 
 * 		rxdemo_client rxdemo_server core 
 * \endcode
 * 
 * 	\section sec6-3 Section 6.3: Computer-Generated files 
 * 
 * \par
 * The four human-generated files described above provide all the information
 * necessary to construct the full set of modules to support the rxdemo example
 * application. This section describes those routines that are generated from
 * the base set by Rxgen, filling out the code required to implement an Rx
 * service. 
 * 
 * 	\subsection sec6-3-1 Client-Side Routines: rxdemo.cs.c 
 * 
 * \par
 * The rxdemo client.c program, described in Section 6.2.2, calls the
 * client-side stub routines contained in this module in order to make rxdemo
 * RPCs. Basically, these client-side stubs are responsible for creating new Rx
 * calls on the given connection parameter and then marshalling and
 * unmarshalling the rest of the interface call parameters. The IN and INOUT
 * arguments, namely those that are to be delivered to the server-side code
 * implementing the call, must be packaged in network byte order and shipped
 * along the given Rx call. The return parameters, namely those objects
 * declared as INOUT and OUT, must be fetched from the server side of the
 * associated Rx call, put back in host byte order, and inserted into the
 * appropriate parameter variables. 
 * \par
 * The first part of rxdemo.cs.c echoes the definitions appearing in the
 * rxdemo.xg interface file, and also #includes another Rxgen-generated file,
 * rxdemo.h. 
 * 
 * \code
 * /*======================================================================% 
 * * % THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED % 
 * * % WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF % 
 * * % MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. % 
 * * %====================================================================== */
 * /* Machine generated file --Do NOT edit */
 * 
 * #include "rxdemo.h" 
 * #define RXDEMO_CODE_WRITE_ERROR 4 
 * 
 * #include <rx/rx.h>
 * #include <rx/rx_null.h>
 * #define RXDEMO_SERVER_PORT 8000 /* Service port to advertise */
 * #define RXDEMO_SERVICE_PORT 0 /* User server's port */
 * #define RXDEMO_SERVICE_ID 4 /* Service ID */
 * #define RXDEMO_NULL_SECOBJ_IDX 0 /* Index of null security object */
 * #define RXDEMO_MAX 3
 * #define RXDEMO_MIN 2
 * #define RXDEMO_NULL 0 
 * #define RXDEMO_NAME_MAX_CHARS 64
 * #define RXDEMO_BUFF_BYTES 512
 * #define RXDEMO_CODE_SUCCESS 0
 * #define RXDEMO_CODE_CANT_OPEN 1
 * #define RXDEMO_CODE_CANT_STAT 2
 * #define RXDEMO_CODE_CANT_READ 3
 * #define RXDEMO_CODE_WRITE_ERROR 4
 * \endcode
 * 
 * \par
 * The next code fragment defines the client-side stub for the RXDEMO Add()
 * routine, called by the rxdemo client program to execute the associated RPC. 
 * 
 * \code
 * int RXDEMO_Add(z_conn, a, b, result) register struct rx_connection *z_conn; 
 * int a, b; 
 * int * result; 
 * { 
 * 	struct rx_call *z_call = rx_NewCall(z_conn); 
 * 	static int z_op = 1; 
 * 	int z_result; 
 * 	XDR z_xdrs; 
 * 	xdrrx_create(&z_xdrs, z_call, XDR_ENCODE); 
 * 	/* Marshal the arguments */
 * 	if ((!xdr_int(&z_xdrs, &z_op)) 
 * 			|| (!xdr_int(&z_xdrs, &a)) 
 * 			|| (!xdr_int(&z_xdrs, &b))) { 
 * 		z_result = RXGEN_CC_MARSHAL; 
 * 		goto fail; 
 * 	} 
 * 	/* Un-marshal the reply arguments */
 * 	z_xdrs.x_op = XDR_DECODE; 
 * 	if ((!xdr_int(&z_xdrs, result))) { 
 * 		z_result = RXGEN_CC_UNMARSHAL; 
 * 		goto fail; 
 * 	} 
 * 	z_result = RXGEN_SUCCESS; 
 * 	fail: return rx_EndCall(z_call, z_result); 
 * } 
 * \endcode
 * 
 * \par
 * The very first operation performed by RXDEMO Add() occurs in the local
 * variable declarations, where z call is set to point to the structure
 * describing a newly-created Rx call on the given connection. An XDR
 * structure, z xdrs, is then created for the given Rx call with xdrrx
 * create(). This XDR object is used to deliver the proper arguments, in
 * network byte order, to the matching server stub code. Three calls to xdr
 * int() follow, which insert the appropriate Rx opcode and the two operands
 * into the Rx call. With the IN arguments thus transmitted, RXDEMO Add()
 * prepares to pull the value of the single OUT parameter. The z xdrs XDR
 * structure, originally set to XDR ENCODE objects, is now reset to XDR DECODE
 * to convert further items received into host byte order. Once the return
 * parameter promised by the function is retrieved, RXDEMO Add() returns
 * successfully. 
 * \par
 * Should any failure occur in passing the parameters to and from the server
 * side of the call, the branch to fail will invoke Rx EndCall(), which advises
 * the server that the call has come to a premature end (see Section 5.6.6 for
 * full details on rx EndCall() and the meaning of its return value). 
 * \par
 * The next client-side stub appearing in this generated file handles the
 * delivery of the IN parameters for StartRXDEMO Getfile(). It operates
 * identically as the RXDEMO Add() stub routine in this respect, except that it
 * does not attempt to retrieve the OUT parameter. Since this is a streamed
 * call, the number of bytes that will be placed on the Rx stream cannot be
 * determined at compile time, and must be handled explicitly by rxdemo
 * client.c. 
 * 
 * \code
 * int StartRXDEMO_Getfile(z_call, a_nameToRead) 
 * 	register struct rx_call *z_call; 
 * char * a_nameToRead; 
 * { 
 * 	static int z_op = 2; 
 * 	int z_result; 
 * 	XDR z_xdrs; 
 * 	xdrrx_create(&z_xdrs, z_call, XDR_ENCODE); 
 * 	/* Marshal the arguments */
 * 	if ((!xdr_int(&z_xdrs, &z_op)) || (!xdr_string(&z_xdrs, &a_nameToRead,
 * 	RXDEMO_NAME_MAX_CHARS))) { 
 * 		z_result = RXGEN_CC_MARSHAL; 
 * 		goto fail; 
 * 	} 
 * 	z_result = RXGEN_SUCCESS; 
 * 	fail: return z_result; 
 * } 
 * \endcode
 * 
 * \par
 * The final stub routine appearing in this generated file, EndRXDEMO
 * Getfile(), handles the case where rxdemo client.c has already successfully
 * recovered the unbounded streamed data appearing on the call, and then simply
 * has to fetch the OUT parameter. This routine behaves identially to the
 * latter portion of RXDEMO Getfile(). 
 * 
 * \code
 * int EndRXDEMO_Getfile(z_call, a_result) 
 * 	register struct rx_call *z_call; 
 * int * a_result; 
 * { 
 * 	int z_result; 
 * 	XDR z_xdrs; 
 * 	/* Un-marshal the reply arguments */
 * 	xdrrx_create(&z_xdrs, z_call, XDR_DECODE); 
 * 	if ((!xdr_int(&z_xdrs, a_result))) { 
 * 		z_result = RXGEN_CC_UNMARSHAL; 
 * 		goto fail; 
 * 	} 
 * 	z_result = RXGEN_SUCCESS; fail: 
 * 	return z_result; 
 * } 
 * \endcode
 * 
 * 	\subsection sec6-3-2 Server-Side Routines: rxdemo.ss.c 
 * 
 * \par
 * This generated file provides the core components required to implement the
 * server side of the rxdemo RPC service. Included in this file is the
 * generated dispatcher routine, RXDEMO ExecuteRequest(), which the rx
 * NewService() invocation in rxdemo server.c uses to construct the body of
 * each listener thread's loop. Also included are the server-side stubs to
 * handle marshalling and unmarshalling of parameters for each defined RPC call
 * (i.e., RXDEMO Add() and RXDEMO Getfile()). These stubs are called by RXDEMO
 * ExecuteRequest(). The routine to be called by RXDEMO ExecuteRequest()
 * depends on the opcode received, which appears as the very first longword in
 * the call data. 
 * \par
 * As usual, the first fragment is copyright information followed by the body
 * of the definitions from the interface file. 
 * 
 * \code
 * /*======================================================================% 
 * % Edward R. Zayas % % Transarc Corporation % % % % % % The United States 
 * Government has rights in this work pursuant % % to contract no. 
 * MDA972-90-C-0036 between the United States Defense % % Advanced Research 
 * Projects Agency and Transarc Corporation. % % % % (C) Copyright 1991 
 * Transarc Corporation % % % % Redistribution and use in source and binary 
 * forms are permitted % % provided that: (1) source distributions retain 
 * this entire copy¬% % right notice and comment, and (2) distributions 
 * including binaries % 
 * %====================================================================== */
 * /* Machine generated file --Do NOT edit */
 * 
 * #include "rxdemo.h" 
 * #include <rx/rx.h> 
 * #include <rx/rx_null.h> 
 * #define RXDEMO_SERVER_PORT 8000 /* Service port to advertise */
 * #define RXDEMO_SERVICE_PORT 0 /* User server's port */
 * #define RXDEMO_SERVICE_ID 4 /* Service ID */
 * #define RXDEMO_NULL_SECOBJ_IDX 0 /* Index of null security object */
 * #define RXDEMO_MAX 3 
 * #define RXDEMO_MIN 2 
 * #define RXDEMO_NULL 0 
 * #define RXDEMO_NAME_MAX_CHARS 64 
 * #define RXDEMO_BUFF_BYTES 512 
 * #define RXDEMO_CODE_SUCCESS 0 
 * #define RXDEMO_CODE_CANT_OPEN 1 
 * #define RXDEMO_CODE_CANT_STAT 2 
 * #define RXDEMO_CODE_CANT_READ 3 
 * #define RXDEMO_CODE_WRITE_ERROR 4 
 * \endcode
 * 
 * \par
 * After this preamble, the first server-side stub appears. This RXDEMO Add()
 * routine is basically the inverse of the RXDEMO Add() client-side stub
 * defined in rxdemo.cs.c. Its job is to unmarshall the IN parameters for the
 * call, invoke the "true" server-side RXDEMO Add() routine (defined in rxdemo
 * server.c), and then package and ship the OUT parameter. Being so similar to
 * the client-side RXDEMO Add(), no further discussion is offered here. 
 * 
 * \code
 * long _RXDEMO_Add(z_call, z_xdrs) 
 * 	struct rx_call *z_call; 
 * XDR *z_xdrs; 
 * { 
 * 	long z_result; 
 * 	int a, b; 
 * 	int result; 
 * 	if ((!xdr_int(z_xdrs, &a)) || (!xdr_int(z_xdrs, &b))) 
 * 	{ 
 * 		z_result = RXGEN_SS_UNMARSHAL; 
 * 		goto fail; 
 * 	} 
 * 	z_result = RXDEMO_Add(z_call, a, b, &result); 
 * 	z_xdrs->x_op = XDR_ENCODE; 
 * 	if ((!xdr_int(z_xdrs, &result))) 
 * 		z_result = RXGEN_SS_MARSHAL; 
 * 	fail: return z_result; 
 * } 
 * \endcode
 * 
 * \par
 * The second server-side stub, RXDEMO Getfile(), appears next. It operates
 * identically to RXDEMO Add(), first unmarshalling the IN arguments, then
 * invoking the routine that actually performs the server-side work for the
 * call, then finishing up by returning the OUT parameters. 
 * 
 * \code
 * long _RXDEMO_Getfile(z_call, z_xdrs) 
 * 	struct rx_call *z_call; 
 * XDR *z_xdrs; 
 * { 
 * 	long z_result; 
 * 	char * a_nameToRead=(char *)0; 
 * 	int a_result; 
 * 	if ((!xdr_string(z_xdrs, &a_nameToRead, RXDEMO_NAME_MAX_CHARS))) { 
 * 		z_result = RXGEN_SS_UNMARSHAL; 
 * 		goto fail; 
 * 	} 
 * 	z_result = RXDEMO_Getfile(z_call, a_nameToRead, &a_result); 
 * 	z_xdrs->x_op = XDR_ENCODE; 
 * 	if ((!xdr_int(z_xdrs, &a_result))) 
 * 		z_result = RXGEN_SS_MARSHAL; 
 * 	fail: z_xdrs->x_op = XDR_FREE; 
 * 	if (!xdr_string(z_xdrs, &a_nameToRead, RXDEMO_NAME_MAX_CHARS)) 
 * 		goto fail1; 
 * 	return z_result; 
 * 	fail1: return RXGEN_SS_XDRFREE; 
 * } 
 * \endcode
 * 
 * \par
 * The next portion of the automatically generated server-side module sets up
 * the dispatcher routine for incoming Rx calls. The above stub routines are
 * placed into an array in opcode order. 
 * 
 * \code
 * long _RXDEMO_Add(); 
 * long _RXDEMO_Getfile(); 
 * static long (*StubProcsArray0[])() = {_RXDEMO_Add, _RXDEMO_Getfile}; 
 * \endcode
 * 
 * \par
 * The dispatcher routine itself, RXDEMO ExecuteRequest, appears next. This is
 * the function provided to the rx NewService() call in rxdemo server.c, and it
 * is used as the body of each listener thread's service loop. When activated,
 * it decodes the first longword in the given Rx call, which contains the
 * opcode. It then dispatches the call based on this opcode, invoking the
 * appropriate server-side stub as organized in the StubProcsArray. 
 * 
 * \code
 * RXDEMO_ExecuteRequest(z_call) 
 * 	register struct rx_call *z_call; 
 * { 
 * 	int op; 
 * 	XDR z_xdrs; 
 * 	long z_result; 
 * 	xdrrx_create(&z_xdrs, z_call, XDR_DECODE); 
 * 	if (!xdr_int(&z_xdrs, &op)) 
 * 		z_result = RXGEN_DECODE; 
 * 	else if (op < RXDEMO_LOWEST_OPCODE || op > RXDEMO_HIGHEST_OPCODE) 
 * 		z_result = RXGEN_OPCODE; 
 * 	else 
 * 		z_result = (*StubProcsArray0[op -RXDEMO_LOWEST_OPCODE])(z_call,
 * 		&z_xdrs); 
 * 	return z_result; 
 * } 
 * \endcode
 * 
 * 	\subsection sec6-3-3 External Data Rep file: rxdemo.xdr.c 
 * 
 * \par
 * This file is created to provide the special routines needed to map any
 * user-defined structures appearing as Rx arguments into and out of network
 * byte order. Again, all on-thewire data appears in network byte order,
 * insuring proper communication between servers and clients with different
 * memory organizations. 
 * \par
 * Since the rxdemo example application does not define any special structures
 * to pass as arguments in its calls, this generated file contains only the set
 * of definitions appearing in the interface file. In general, though, should
 * the user define a struct xyz and use it as a parameter to an RPC function,
 * this file would contain a routine named xdr xyz(), which converted the
 * structure field-by-field to and from network byte order. 
 * 
 * \code
 * /*======================================================================% 
 * %% % in the documentation or other materials mentioning features or % % 
 * use of this software. Neither the name of Transarc nor the names % % of 
 * its contributors may be used to endorse or promote products % % derived 
 * from this software without specific prior written % % permission. % % % 
 * % THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED % 
 * % WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF % 
 * % MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. % 
 * % Edward R. Zayas % Transarc Corporation % % % The United States 
 * Government has rights in this work pursuant to contract no. 
 * MDA972-90-C-0036 between the United States Defense % Advanced Research 
 * Projects Agency and Transarc Corporation. % % (C) Copyright 1991 Transarc 
 * Corporation % % Redistribution and use in source and binary forms are 
 * permitted % % provided that: (1) source distributions retain this entire 
 * copy¬ % right notice and comment, and (2) distributions including binaries 
 * % % display the following acknowledgement: % % % % ``This product includes 
 * software developed by Transarc % % Corporation and its contributors'' % 
 * %====================================================================== */
 * /* Machine generated file --Do NOT edit */
 * 
 * #include "rxdemo.h" 
 * #include <rx/rx.h> 
 * #include <rx/rx_null.h> 
 * #define RXDEMO_SERVER_PORT 8000 /* Service port to advertise */
 * #define RXDEMO_SERVICE_PORT 0 /* User server's port */
 * #define RXDEMO_SERVICE_ID 4 /* Service ID */
 * #define RXDEMO_NULL_SECOBJ_IDX 0 /* Index of null security object */
 * #define RXDEMO_MAX 3 
 * #define RXDEMO_MIN 2 
 * #define RXDEMO_NULL 0 
 * #define RXDEMO_NAME_MAX_CHARS 64 
 * #define RXDEMO_BUFF_BYTES 512 
 * #define RXDEMO_CODE_SUCCESS 0 
 * #define RXDEMO_CODE_CANT_OPEN 1 
 * #define RXDEMO_CODE_CANT_STAT 2 
 * #define RXDEMO_CODE_CANT_READ 3 
 * #define RXDEMO_CODE_WRITE_ERROR 4 
 * \endcode
 * 
 * 	\section sec6-4 Section 6.4: Sample Output 
 * 
 * \par
 * This section contains the output generated by running the example rxdemo
 * server and rxdemo client programs described above. The server end was run on
 * a machine named Apollo, and the client program was run on a machine named
 * Bigtime. 
 * \par
 * The server program on Apollo was started as follows: 
 * \li apollo: rxdemo_server 
 * \li rxdemo_server: Example Rx server process 
 * \li Listening on UDP port 8000 
 * \par
 * At this point, rxdemo server has initialized its Rx module and started up
 * its listener LWPs, which are sleeping on the arrival of an RPC from any
 * rxdemo client. 
 * \par
 * The client portion was then started on Bigtime: 
 * \n bigtime: rxdemo_client apollo 
 * \n rxdemo: Example Rx client process 
 * \n Connecting to Rx server on 'apollo', IP address 0x1acf37c0, UDP port 8000 
 * \n ---> Connected. Asking server to add 1 and 2: Reported sum is 3 
 * \par
 * The command line instructs rxdemo client to connect to the rxdemo server on
 * host apollo and to use the standard port defined for this service. It
 * reports on the successful Rx connection establishment, and immediately
 * executes an rxdemo Add(1, 2) RPC. It reports that the sum was successfully
 * received. When the RPC request arrived at the server and was dispatched by
 * the rxdemo server code, it printed out the following line: 
 * \n [Handling call to RXDEMO_Add(1, 2)] 
 * \par
 * Next, rxdemo client prompts for the name of the file to read from the rxdemo
 * server. It is told to fetch the Makefile for the Rx demo directory. The
 * server is executing in the same directory in which it was compiled, so an
 * absolute name for the Makefile is not required. The client echoes the
 * following: 
 * \n Name of file to read from server: Makefile Setting up an Rx call for
 * RXDEMO_Getfile...done 
 * \par
 * As with the rxdemo Add() call, rxdemo server receives this RPC, and prints
 * out the following information: 
 * \li [Handling call to RXDEMO_Getfile(Makefile)] 
 * \li [file opened] 
 * \li [file has 2450 bytes] 
 * \li [file closed] 
 * \par
 * It successfully opens the named file, and reports on its size in bytes. The
 * rxdemo server program then executes the streamed portion of the rxdemo
 * Getfile call, and when complete, indicates that the file has been closed.
 * Meanwhile, rxdemo client prints out the reported size of the file, follows
 * it with the file's contents, then advises that the test run has completed: 
 * 
 * \code
 * [file contents (2450 bytes) fetched over the Rx call appear below] 
 * 
 * /*#=======================================================================# 
 * # The United States Government has rights in this work pursuant # # to 
 * contract no. MDA972-90-C-0036 between the United States Defense # # Advanced 
 * Research Projects Agency and Transarc Corporation. # # # # (C) Copyright 
 * 1991 Transarc Corporation # # # # Redistribution and use in source and 
 * binary forms are permitted # # provided that: (1) source distributions 
 * retain this entire copy-# # right notice and comment, and (2) distributions 
 * including binaries # # display the following acknowledgement: # # # # ''This 
 * product includes software developed by Transarc # # Corporation and its 
 * contributors'' # # # # in the documentation or other materials mentioning 
 * features or # # use of this software. Neither the name of Transarc nor the 
 * names # # of its contributors may be used to endorse or promote products # 
 * # derived from this software without specific prior written # # permission. 
 * # # # # THIS SOFTWARE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED 
 * # # WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF # # 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. # 
 * #=======================================================================# */
 * 
 * SHELL = /bin/sh 
 * TOOL_CELL = grand.central.org 
 * AFS_INCLIB_CELL = transarc.com 
 * USR_CONTRIB = /afs/${TOOL_CELL}/darpa/usr/contrib/ 
 * PROJ_DIR = ${USR_CONTRIB}.site/grand.central.org/rxdemo/ 
 * AFS_INCLIB_DIR = /afs/${AFS_INCLIB_CELL}/afs/dest/ 
 * RXGEN = ${AFS_INCLIB_DIR}bin/rxgen 
 * INSTALL = ${AFS_INCLIB_DIR}bin/install 
 * LIBS = 	${AFS_INCLIB_DIR}lib/librx.a \ ${AFS_INCLIB_DIR}lib/liblwp.a 
 * 	CFLAGS = -g \ 
 * 	-I. \ 
 * 	-I${AFS_INCLIB_DIR}include \ 
 * 	-I${AFS_INCLIB_DIR}include/afs \ 
 * 	-I${AFS_INCLIB_DIR} \ 
 * 	-I/usr/include 
 * 
 * system: install 
 * 
 * install: all 
 * 	${INSTALL} rxdemo_client ${PROJ_DIR}bin 
 * 	${INSTALL} rxdemo_server ${PROJ_DIR}bin 
 * 
 * all: rxdemo_client rxdemo_server 
 * 
 * rxdemo_client: rxdemo_client.o ${LIBS} rxdemo.cs.o ${CC} ${CFLAGS} 
 * 	-o rxdemo_client rxdemo_client.o rxdemo.cs.o ${LIBS} 
 * 
 * rxdemo_server: rxdemo_server.o rxdemo.ss.o ${LIBS} ${CC} ${CFLAGS} 
 * 	-o rxdemo_server rxdemo_server.o rxdemo.ss.o ${LIBS} 
 * 
 * rxdemo_client.o: rxdemo.h 
 * 
 * rxdemo_server.o: rxdemo.h 
 * 
 * rxdemo.cs.c rxdemo.ss.c rxdemo.er.c rxdemo.h: rxdemo.xg rxgen rxdemo.xg 
 * 
 * clean: rm -f *.o rxdemo.cs.c rxdemo.ss.c rxdemo.xdr.c rxdemo.h \ 
 * 	rxdemo_client rxdemo_server core 
 * 
 * [End of file data] 
 * rxdemo complete. 
 * \endcode
 * 
 * \par
 * The rxdemo server program continues to run after handling these calls,
 * offering its services to any other callers. It can be killed by sending it
 * an interrupt signal using Control-C (or whatever mapping has been set up for
 * the shell's interrupt character). 
 * 
 * 	\section Bibliography Bibliography 
 * 
 * \li [1] Transarc Corporation. AFS 3.0 System Administrator's Guide,
 * F-30-0-D102, Pittsburgh, PA, April 1990. 
 * \li [2] S.P. Miller, B.C. Neuman, J.I. Schiller, J.H. Saltzer. Kerberos
 * Authentication and Authorization System, Project Athena Technical Plan,
 * Section E.2.1, M.I.T., December 1987. 
 * \li [3] Bill 	Bryant. Designing an Authentication System: a Dialogue
 * in Four Scenes, Project Athena internal document, M.I.T, draft of 8 February
 * 1988. 
 * \li [4] S. R. Kleinman. 	Vnodes: An Architecture for Multiple file
 * System Types in Sun UNIX, Conference Proceedings, 1986 Summer Usenix
 * Technical Conference, pp. 238-247, El Toro, CA, 1986. 
 *
 * @}
 */
