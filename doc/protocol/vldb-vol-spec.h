/*!
  \addtogroup vldb-spec VLDB Server Interface
  @{

	\page title AFS-3 Programmer's Reference: Volume Server/Volume Location
Server  Interface 

	\author Edward R. Zayas 
Transarc Corporation 
\version 1.0
\date 29 August 1991 14:48 Copyright 1991 Transarc Corporation All Rights
Reserved FS-00-D165 


	\page chap1 Chapter 1: Overview 

	\section sec1-1 Section 1.1: Introduction 

\par
This document describes the architecture and interfaces for two of the
important agents of the AFS distributed file system, the Volume Server and the
Volume Location Server. The Volume Server allows operations affecting entire
AFS volumes to be executed, while the Volume Location Server provides a lookup
service for volumes, identifying the server or set of servers on which volume
instances reside. 

	\section sec1-2 Section 1.2: Volumes 

	\subsection sec1-2-1 Section 1.2.1: Definition 

\par
The underlying concept manipulated by the two AFS servers examined by this
document is the volume. Volumes are the basic mechanism for organizing the data
stored within the file system. They provide the foundation for addressing,
storing, and accessing file data, along with serving as the administrative
units for replication, backup, quotas, and data motion between File Servers. 
\par
Specifically, a volume is a container for a hierarchy of files, a connected
file system subtree. In this respect, a volume is much like a traditional unix
file system partition. Like a partition, a volume can be mounted in the sense
that the root directory of the volume can be named within another volume at an
AFS mount point. The entire file system hierarchy is built up in this manner,
using mount points to glue together the individual subtrees resident within
each volume. The root of this hierarchy is then mounted by each AFS client
machine using a conventional unix mount point within the workstation's local
file system. By convention, this entryway into the AFS domain is mounted on the
/afs local directory. From a user's point of view, there is only a single mount
point to the system; the internal mount points are generally transparent. 

	\subsection sec1-2-2 Section 1.2.2: Volume Naming 

\par
There are two methods by which volumes may be named. The first is via a
human-readable string name, and the second is via a 32-bit numerical
identifier. Volume identifiers, whether string or numerical, must be unique
within any given cell. AFS mount points may use either representation to
specify the volume whose root directory is to be accessed at the given
position. Internally, however, AFS agents use the numerical form of
identification exclusively, having to translate names to the corresponding
32-bit value. 

	\subsection sec1-2-3 Section 1.2.3: Volume Types 

\par
There are three basic volume types: read-write, read-only, and backup volumes. 
\li Read-write: The data in this volume may be both read and written by those
clients authorized to do so. 
\li Read-only: It is possible to create one or more read-only snapshots of
read-write volumes. The read-write volume serving as the source image is
referred to as the parent volume. Each read-only clone, or child, instance must
reside on a different unix disk partition than the other clones. Every clone
instance generated from the same parent read-write volume has the identical
volume name and numerical volume ID. This is the reason why no two clones may
appear on the same disk partition, as there would be no way to differentiate
the two. AFS clients are allowed to read files and directories from read-only
volumes, but cannot overwrite them individually. However, it is possible to
make changes to the read-write parent and then release the contents of the
entire volume to all the read-only replicas. The release operation fails if it
does not reach the appropriate replication sites. 
\li Backup: A backup volume is a special instance of a read-only volume. While
it is also a read-only snapshot of a given read-write volume, only one instance
is allowed to exist at any one time. Also, the backup volume must reside on the
same partition as the parent read-write volume from which it was created. It is
from a backup volume that the AFS backup system writes file system data to
tape. In addition, backup volumes may be mounted into the file tree just like
the other volume types. In fact, by convention, the backup volume for each
user's home directory subtree is typically mounted as OldFiles in that
directory. If a user accidentally deletes a file that resides in the backup
snapshot, the user may simply copy it out of the backup directly without the
assistance of a system administrator, or any kind of tape restore operation. 
Backup volume are implemented in a copy-on-write fashion. Thus, backup volumes
may be envisioned as consisting of a set of pointers to the true data objects
in the base read-write volume when they are first created. When a file is
overwritten in the read-write version for the first time after the backup
volume was created, the original data is physically written to the backup
volume, breaking the copyon-write link. With this mechanism, backup volumes
maintain the image of the read-write volume at the time the snapshot was taken
using the minimum amount of additional disk space. 

	\section sec1-3 Section 1.3: Scope 

\par
This paper is a member of a documentation suite providing specifications of the
operation and interfaces offered by the various AFS servers and agents. The
scope of this work is to provide readers with a sufficiently detailed
description of the Volume Location Server and the Volume Server so that they
may construct client applications which call their RPC interface routines. 

	\section sec1-4 Section 1.4: Document Layout 

\par
After this introductory portion of the document, Chapters 2 and 3 examine the
architecture and RPC interface of the Volume Location Server and its replicated
database. Similarly, Chapters 4 and 5 describe the architecture and RPC
interface of the Volume Server. 

	\page chap2 Chapter 2: Volume Location Server Architecture 

	\section sec2-1 Section 2.1: Introduction 

\par
The Volume Location Server allows AFS agents to query the location and basic
status of volumes resident within the given cell. Volume Location Server
functions may be invoked directly from authorized users via the vos utility. 
\par
This chapter briefly discusses various aspects of the Volume Location Server's
architecture. First, the need for volume location is examined, and the specific
parties that call the Volume Location Server interface routines are identified.
Then, the database maintained to provide volume location service, the Volume
Location Database (VLDB), is examined. Finally, the vlserver process which
implements the Volume Location Server is considered. 
\par
As with all AFS servers, the Volume Location Server uses the Rx remote
procedure call package for communication with its clients. 

	\section sec2-2 Section 2.2: The Need For Volume Location 

\par
The Cache Manager agent is the primary consumer of AFS volume location service,
on which it is critically dependent for its own operation. The Cache Manager
needs to map volume names or numerical identifiers to the set of File Servers
on which its instances reside in order to satisfy the file system requests it
is processing on behalf of it clients. Each time a Cache Manager encounters a
mount point for which it does not have location information cached, it must
acquire this information before the pathname resolution may be successfully
completed. Once the File Server set is known for a particular volume, the Cache
Manager may then select the proper site among them (e.g. choosing the single
home for a read-write volume, or randomly selecting a site from a read-only
volume's replication set) and begin addressing its file manipulation operations
to that specific server. 
\par
While the Cache Manager consults the volume location service, it is not capable
of changing the location of volumes and hence modifying the information
contained therein. This capability to perform acts which change volume location
is concentrated within the Volume Server. The Volume Server process running on
each server machine manages all volume operations affecting that platform,
including creations, deletions, and movements between servers. It must update
the volume location database every time it performs one of these actions. 
\par
None of the other AFS system agents has a need to access the volume location
database for its site. Surprisingly, this also applies to the File Server
process. It is only aware of the specific set of volumes that reside on the set
of physical disks directly attached to the machine on which they execute. It
has no knowlege of the universe of volumes resident on other servers, either
within its own cell or in foreign cells. 

	\section sec2-3 Section 2.3: The VLDB 

\par
The Volume Location Database (VLDB) is used to allow AFS application programs
to discover the location of any volume within its cell, along with select
information about the nature and state of that volume. It is organized in a
very straightforward fashion, and uses the ubik [4] [5] facility to to provide
replication across multiple server sites. 

	\subsection sec2-3-1 Section 2.3.1: Layout 

\par
The VLDB itself is a very simple structure, and synchronized copies may be
maintained at two or more sites. Basically, each copy consists of header
information, followed by a linear (yet unbounded) array of entries. There are
several associated hash tables used to perform lookups into the VLDB. The first
hash table looks up volume location information based on the volume's name.
There are three other hash tables used for lookup, based on volume ID/type
pairs, one for each possible volume type. 
\par
The VLDB for a large site may grow to contain tens of thousands of entries, so
some attempts were made to make each entry as small as possible. For example,
server addresses within VLDB entries are represented as single-byte indicies
into a table containing the full longword IP addresses. 
\par
A free list is kept for deleted VLDB entries. The VLDB will not grow unless all
the entries on the free list have been exhausted, keeping it as compact as
possible. 

	\subsection sec2-3-2 Section 2.3.2: Database Replication 

\par
The VLDB, along with other important AFS databases, may be replicated to
multiple sites to improve its availability. The ubik replication package is
used to implement this functionality for the VLDB. A full description of ubik
and of the quorum completion algorithm it implements may be found in [4] and
[5]. The basic abstraction provided by ubik is that of a disk file replicated
to multiple server locations. One machine is considered to be the
synchronization site, handling all write operations on the database file. Read
operations may be directed to any of the active members of the quorum, namely a
subset of the replication sites large enough to insure integrity across such
failures as individual server crashes and network partitions. All of the quorum
members participate in regular elections to determine the current
synchronization site. The ubik algorithms allow server machines to enter and
exit the quorum in an orderly and consistent fashion. All operations to one of
these replicated "abstract files" are performed as part of a transaction. If
all the related operations performed under a transaction are successful, then
the transaction is committed, and the changes are made permanent. Otherwise,
the transaction is aborted, and all of the operations for that transaction are
undone. 

	\section sec2-4 Section 2.4: The vlserver Process 

\par
The user-space vlserver process is in charge of providing volume location
service for AFS clients. This program maintains the VLDB replica at its
particular server, and cooperates with all other vlserver processes running in
the given cell to propagate updates to the database. It implements the RPC
interface defined in the vldbint.xg definition file for the rxgen RPC stub
generator program. As part of its startup sequence, it must discover the VLDB
version it has on its local disk, move to join the quorum of replication sites
for the VLDB, and get the latest version if the one it came up with was out of
date. Eventually, it will synchronize with the other VLDB replication sites,
and it will begin accepting calls. 
\par
The vlserver program uses at most three Rx worker threads to listen for
incoming Volume Location Server calls. It has a single, optional command line
argument. If the string "-noauth" appears when the program is invoked, then
vlserver will run in an unauthenticated mode where any individual is considered
authorized to perform any VLDB operation. This mode is necessary when first
bootstrapping an AFS installation. 

	\page chap3 Chapter 3: Volume Location Server Interface 

	\section sec3-1 Section 3.1: Introduction 

\par
This chapter documents the API for the Volume Location Server facility, as
defined by the vldbint.xg Rxgen interface file and the vldbint.h include file.
Descriptions of all the constants, structures, macros, and interface functions
available to the application programmer appear here. 
\par
It is expected that Volume Location Server client programs run in user space,
as does the associated vos volume utility. However, the kernel-resident Cache
Manager agent also needs to call a subset of the Volume Location Server's RPC
interface routines. Thus, a second Volume Location Server interface is
available, built exclusively to satisfy the Cache Manager's limited needs. This
subset interface is defined by the afsvlint.xg Rxgen interface file, and is
examined in the final section of this chapter. 

	\section sec3-2 3.2: Constants 

\par
This section covers the basic constant definitions of interest to the Volume
Location Server application programmer. These definitions appear in the
vldbint.h file, automatically generated from the vldbint.xg Rxgen interface
file, and in vlserver.h. 
\par
Each subsection is devoted to describing the constants falling into the
following categories: 
\li Configuration and boundary quantities 
\li Update entry bits 
\li List-by-attribute bits 
\li Volume type indices 
\li States for struct vlentry 
\li States for struct vldbentry 
\li ReleaseType argument values 
\li Miscellaneous items 

	\subsection sec3-2-1 Section 3.2.1: Configuration and Boundary
Quantities 

\par
These constants define some basic system values, including configuration
information. 

\par Name
MAXNAMELEN
\par Value
65
\par Description
Maximum size of various character strings, including volume name fields in
structures and host names.

\par Name
MAXNSERVERS
\par Value
8
\par Description
Maximum number of replications sites for a volume.

\par Name
MAXTYPES
\par Value
3
\par Description
Maximum number of volume types.

\par Name
VLDBVERSION
\par Value
1
\par Description
VLDB database version number

\par Name
HASHSIZE
\par Value
8,191
\par Description
Size of internal Volume Location Server volume name and volume ID hash tables.
This must always be a prime number.

\par Name
NULLO
\par Value
0
\par Description
Specifies a null pointer value.

\par Name
VLDBALLOCCOUNT
\par Value
40
\par Description
Value used when allocating memory internally for VLDB entry records.

\par Name
BADSERVERID
\par Value
255
\par Description
Illegal Volume Location Server host ID.

\par Name
MAXSERVERID
\par Value
30
\par Description
Maximum number of servers appearing in the VLDB.

\par Name
MAXSERVERFLAG
\par Value
0x80
\par Description
First unused flag value in such fields as serverFlags in struct vldbentry and
RepsitesNewFlags in struct VldbUpdateEntry.

\par Name
MAXPARTITIONID
\par Value
126
\par Description
Maximum number of AFS disk partitions for any one server.

\par Name
MAXBUMPCOUNT
\par Value
0x7fffffff
\par Description
Maximum interval that the current high-watermark value for a volume ID can be
increased in one operation.

\par Name
MAXLOCKTIME
\par Value
0x7fffffff
\par Description
Maximum number of seconds that any VLDB entry can remain locked.

\par Name
SIZE
\par Value
1,024
\par Description
Maximum size of the name field within a struct.

	\subsection sec3-2-2 Section 3.2.2: Update Entry Bits 

\par
These constants define bit values for the Mask field in the struct
VldbUpdateEntry. Specifically, setting these bits is equivalent to declaring
that the corresponding field within an object of type struct VldbUpdateEntry
has been set. For example, setting the VLUPDATE VOLUMENAME flag in Mask
indicates that the name field contains a valid value. 

\par Name
VLUPDATE VOLUMENAME
\par Value
0x0001
\par Description
If set, indicates that the name field is valid.

\par Name
VLUPDATE VOLUMETYPE
\par Value
0x0002
\par Description
If set, indicates that the volumeType field is valid.

\par Name
VLUPDATE FLAGS
\par Value
0x0004
\par Description
If set, indicates that the flags field is valid.

\par Name
VLUPDATE READONLYID
\par Value
0x0008
\par Description
If set, indicates that the ReadOnlyId field is valid.

\par Name
VLUPDATE BACKUPID
\par Value
0x0010
\par Description
If set, indicates that the BackupId field is valid.

\par Name
VLUPDATE REPSITES 
\par Value
0x0020
\par Description
If set, indicates that the nModifiedRepsites field is valid.

\par Name
VLUPDATE CLONEID
\par Value
0x0080
\par Description
If set, indicates that the cloneId field is valid.

\par Name
VLUPDATE REPS DELETE
\par Value
0x0100
\par Description
Is the replica being deleted?

\par Name
VLUPDATE REPS ADD
\par Value
0x0200
\par Description
Is the replica being added?

\par Name
VLUPDATE REPS MODSERV
\par Value
0x0400
\par Description
Is the server part of the replica location correct?

\par Name
VLUPDATE REPS MODPART
\par Value
0x0800
\par Description
Is the partition part of the replica location correct?

\par Name
VLUPDATE REPS MODFLAG 
\par Value
0x1000 
\par Description
Various modification flag values.

	\subsection sec3-2-3 Section 3.2.3: List-By-Attribute Bits 

\par
These constants define bit values for the Mask field in the struct
VldbListByAttributes is to be used in a match. Specifically, setting these bits
is equivalent to declaring that the corresponding field within an object of
type struct VldbListByAttributes is set. For example, setting the VLLIST SERVER
flag in Mask indicates that the server field contains a valid value. 

\par Name
VLLIST SERVER
\par Value
0x1
\par Description
If set, indicates that the server field is valid.

\par Name
VLLIST PARTITION
\par Value
0x2
\par Description
If set, indicates that the partition field is valid.

\par Name
VLLIST VOLUMETYPE
\par Value
0x4
\par Description
If set, indicates that the volumetype field is valid.

\par Name
VLLIST VOLUMEID
\par Value
0x8
\par Description
If set, indicates that the volumeid field is valid.

\par Name
VLLIST FLAG
\par Value
0x10
\par Description
If set, indicates that that flag field is valid.

	\subsection sec3-2-4 Section 3.2.4: Volume Type Indices 

\par
These constants specify the order of entries in the volumeid array in an object
of type struct vldbentry. They also identify the three different types of
volumes in AFS. 

\par Name
RWVOL
\par Value
0
\par Description
Read-write volume.

\par Name
ROVOL
\par Value
1
\par Description
Read-only volume.

\par Name
BACKVOL 
\par Value
2
\par Description
Backup volume.

	\subsection sec3-2-5 Section 3.2.5: States for struct vlentry 

\par
The following constants appear in the flags field in objects of type struct
vlentry. The first three values listed specify the state of the entry, while
all the rest stamp the entry with the type of an ongoing volume operation, such
as a move, clone, backup, deletion, and dump. These volume operations are the
legal values to provide to the voloper parameter of the VL SetLock() interface
routine. 
\par
For convenience, the constant VLOP ALLOPERS is defined as the inclusive OR of
the above values from VLOP MOVE through VLOP DUMP. 

\par Name
VLFREE
\par Value
0x1
\par Description
Entry is in the free list.

\par Name
VLDELETED
\par Value
0x2
\par Description
Entry is soft-deleted.

\par Name
VLLOCKED
\par Value
0x4
\par Description
Advisory lock held on the entry.

\par Name
VLOP MOVE
\par Value
0x10
\par Description
The associated volume is being moved between servers.

\par Name
VLOP RELEASE
\par Value
0x20
\par Description
The associated volume is being cloned to its replication sites.

\par Name
VLOP BACKUP
\par Value
0x40
\par Description
A backup volume is being created for the associated volume.

\par Name
VLOP DELETE
\par Value
0x80
\par Description
The associated volume is being deleted.

\par Name
VLOP DUMP
\par Value
0x100 
\par Description
A dump is being taken of the associated volume.

	\subsection sec3-2-6 Section 3.2.6: States for struct vldbentry 

\par
Of the following constants, the first three appear in the flags field within an
object of type struct vldbentry, advising of the existence of the basic volume
types for the given volume, and hence the validity of the entries in the
volumeId array field. The rest of the values provided in this table appear in
the serverFlags array field, and apply to the instances of the volume appearing
in the various replication sites. 
\par
This structure appears in numerous Volume Location Server interface calls,
namely VL CreateEntry(), VL GetEntryByID(), VL GetEntryByName(), VL
ReplaceEntry() and VL ListEntry(). 

\par Name
VLF RWEXISTS
\par Value
0x1000
\par Description
The read-write volume ID is valid.

\par Name
VLF ROEXISTS
\par Value
0x2000
\par Description
The read-only volume ID is valid.

\par Name
VLF BACKEXISTS
\par Value
0x4000
\par Description
The backup volume ID is valid.

\par Name
VLSF NEWREPSITE
\par Value
0x01
\par Description
Not used; originally intended to mark an entry as belonging to a
partially-created volume instance.

\par Name
VLSF ROVOL
\par Value
0x02
\par Description
A read-only version of the volume appears at this server.

\par Name
VLSF RWVOL
\par Value
0x02
\par Description
A read-write version of the volume appears at this server.

\par Name
VLSF BACKVOL
\par Value
0x08
\par Description
A backup version of the volume appears at this server. 

	\subsection sec3-2-7 Section 3.2.7: ReleaseType Argument Values 

\par
The following values are used in the ReleaseType argument to various Volume
Location Server interface routines, namely VL ReplaceEntry(), VL UpdateEntry()
and VL ReleaseLock(). 

\par Name
LOCKREL TIMESTAMP
\par Value
1
\par Description
Is the LockTimestamp field valid?

\par Name
LOCKREL OPCODE
\par Value
2
\par Description
Are any of the bits valid in the flags field?

\par Name
LOCKREL AFSID
\par Value
4
\par Description
Is the LockAfsId field valid?

	\subsection sec3-2-8 Section 3.2.8: Miscellaneous 

\par
Miscellaneous values. 
\par Name
VLREPSITE NEW 	
\par Value
1 	
\par Description
Has a replication site gotten a new release of a volume? 
\par 
A synonym for this constant is VLSF NEWREPSITE. 

	\section sec3-3 Section 3.3: Structures and Typedefs 

\par
This section describes the major exported Volume Location Server data
structures of interest to application programmers, along with the typedefs
based upon those structures. 

	\subsection sec3-3-1 Section 3.3.1: struct vldbentry 

\par
This structure represents an entry in the VLDB as made visible to Volume
Location Server clients. It appears in numerous Volume Location Server
interface calls, namely VL CreateEntry(), VL GetEntryByID(), VL
GetEntryByName(), VL ReplaceEntry() and VL ListEntry(). 
\n \b Fields 
\li char name[] - The string name for the volume, with a maximum length of
MAXNAMELEN (65) characters, including the trailing null. 
\li long volumeType - The volume type, one of RWVOL, ROVOL, or BACKVOL. 
\li long nServers - The number of servers that have an instance of this volume. 
\li long serverNumber[] - An array of indices into the table of servers,
identifying the sites holding an instance of this volume. There are at most
MAXNSERVERS (8) of these server sites allowed by the Volume Location Server. 
\li long serverPartition[] - An array of partition identifiers, corresponding
directly to the serverNumber array, specifying the partition on which each of
those volume instances is located. As with the serverNumber array,
serverPartition has up to MAXNSERVERS (8) entries. 
\li long serverFlags[] - This array holds one flag value for each of the
servers in the previous arrays. Again, there are MAXNSERVERS (8) slots in this
array. 
\li u long volumeId[] - An array of volume IDs, one for each volume type. There
are MAXTYPES slots in this array. 
\li long cloneId - This field is used during a cloning operation. 
\li long flags - Flags concerning the status of the fields within this
structure; see Section 3.2.6 for the bit values that apply. 

	\subsection sec3-3-2 Section 3.3.2: struct vlentry 

\par
This structure is used internally by the Volume Location Server to fully
represent a VLDB entry. The client-visible struct vldbentry represents merely a
subset of the information contained herein. 
\n \b Fields 
\li u long volumeId[] - An array of volume IDs, one for each of the MAXTYPES of
volume types. 
\li long flags - Flags concerning the status of the fields within this
structure; see Section 3.2.6 for the bit values that apply. 
\li long LockAfsId - The individual who locked the entry.  This feature has not
yet been implemented. 
\li long LockTimestamp - Time stamp on the entry lock. 
\li long cloneId - This field is used during a cloning operation. 
\li long AssociatedChain - Pointer to the linked list of associated VLDB
entries. 
\li long nextIdHash[] - Array of MAXTYPES next pointers for the ID hash table
pointer, one for each related volume ID. 
\li long nextNameHash - Next pointer for the volume name hash table. 
\li long spares1[] - Two longword spare fields. 
\li char name[] - The volume's string name, with a maximum of MAXNAMELEN (65)
characters, including the trailing null. 
\li u char volumeType - The volume's type, one of RWVOL, ROVOL, or BACKVOL. 
\li u char serverNumber[] - An array of indices into the table of servers,
identifying the sites holding an instance of this volume. There are at most
MAXNSERVERS (8) of these server sites allowed by the Volume Location Server. 
\li u char serverPartition[] - An array of partition identifiers, corresponding
directly to the serverNumber array, specifying the partition on which each of
those volume instances is located. As with the serverNumber array,
serverPartition has up to MAXNSERVERS (8) entries. 
\li u char serverFlags[] - This array holds one flag value for each of the
servers in the previous arrays. Again, there are MAXNSERVERS (8) slots in this
array. 
\li u char RefCount - Only valid for read-write volumes, this field serves as a
reference count, basically the number of dependent children volumes. 
\li char spares2[] - This field is used for 32-bit alignment. 

	\subsection sec3-3-3 Section 3.3.3: struct vital vlheader 

\par
This structure defines the leading section of the VLDB header, of type struct
vlheader. It contains frequently-used global variables and general statistics
information. 
\n \b Fields 
\li long vldbversion - The VLDB version number. This field must appear first in
the structure. 
\li long headersize - The total number of bytes in the header. 
\li long freePtr - Pointer to the first free enry in the free list, if any. 
\li long eofPtr - Pointer to the first free byte in the header file. 
\li long allocs - The total number of calls to the internal AllocBlock()
function directed at this file. 
\li long frees - The total number of calls to the internal FreeBlock() function
directed at this file. 
\li long MaxVolumeId - The largest volume ID ever granted for this cell. 
\li long totalEntries[] - The total number of VLDB entries by volume type in
the VLDB. This array has MAXTYPES slots, one for each volume type. 

	\subsection sec3-3-4 Section 3.3.4: struct vlheader 

\par
This is the layout of the information stored in the VLDB header. Notice it
includes an object of type struct vital vlheader described above (see Section
3.3.3) as the first field. 
\n \b Fields 
\li struct vital vlheader vital header - Holds critical VLDB header
information. 
\li u long IpMappedAddr[] - Keeps MAXSERVERID+1 mappings of IP addresses to
relative ones. 
\li long VolnameHash[] - The volume name hash table, with HASHSIZE slots. 
\li long VolidHash[][] - The volume ID hash table. The first dimension in this
array selects which of the MAXTYPES volume types is desired, and the second
dimension actually implements the HASHSIZE hash table buckets for the given
volume type. 

	\subsection sec3-3-5 Section 3.3.5: struct VldbUpdateEntry 

\par
This structure is used as an argument to the VL UpdateEntry() routine (see
Section 3.6.7). Please note that multiple entries can be updated at once by
setting the appropriate Mask bits. The bit values for this purpose are defined
in Section 3.2.2. 
\n \b Fields 
\li u long Mask - Bit values determining which fields are to be affected by the
update operation. 
\li char name[] - The volume name, up to MAXNAMELEN (65) characters including
the trailing null. 
\li long volumeType - The volume type. 
\li long flags - This field is used in conjuction with Mask (in fact, one of
the Mask bits determines if this field is valid) to choose the valid fields in
this record. 
\li u long ReadOnlyId - The read-only ID. 
\li u long BackupId - The backup ID. 
\li long cloneId - The clone ID. 
\li long nModifiedRepsites - Number of replication sites whose entry is to be
changed as below. 
\li u long RepsitesMask[] - Array of bit masks applying to the up to
MAXNSERVERS (8) replication sites involved. 
\li long RepsitesTargetServer[] - Array of target servers for the operation, at
most MAXNSERVERS (8) of them. 
\li long RepsitesTargetPart[] - Array of target server partitions for the
operation, at most MAXNSERVERS (8) of them. 
\li long RepsitesNewServer[] - Array of new server sites, at most MAXNSERVERS
(8) of them. 
\li long RepsitesNewPart[] - Array of new server partitions for the operation,
at most MAXNSERVERS (8) of them. 
\li long RepsitesNewFlags[] - Flags applying to each of the new sites, at most
MAXNSERVERS (8) of them. 

	\subsection sec3-3-6 Section 3.3.6: struct VldbListByAttributes 

\par
This structure is used by the VL ListAttributes() routine (see Section 3.6.11). 
\n \b Fields 
\li u long Mask - Bit mask used to select the following attribute fields on
which to match. 
\li long server - The server address to match. 
\li long partition - The partition ID to match. 
\li long volumetype - The volume type to match. 
\li long volumeid - The volume ID to match. 
\li long flag - Flags concerning these values. 

	\subsection sec3-3-7 Section 3.3.7: struct single vldbentry 

\par
This structure is used to construct the vldblist object (See Section 3.3.12),
which basically generates a queueable (singly-linked) version of struct
vldbentry. 
\n \b Fields 
\li vldbentry VldbEntry - The VLDB entry to be queued. 
\li vldblist next vldb - The next pointer in the list. 

	\subsection sec3-3-8 Section 3.3.8: struct vldb list 

\par
This structure defines the item returned in linked list form from the VL
LinkedList() function (see Section 3.6.12). This same object is also returned
in bulk form in calls to the VL ListAttributes() routine (see Section 3.6.11). 
\n \b Fields 
\li vldblist node - The body of the first object in the linked list. 

	\subsection sec3-3-9 Section 3.3.9: struct vldstats 

\par
This structure defines fields to record statistics on opcode hit frequency. The
MAX NUMBER OPCODES constant has been defined as the maximum number of opcodes
supported by this structure, and is set to 30. 
\n \b Fields 
\li unsigned long start time - Clock time when opcode statistics were last
cleared. 
\li long requests[] - Number of requests received for each of the MAX NUMBER
OPCODES opcode types. 
\li long aborts[] - Number of aborts experienced for each of the MAX NUMBER
OPCODES opcode types. 
\li long reserved[] - These five longword fields are reserved for future use. 

	\subsection sec3-3-10 Section 3.3.10: bulk 

\code
typedef opaque bulk<DEFAULTBULK>; 
\endcode
\par
This typedef may be used to transfer an uninterpreted set of bytes across the
Volume Location Server interface. It may carry up to DEFAULTBULK (10,000)
bytes. 
\n \b Fields 
\li bulk len - The number of bytes contained within the data pointed to by the
next field. 
\li bulk val - A pointer to a sequence of bulk len bytes. 

	\subsection sec3-3-11 Section 3.3.11: bulkentries 

\code
typedef vldbentry bulkentries<>; 
\endcode
\par
This typedef is used to transfer an unbounded number of struct vldbentry
objects. It appears in the parameter list for the VL ListAttributes() interface
function. 
\n \b Fields 
\li bulkentries len - The number of vldbentry structures contained within the
data pointed to by the next field. 
\li bulkentries val - A pointer to a sequence of bulkentries len vldbentry
structures. 

	\subsection sec3-3-12 Section 3.3.12: vldblist 

\code
typedef struct single_vldbentry *vldblist; 
\endcode
\par
This typedef defines a queueable struct vldbentry object, referenced by the
single vldbentry typedef as well as struct vldb list. 

	\subsection sec3-3-13 Section 3.3.13: vlheader 

\code
typedef struct vlheader vlheader; 
\endcode
\par
This typedef provides a short name for objects of type struct vlheader (see
Section 3.3.4). 

	\subsection sec3-3-14 Section 3.3.14: vlentry 

\code
typedef struct vlentry vlentry; 
\endcode
\par
This typedef provides a short name for objects of type struct vlentry (see
Section 3.3.2). 

	\section sec3-4 Section 3.4: Error Codes 

\par
This section covers the set of error codes exported by the Volume Location
Server, displaying the printable phrases with which they are associated. 

\par Name
VL IDEXIST
\par Value
(363520L)
\par Description
Volume Id entry exists in vl database.

\par Name
VL IO
\par Value
(363521L)
\par Description
I/O related error.

\par Name
VL NAMEEXIST
\par Value
(363522L)
\par Description
Volume name entry exists in vl database.

\par Name
VL CREATEFAIL
\par Value
(363523L)
\par Description
Internal creation failure.

\par Name
VL NOENT
\par Value
(363524L)
\par Description
No such entry.

\par Name
VL EMPTY
\par Value
(363525L)
\par Description
Vl database is empty.

\par Name
VL ENTDELETED
\par Value
(363526L)
\par Description
Entry is deleted (soft delete).

\par Name
VL BADNAME
\par Value
(363527L)
\par Description
Volume name is illegal.

\par Name
VL BADINDEX
\par Value
(363528L)
\par Description
Index is out of range.

\par Name
VL BADVOLTYPE
\par Value
(363529L)
\par Description
Bad volume range.

\par Name
VL BADSERVER
\par Value
(363530L)
\par Description
Illegal server number (out of range).

\par Name
VL BADPARTITION
\par Value
(363531L)
\par Description
Bad partition number.

\par Name
VL REPSFULL
\par Value
(363532L)
\par Description
Run out of space for Replication sites.

\par Name
VL NOREPSERVER
\par Value
(363533L)
\par Description
No such Replication server site exists.

\par Name
VL DUPREPSERVER
\par Value
(363534L)
\par Description
Replication site already exists.

\par Name
RL RWNOTFOUND
\par Value
(363535L)
\par Description
Parent R/W entry not found.

\par Name
VL BADREFCOUNT
\par Value
(363536L)
\par Description
Illegal Reference Count number.

\par Name
VL SIZEEXCEEDED
\par Value
(363537L)
\par Description
Vl size for attributes exceeded.

\par Name
VL BADENTRY
\par Value
(363538L)
\par Description
Bad incoming vl entry.

\par Name
VL BADVOLIDBUMP
\par Value
(363539L)
\par Description
Illegal max volid increment.

\par Name
VL IDALREADYHASHED
\par Value
(363540L)
\par Description
RO/BACK id already hashed.

\par Name
VL ENTRYLOCKED
\par Value
(363541L)
\par Description
Vl entry is already locked.

\par Name
VL BADVOLOPER
\par Value
(363542L)
\par Description
Bad volume operation code.

\par Name
VL BADRELLOCKTYPE
\par Value
(363543L)
\par Description
Bad release lock type.

\par Name
VL RERELEASE
\par Value
(363544L)
\par Description
Status report: last release was aborted.

\par Name
VL BADSERVERFLAG
\par Value
(363545L)
\par Description
Invalid replication site server flag.

\par Name
VL PERM
\par Value
(363546L)
\par Description
No permission access.

\par Name
VL NOMEM
\par Value
(363547L) 
\par Description
malloc(realloc) failed to alloc enough memory. 

	\section sec3-5 Section 3.5: Macros 

\par
The Volume Location Server defines a small number of macros, as described in
this section. They are used to update the internal statistics variables and to
compute offsets into character strings. All of these macros really refer to
internal operations, and strictly speaking should not be exposed in this
interface. 

	\subsection sec3-5-1 Section 3.5.1: COUNT REQ() 

\code
#define COUNT_REQ(op) 
static int this_op = op-VL_LOWEST_OPCODE; 
dynamic_statistics.requests[this_op]++ 
\endcode
\par
Bump the appropriate entry in the variable maintaining opcode usage statistics
for the Volume Location Server. Note that a static variable is set up to record
this op, namely the index into the opcode monitoring array. This static
variable is used by the related COUNT ABO() macro defined below. 

	\subsection sec3-5-2 Section 3.5.2: COUNT ABO() 

\code
#define COUNT_ABO dynamic_statistics.aborts[this_op]++ 
\endcode
\par
Bump the appropriate entry in the variable maintaining opcode abort statistics
for the Volume Location Server. Note that this macro does not take any
arguemnts. It expects to find a this op variable in its environment, and thus
depends on its related macro, COUNT REQ() to define that variable. 

	\subsection sec3-5-3 Section 3.5.3: DOFFSET() 

\code
#define DOFFSET(abase, astr, aitem) ((abase)+(((char *)(aitem)) -((char
*)(astr)))) 
\endcode
\par
Compute the byte offset of charcter object aitem within the enclosing object
astr, also expressed as a character-based object, then offset the resulting
address by abase. This macro is used ot compute locations within the VLDB when
actually writing out information. 

	\section sec3-6 Section 3.6: Functions 

\par
This section covers the Volume Location Server RPC interface routines. The
majority of them are generated from the vldbint.xg Rxgen file, and are meant to
be used by user-space agents. There is also a subset interface definition
provided in the afsvlint.xg Rxgen file. These routines, described in Section
3.7, are meant to be used by a kernel-space agent when dealing with the Volume
Location Server; in particular, they are called by the Cache Manager. 

	\subsection sec3-6-1 Section 3.6.1: VL CreateEntry - Create a VLDB
entry 

\code
int VL CreateEntry(IN struct rx connection *z conn, 
			IN vldbentry *newentry) 
\endcode
\par Description
This function creates a new entry in the VLDB, as specified in the newentry
argument. Both the name and numerical ID of the new volume must be unique
(e.g., it must not already appear in the VLDB). For non-read-write entries, the
read-write parent volume is accessed so that its reference count can be
updated, and the new entry is added to the parent's chain of associated
entries. 
The VLDB is write-locked for the duration of this operation. 
\par Error Codes 
VL PERM The caller is not authorized to execute this function. VL NAMEEXIST The
volume name already appears in the VLDB. VL CREATEFAIL Space for the new entry
cannot be allocated within the VLDB. VL BADNAME The volume name is invalid. VL
BADVOLTYPE The volume type is invalid. VL BADSERVER The indicated server
information is invalid. VL BADPARTITION The indicated partition information is
invalid. VL BADSERVERFLAG The server flag field is invalid. VL IO An error
occurred while writing to the VLDB. 

	\subsection sec3-6-2 Section 3.6.2: VL DeleteEntry - Delete a VLDB
entry 

\code
int VL DeleteEntry(IN struct rx connection *z conn, 
			IN long Volid, 	
			IN long voltype) 	
\endcode
\par Description 
Delete the entry matching the given volume identifier and volume type as
specified in the Volid and voltype arguments. For a read-write entry whose
reference count is greater than 1, the entry is not actually deleted, since at
least one child (read-only or backup) volume still depends on it. For cases of
non-read-write volumes, the parent's reference count and associated chains are
updated. 
\par
If the associated VLDB entry is already marked as deleted (i.e., its flags
field has the VLDELETED bit set), then no further action is taken, and VL
ENTDELETED is returned. The VLDB is write-locked for the duration of this
operation. 
\par Error Codes 
VL PERM The caller is not authorized to execute this function. VL BADVOLTYPE An
illegal volume type has been specified by the voltype argument. VL NOENT This
volume instance does not appear in the VLDB. VL ENTDELETED The given VLDB entry
has already been marked as deleted. VL IO An error occurred while writing to
the VLDB. 

	\subsection sec3-6-3 Section 3.6.3: VL GetEntryByID - Get VLDB entry by
volume ID/type 

\code
int VL GetEntryByID(IN struct rx connection *z conn, IN long Volid, IN long
voltype, OUT vldbentry *entry) 
\endcode
\par Description 
Given a volume's numerical identifier (Volid) and type (voltype), return a
pointer to the entry in the VLDB describing the given volume instance. 
\par
The VLDB is read-locked for the duration of this operation. 
\par Error Codes 
VL BADVOLTYPE An illegal volume type has been specified by the voltype
argument. 
\n VL NOENT This volume instance does not appear in the VLDB. 
\n VL ENTDELETED The given VLDB entry has already been marked as deleted. 

	\subsection sec3-6-4 Section 3.6.4: VL GetEntryByName - Get VLDB entry
by volume name 

\code
int VL GetEntryByName(IN struct rx connection *z conn, 
			IN char *volumename, 
			OUT vldbentry *entry) 
\endcode
\par Description 
Given the volume name in the volumename parameter, return a pointer to the
entry in the VLDB describing the given volume. The name in volumename may be no
longer than MAXNAMELEN (65) characters, including the trailing null. Note that
it is legal to use the volume's numerical identifier (in string form) as the
volume name. 
\par
The VLDB is read-locked for the duration of this operation. 
\par
This function is closely related to the VL GetEntryByID() routine, as might be
expected. In fact, the by-ID routine is called if the volume name provided in
volumename is the string version of the volume's numerical identifier. 
\par Error Codes 
VL BADVOLTYPE An illegal volume type has been specified by the voltype
argument. 
\n VL NOENT This volume instance does not appear in the VLDB. 
\n VL ENTDELETED The given VLDB entry has already been marked as deleted. 
\n VL BADNAME The volume name is invalid. 

	\subsection sec3-6-5 Section 3.6.5: VL GetNewVolumeId - Generate a new
volume ID 

\code
int VL GetNewVolumeId(IN struct rx connection *z conn, 
			IN long bumpcount, 
			OUT long *newvolumid) 
\endcode
\par Description 
Acquire bumpcount unused, consecutively-numbered volume identifiers from the
Volume Location Server. The lowest-numbered of the newly-acquired set is placed
in the newvolumid argument. The largest number of volume IDs that may be
generated with any one call is bounded by the MAXBUMPCOUNT constant defined in
Section 3.2.1. Currently, there is (effectively) no restriction on the number
of volume identifiers that may thus be reserved in a single call. 
\par
The VLDB is write-locked for the duration of this operation. 
\par Error Codes 
VL PERM The caller is not authorized to execute this function. 
\n VL BADVOLIDBUMP The value of the bumpcount parameter exceeds the system
limit of MAXBUMPCOUNT. 
\n VL IO An error occurred while writing to the VLDB. 

	\subsection sec3-6-6 Section 3.6.6: VL ReplaceEntry - Replace entire
contents of VLDB entry 

\code
int VL ReplaceEntry(IN struct rx connection *z conn, 
			IN long Volid, 
			IN long voltype, 
			IN vldbentry *newentry, 
			IN long ReleaseType) 
\endcode
\par Description 
Perform a wholesale replacement of the VLDB entry corresponding to the volume
instance whose identifier is Volid and type voltype with the information
contained in the newentry argument. Individual VLDB entry fields cannot be
selectively changed while the others are preserved; VL UpdateEntry() should be
used for this objective. The permissible values for the ReleaseType parameter
are defined in Section 3.2.7. 
\par
The VLDB is write-locked for the duration of this operation. All of the hash
tables impacted are brought up to date to incorporate the new information. 
\par Error Codes 
VL PERM The caller is not authorized to execute this function. 
\n VL BADVOLTYPE An illegal volume type has been specified by the voltype
argument. 
\n VL BADRELLOCKTYPE An illegal release lock has been specified by the
ReleaseType argument. 
\n VL NOENT This volume instance does not appear in the VLDB. 
\n VL BADENTRY An attempt was made to change a read-write volume ID. 
\n VL IO An error occurred while writing to the VLDB. 

	\subsection sec3-6-7 Section 3.6.7: VL UpdateEntry - Update contents of
VLDB entry 

\code
int VL UpdateEntry(IN struct rx connection *z conn, 
			IN long Volid, 
			IN long voltype, 
			IN VldbUpdateEntry *UpdateEntry, 
			IN long ReleaseType) 
\endcode
\par Description 
Update the VLDB entry corresponding to the volume instance whose identifier is
Volid and type voltype with the information contained in the UpdateEntry
argument. Most of the entry's fields can be modified in a single call to VL
UpdateEntry(). The Mask field within the UpdateEntry parameter selects the
fields to update with the values stored within the other UpdateEntry fields.
Permissible values for the ReleaseType parameter are defined in Section 3.2.7. 
\par
The VLDB is write-locked for the duration of this operation. 
\par Error Codes 
VL PERM The caller is not authorized to execute this function. 
\n VL BADVOLTYPE An illegal volume type has been specified by the voltype
argument. 
\n VL BADRELLOCKTYPE An illegal release lock has been specified by the
ReleaseType argument. 
\n VL NOENT This volume instance does not appear in the VLDB. 
\n VL IO An error occurred while writing to the VLDB. 

	\subsection sec3-6-8 Section 3.6.8: VL SetLock - Lock VLDB entry 

\code
int VL SetLock(IN struct rx connection *z conn, 
		IN long Volid, 	
		IN long voltype, 	
		IN long voloper) 	
\endcode
\par Description 
Lock the VLDB entry matching the given volume ID (Volid) and type (voltype) for
volume operation voloper (e.g., VLOP MOVE and VLOP RELEASE). If the entry is
currently unlocked, then its LockTimestamp will be zero. If the lock is
obtained, the given voloper is stamped into the flags field, and the
LockTimestamp is set to the time of the call. 
\Note 
When the caller attempts to lock the entry for a release operation, special
care is taken to abort the operation if the entry has already been locked for
this operation, and the existing lock has timed out. In this case, VL SetLock()
returns VL RERELEASE. 
\par
The VLDB is write-locked for the duration of this operation. 
\par Error Codes 
VL PERM The caller is not authorized to execute this function. 
\n VL BADVOLTYPE An illegal volume type has been specified by the voltype
argument. 
\n VL BADVOLOPER An illegal volume operation was specified in the voloper
argument.  Legal values are defined in the latter part of the table in Section
3.2.5. 
\n VL ENTDELETED The given VLDB entry has already been marked as deleted. 
\n VL ENTRYLOCKED The given VLDB entry has already been locked (which has not
yet timed out). 
\n VL RERELEASE A VLDB entry locked for release has timed out, and the caller
also wanted to perform a release operation on it. 
\n VL IO An error was experienced while attempting to write to the VLDB. 

	\subsection sec3-6-9 Section 3.6.9: VL ReleaseLock - Unlock VLDB entry 

\code
int VL ReleaseLock(IN struct rx connection *z conn, 
			IN long Volid, 	
			IN long voltype, 	
			IN long ReleaseType) 	
\endcode
\par Description 
Unlock the VLDB entry matching the given volume ID (Volid) and type (voltype).
The ReleaseType argument determines which VLDB entry fields from flags and
LockAfsId will be cleared along with the lock timestamp in LockTimestamp.
Permissible values for the ReleaseType parameter are defined in Section 3.2.7. 
\par
The VLDB is write-locked for the duration of this operation. 
\par Error Codes 
VL PERM The caller is not authorized to execute this function. 
\n VL BADVOLTYPE An illegal volume type has been specified by the voltype
argument. 
\n VL BADRELLOCKTYPE An illegal release lock has been specified by the
ReleaseType argument. 
\n VL NOENT This volume instance does not appear in the VLDB. 
\n VL ENTDELETED The given VLDB entry has already been marked as deleted. 
\n VL IO An error was experienced while attempting to write to the VLDB. 

	\subsection sec3-6-10 Section 3.6.10: VL ListEntry - Get contents of
VLDB via index 

\code
int VL ListEntry(IN struct rx connection *z conn, 
			IN long previous index, 
			OUT long *count, 
			OUT long *next index, 
			OUT vldbentry *entry) 
\endcode
\par Description 
This function assists in the task of enumerating the contents of the VLDB.
Given an index into the database, previous index, this call return the single
VLDB entry at that offset, placing it in the entry argument. The number of VLDB
entries left to list is placed in count, and the index of the next entry to
request is returned in next index. If an illegal index is provided, count is
set to -1. 
\par
The VLDB is read-locked for the duration of this operation. 
\par Error Codes 
---None. 

	\subsection sec3-6-11 Section 3.6.11: VL ListAttributes - List all VLDB
entry matching given attributes, single return object 

\code
int VL ListAttributes(IN struct rx connection *z conn, 
			IN VldbListByAttributes *attributes, 
			OUT long *nentries, 
			OUT bulkentries *blkentries) 
\endcode
\par Description 
Retrieve all the VLDB entries that match the attributes listed in the
attributes parameter, placing them in the blkentries object. The number of
matching entries is placed in nentries. Matching can be done by server number,
partition, volume type, flag, or volume ID. The legal values to use in the
attributes argument are listed in Section 3.2.3. Note that if the VLLIST
VOLUMEID bit is set in attributes, all other bit values are ignored and the
volume ID provided is the sole search criterion. 
\par
The VLDB is read-locked for the duration of this operation. 
\par
Note that VL ListAttributes() is a potentially expensive function, as
sequential search through all of the VLDB entries is performed in most cases. 
\par Error Codes 
VL NOMEM Memory for the blkentries object could not be allocated. 
\n VL NOENT This specified volume instance does not appear in the VLDB. 
\n VL SIZEEXCEEDED Ran out of room in the blkentries object. 
\n VL IO Error while reading from the VLDB. 

	\subsection sec3-6-12 Section 3.6.12: VL LinkedList - List all VLDB
entry matching given attributes, linked list return object 

\code
int VL LinkedList(IN struct rx connection *z conn, 
			IN VldbListByAttributes *attributes, 
			OUT long *nentries, 
			OUT vldb list *linkedentries) 
\endcode
\par Description 
Retrieve all the VLDB entries that match the attributes listed in the
attributes parameter, creating a linked list of entries based in the
linkedentries object. The number of matching entries is placed in nentries.
Matching can be done by server number, partition, volume type, flag, or volume
ID. The legal values to use in the attributes argument are listed in Section
3.2.3. Note that if the VLLIST VOLUMEID bit is set in attributes, all other bit
values are ignored and the volume ID provided is the sole search criterion. 
\par
The VL LinkedList() function is identical to the VL ListAttributes(), except
for the method of delivering the VLDB entries to the caller. 
\par
The VLDB is read-locked for the duration of this operation. 
\par Error Codes 
VL NOMEM Memory for an entry in the list based at linkedentries object could
not be allocated. 
\n VL NOENT This specified volume instance does not appear in the VLDB. 
\n VL SIZEEXCEEDED Ran out of room in the current list object. 
\n VL IO Error while reading from the VLDB. 

	\subsection sec3-6-13 Section 3.6.13: VL GetStats - Get Volume Location
Server statistics 

\code
int VL GetStats(IN struct rx connection *z conn, 
		OUT vldstats *stats, 
		OUT vital vlheader *vital header) 
\endcode
\par Description
Collect the different types of VLDB statistics. Part of the VLDB header is
returned in vital header, which includes such information as the number of
allocations and frees performed, and the next volume ID to be allocated. The
dynamic per-operation stats are returned in the stats argument, reporting the
number and types of operations and aborts. 
\par
The VLDB is read-locked for the duration of this operation. 
\par Error Codes 
VL PERM The caller is not authorized to execute this function. 

	\subsection sec3-6-14 Section 3.6.14: VL Probe - Verify Volume Location
Server connectivity/status 

\code
int VL Probe(IN struct rx connection *z conn) 
\endcode
\par Description 
This routine serves a 'pinging' function to determine whether the Volume
Location Server is still running. If this call succeeds, then the Volume
Location Server is shown to be capable of responding to RPCs, thus confirming
connectivity and basic operation. 
\par
The VLDB is not locked for this operation. 
\par Error Codes 
---None. 

	\section sec3-7 Section 3.7: Kernel Interface Subset 

\par
The interface described by this document so far applies to user-level clients,
such as the vos utility. However, some volume location operations must be
performed from within the kernel. Specifically, the Cache Manager must find out
where volumes reside and otherwise gather information about them in order to
conduct its business with the File Servers holding them. In order to support
Volume Location Server interconnection for agents operating within the kernel,
the afsvlint.xg Rxgen interface was built. It is a minimal subset of the
user-level vldbint.xg definition. Within afsvlint.xg, there are duplicate
definitions for such constants as MAXNAMELEN, MAXNSERVERS, MAXTYPES, VLF
RWEXISTS, VLF ROEXISTS, VLF BACKEXISTS, VLSF NEWREPSITE, VLSF ROVOL, VLSF
RWVOL, and VLSF BACKVOL. Since the only operations the Cache Manager must
perform are volume location given a specific volume ID or name, and to find out
about unresponsive Volume Location Servers, the following interface routines
are duplicated in afsvlint.xg, along with the struct vldbentry declaration: 
\li VL GetEntryByID() 
\li VL GetEntryByName() 
\li VL Probe() 

	\page chap4 Chapter 4: Volume Server Architecture 

	\section sec4-1 Section 4.1: Introduction 

\par
The Volume Server allows administrative tasks and probes to be performed on the
set of AFS volumes residing on the machine on which it is running. As described
in Chapter 2, a distributed database holding volume location info, the VLDB, is
used by client applications to locate these volumes. Volume Server functions
are typically invoked either directly from authorized users via the vos utility
or by the AFS backup system. 
\par
This chapter briefly discusses various aspects of the Volume Server's
architecture. First, the high-level on-disk representation of volumes is
covered. Then, the transactions used in conjuction with volume operations are
examined. Then, the program implementing the Volume Server, volserver, is
considered. The nature and format of the log file kept by the Volume Server
rounds out the description. 
As with all AFS servers, the Volume Server uses the Rx remote procedure call
package for communication with its clients. 

	\section sec4-2 Section 4.2: Disk Representation 

\par
For each volume on an AFS partition, there exists a file visible in the unix
name space which describes the contents of that volume. By convention, each of
these files is named by concatenating a prefix string, "V", the numerical
volume ID, and the postfix string ".vol". Thus, file V0536870918.vol describes
the volume whose numerical ID is 0536870918. Internally, each per-volume
descriptor file has such fields as a version number, the numerical volume ID,
and the numerical parent ID (useful for read-only or backup volumes). It also
has a list of related inodes, namely files which are not visible from the unix
name space (i.e., they do not appear as entries in any unix directory object).
The set of important related inodes are: 
\li Volume info inode: This field identifies the inode which hosts the on-disk
representation of the volume's header. It is very similar to the information
pointed to by the volume field of the struct volser trans defined in Section
5.4.1, recording important status information for the volume. 
\li Large vnode index inode: This field identifies the inode which holds the
list of vnode identifiers for all directory objects residing within the volume.
These are "large" since they must also hold the Access Control List (ACL)
information for the given AFS directory. 
\li Small vnode index inode: This field identifies the inode which holds the
list of vnode identifiers for all non-directory objects hosted by the volume. 
\par
All of the actual files and directories residing within an AFS volume, as
identified by the contents of the large and small vnode index inodes, are also
free-floating inodes, not appearing in the conventional unix name space. This
is the reason the vendor-supplied fsck program should not be run on partitions
containing AFS volumes. Since the inodes making up AFS files and directories,
as well as the inodes serving as volume indices for them, are not mapped to any
directory, the standard fsck program would throw away all of these
"unreferenced" inodes. Thus, a special version of fsck is provided that
recognizes partitions containing AFS volumes as well as standard unix
partitions. 

	\section sec4-3 Section 4.3: Transactions 

\par
Each individual volume operation is carried out by the Volume Server as a
transaction, but not in the atomic sense of the word. Logically, creating a
Volume Server transaction can be equated with performing an "exclusive open" on
the given volume before beginning the actual work of the desired volume
operation. No other Volume Server (or File Server) operation is allowed on the
opened volume until the transaction is terminated. Thus, transactions in the
context of the Volume Server serve to provide mutual exclusion without any of
the normal atomicity guarantees. Volumes maintain enough internal state to
enable recovery from interrupted or failed operations via use of the salvager
program. Whenever volume inconsistencies are detected, this salvager program is
run, which then attempts to correct the problem. 
\par
Volume transactions have timeouts associated with them. This guarantees that
the death of the agent performing a given volume operation cannot result in the
volume being permanently removed from circulation. There are actually two
timeout periods defined for a volume transaction. The first is the warning
time, defined to be 5 minutes. If a transaction lasts for more than this time
period without making progress, the Volume Server prints a warning message to
its log file (see Section 4.5). The second time value associated with a volume
transaction is the hard timeout, defined to occur 10 minutes after any progress
has been made on the given operation. After this period, the transaction will
be unconditionally deleted, and the volume freed for any other operations.
Transactions are reference-counted. Progress will be deemed to have occurred
for a transaction, and its internal timeclock field will be updated, when: 
\li 1 The transaction is first created. 
\li 2 A reference is made to the transaction, causing the Volume Server to look
it up in its internal tables. 
\li 3 The transaction's reference count is decremented. 

	\section sec4-4 Section 4.4: The volserver Process 

\par
The volserver user-level program is run on every AFS server machine, and
implements the Volume Server agent. It is responsible for providing the Volume
Server interface as defined by the volint.xg Rxgen file. 
\par
The volserver process defines and launches five threads to perform the bulk of
its duties. One thread implements a background daemon whose job it is to
garbage-collect timed-out transaction structures. The other four threads are
RPC interface listeners, primed to accept remote procedure calls and thus
perform the defined set of volume operations. 
\par
Certain non-standard configuration settings are made for the RPC subsystem by
the volserver program. For example, it chooses to extend the length of time
that an Rx connection may remain idle from the default 12 seconds to 120
seconds. The reasoning here is that certain volume operations may take longer
than 12 seconds of processing time on the server, and thus the default setting
for the connection timeout value would incorrectly terminate an RPC when in
fact it was proceeding normally and correctly. 
\par
The volserver program takes a single, optional command line argument. If a
positive integer value is provided on the command line, then it shall be used
to set the debugging level within the Volume Server. By default, a value of
zero is used, specifying that no special debugging output will be generated and
fed to the Volume Server log file described below. 

	\section sec4-5 Section 4.5: Log File 

\par
The Volume Server keeps a log file, recording the set of events of special
interest it has encountered. The file is named VolserLog, and is stored in the
/usr/afs/logs directory on the local disk of the server machine on which the
Volume Server runs. This is a human-readable file, with every entry
time-stamped. 
\par
Whenever the volserver program restarts, it renames the current VolserLog file
to VolserLog.old, and starts up a fresh log. A properly-authorized individual
can easily inspect the log file residing on any given server machine. This is
made possible by the BOS Server AFS agent running on the machine, which allows
the contents of this file to be fetched and displayed on the caller's machine
via the bos getlog command. 
\par
An excerpt from a Volume Server log file follows below. The numbers appearing
in square brackets at the beginning of each line have been inserted so that we
may reference the individual lines of the log excerpt in the following
paragraph. 
\code
[1] Wed May 8 06:03:00 1991 AttachVolume: Error attaching volume
/vicepd/V1969547815.vol; volume needs salvage 
[2] Wed May 8 06:03:01 1991 Volser: ListVolumes: Could not attach volume
1969547815 
[3] Wed May 8 07:36:13 1991 Volser: Clone: Cloning volume 1969541499 to new
volume 1969541501 
[4] Wed May 8 11:25:05 1991 AttachVolume: Cannot read volume header
/vicepd/V1969547415.vol 
[5] Wed May 8 11:25:06 1991 Volser: CreateVolume: volume 1969547415
(bld.dce.s3.dv.pmax_ul3) created 
\endcode
\par
Line [1] indicates that the volume whose numerical ID is 1969547815 could not
be attached on partition /vicepd. This error is probably the result of an
aborted transaction which left the volume in an inconsistent state, or by
actual damage to the volume structure or data. In this case, the Volume Server
recommends that the salvager program be run on this volume to restore its
integrity. Line [2] records the operation which revealed this situation, namely
the invocation of an AFSVolListVolumes() RPC. 
\par
Line [4] reveals that the volume header file for a specific volume could not be
read. Line [5], as with line [2] in the above paragraph, indicates why this is
true. Someone had called the AFSVolCreateVolume() interface function, and as a
precaution, the Volume Server first checked to see if such a volume was already
present by attempting to read its header. 
\par
Thus verifying that the volume did not previously exist, the Volume Server
allowed the AFSVolCreateVolume() call to continue its processing, creating and
initializing the proper volume file, V1969547415.vol, and the associated header
and index inodes. 

	\page chap5 Chapter 5: Volume Server Interface 

	\section sec5-1 Section 5.1 Introduction 

\par
This chapter documents the API for the Volume Server facility, as defined by
the volint.xg Rxgen interface file and the volser.h include file. Descriptions
of all the constants, structures, macros, and interface functions available to
the application programmer appear here. 

	\section sec5-2 Section 5.2: Constants 

\par
This section covers the basic constant definitions of interest to the Volume
Server application programmer. These definitions appear in the volint.h file,
automatically generated from the volint.xg Rxgen interface file, and in
volser.h. 
\par
Each subsection is devoted to describing the constants falling into the
following categories: 
\li Configuration and boundary values 
\li Interface routine opcodes 
\li Transaction Flags 
\li Volume Types 
\li LWP State 
\li States for struct vldbentry 
\li Validity Checks 
\li Miscellaneous 

	\subsection sec5-2-1 Section 5.2.1: Configuration and Boundary Values 

\par
These constants define some basic system configuration values, along with such
things as maximum sizes of important arrays. 

MyPort 	5,003 	The Rx UDP port on which the Volume Server service may be
found. 
\par Name
NameLen 	
\par Value
80 	
\par Description
Used by the vos utility to define maximum lengths for internal filename
variables. 

\par Name
VLDB MAXSERVERS 	
\par Value
10 	
\par Description
Maximum number of server agents implementing the AFS Volume Location Database
(VLDB) for the cell. 

\par Name
VOLSERVICE ID 	
\par Value
4 	
\par Description
The Rx service number on the given UDP port (MyPort) above. 

\par Name
INVALID BID 	
\par Value
0 	
\par Description
Used as an invalid read-only or backup volume ID. 

\par Name
VOLSER MAXVOLNAME 	
\par Value
65 	
\par Description
The number of characters in the longest possible volume name, including the
trailing null. Note: this is only used by the vos utility; the Volume Server
uses the "old" value below. 

\par Name
VOLSER OLDMAXVOLNAME 	
\par Value
32 	
\par Description
The "old" maximum number of characters in an AFS volume name, including the
trailing null. In reality, it is also the current maximum. 

\par Name
VOLSER MAX REPSITES 	
\par Value
7 	
\par Description
The maximum number of replication sites for a volume. 

\par Name
VNAMESIZE 	
\par Value
32 	
\par Description
Size in bytes of the name field in struct volintInfo (see Section 5.4.6). 


	\subsection sec5-2-2 Section 5.2.2: Interface Routine Opcodes 

\par
These constants, appearing in the volint.xg Rxgen interface file for the Volume
Server, define the opcodes for the RPC routines. Every Rx call on this
interface contains this opcode, and the dispatcher uses it to select the proper
code at the server site to carry out the call. 

\par Name 	
VOLCREATEVOLUME 	
\par Value 	
100 	
\par Description 
Opcode for AFSVolCreateVolume() 

\par Name 	
VOLDELETEVOLUME 	
\par Value 	
101 	
\par Description 
Opcode for AFSVolDeleteVolume() 

\par Name 	
VOLRESTORE 	
\par Value 	
102 	
\par Description 
Opcode for AFSVolRestoreVolume() 

\par Name 	
VOLFORWARD 	
\par Value 	
103 	
\par Description 
Opcode for AFSVolForward() 

\par Name 	
VOLENDTRANS 	
\par Value 	
104 	
\par Description 
Opcode for AFSVolEndTrans() 

\par Name 	
VOLCLONE 	
\par Value 	
105 	
\par Description 
Opcode for AFSVolClone() . 

\par Name 	
VOLSETFLAGS 	
\par Value 	
106 	
\par Description 
Opcode for AFSVolSetFlags() 

\par Name 	
VOLGETFLAGS 	
\par Value 	
107 	
\par Description 
Opcode for AFSVolGetFlags() 

\par Name 	
VOLTRANSCREATE 	
\par Value 	
108 	
\par Description 
Opcode for AFSVolTransCreate() 

\par Name 	
VOLDUMP 	
\par Value 	
109 	
\par Description 
Opcode for AFSVolDump() 

\par Name 	
VOLGETNTHVOLUME 	
\par Value 	
110 	
\par Description 
Opcode for AFSVolGetNthVolume() 

\par Name 	
VOLSETFORWARDING 	
\par Value 	
111 	
\par Description 
Opcode for AFSVolSetForwarding() 

\par Name 	
VOLGETNAME 	
\par Value 	
112 	
\par Description 
Opcode for AFSVolGetName() 

\par Name 	
VOLGETSTATUS 	
\par Value 	
113 	
\par Description 
Opcode for AFSVolGetStatus() 

\par Name 	
VOLSIGRESTORE 	
\par Value 	
114 	
\par Description 
Opcode for AFSVolSignalRestore() 

\par Name 	
VOLLISTPARTITIONS 	
\par Value 	
115 	
\par Description 
Opcode for AFSVolListPartitions() 

\par Name 	
VOLLISTVOLS 	
\par Value 	
116 	
\par Description 
Opcode for AFSVolListVolumes() 

\par Name 	
VOLSETIDSTYPES 	
\par Value 	
117 	
\par Description 
Opcode for AFSVolSetIdsTypes() 

\par Name 	
VOLMONITOR 	
\par Value 	
118 	
\par Description 
Opcode for AFSVolMonitor() 

\par Name 	
VOLDISKPART 	
\par Value 	
119 	
\par Description 
Opcode for AFSVolPartitionInfo() 

\par Name 	
VOLRECLONE 	
\par Value 	
120 	
\par Description 
Opcode for AFSVolReClone() 

\par Name 	
VOLLISTONEVOL 	
\par Value 	
121 	
\par Description 
Opcode for AFSVolListOneVolume() 

\par Name 	
VOLNUKE 	
\par Value 	
122 	
\par Description 
Opcode for AFSVolNukeVolume() 

\par Name 	
VOLSETDATE 	
\par Value 	
123 	
\par Description 
Opcode for AFSVolSetDate() 

	\subsection sec5-2-3 Section 5.2.3: Transaction Flags 

\par
These constants define the various flags the Volume Server uses in assocation
with volume transactions, keeping track of volumes upon which operations are
currently proceeding. There are three sets of flag values, stored in three
different fields within a struct volser trans: general volume state, attachment
modes, and specific transaction states. 

	\subsubsection sec5-2-3-1: Section 5.2.3.1 vflags 

\par
These values are used to represent the general state of the associated volume.
They appear in the vflags field within a struct volser trans. 

\par Name 	
VTDeleteOnSalvage 	
\par Value 	
1 	
\par Description 
The volume should be deleted on next salvage. 

\par Name 	
VTOutOfService 	
\par Value 	
2 	
\par Description 
This volume should never be put online. 

\par Name 	
VTDeleted 	
\par Value 	
4 	
\par Description 
This volume has been deleted (via AFSVolDeleteVol¬ume() ), and thus should not
be manipulated. 

	\subsubsection sec5-2-3-2 Section 5.2.3.2: iflags 

\par
These constants represent the desired attachment mode for a volume at the start
of a transaction. Once attached, the volume header is marked to reflect this
mode. Attachment modes are useful in salvaging partitions, as they indicate
whether the operations being performed on individual volumes at the time the
crash occured could have introduced inconsistencies in their metadata
descriptors. If a volume was attached in a read-only fashion, then the salvager
may decide (taking other factors into consideration) that the volume doesn't
need attention as a result of the crash. 
\par
These values appear in the iflags field within a struct volser trans. 

\par Name 	
ITOffline 	
\par Value 	
0x1 	
\par Description 
Volume offline on server (returns VOFFLINE). 

\par Name 	
ITBusy 	
\par Value 	
0x2 	
\par Description 
Volume busy on server (returns VBUSY). 

\par Name 	
ITReadOnly 	
\par Value 	
0x8 	
\par Description 
Volume is read-only on client, read-write on server -DO NOT USE. 

\par Name 	
ITCreate 	
\par Value 	
0x10 	
\par Description 
Volume does not exist correctly yet. 

\par Name 	
ITCreateVolID 	
\par Value 	
0x1000 	
\par Description 
Create volid. 

	\subsubsection sec5-2-3-3 Section 5.2.3.3: tflags 

\par
This value is used to represent the transaction state of the associated volume,
and appears in the tflags field within a struct volser trans. 

\par Name 	
TTDeleted 	
\par Value 	
1 	
\par Description 
Delete transaction not yet freed due to high reference count. 

	\subsection sec5-2-4 Section 5.2.4: Volume Types 

\par
The following constants may be supplied as values for the type argument to the
AFSVol-CreateVolume() interface call. They are just synonyms for the three
values RWVOL, ROVOL, 

\par Name 	
volser RW 	
\par Value 	
0 	
\par Description 
Specifies a read-write volume type. 

\par Name 	
volser RO 	
\par Value 	
1 	
\par Description 
Specifies a read-only volume type. 

\par Name 	
volser BACK 	
\par Value 	
2 	
\par Description 
Specifies a backup volume type. 

	\subsection sec5-2-5 Section 5.2.5: LWP State 

\par
This set of exported definitions refers to objects internal to the Volume
Server, and strictly speaking should not be visible to other agents.
Specifically, a busyFlags array keeps a set of flags referenced by the set of
lightweight threads running within the Volume Server. These flags reflect and
drive the state of each of these worker LWPs. 

\par Name 	
VHIdle 	
\par Value 	
1 	
\par Description 
Volume Server LWP is idle, waiting for new work. 

\par Name 	
VHRequest 	
\par Value 	
2 	
\par Description 
A work item has been queued. 

	\subsection sec5-2-6 Section 5.2.6: States for struct vldbentry 

\par
The Volume Server defines a collection of synonyms for certain values defined
by the Volume Location Server. These particular constants are used within the
flags field in objects of type struct vldbentry. The equivalent Volume Location
Server values are described in Section 3.2.6. 

\par Name 	
RW EXISTS 	
\par Value 	
0x1000 	
\par Description 
Synonym for VLF RWEXISTS. 

\par Name 	
RO EXISTS 	
\par Value 	
0x2000 	
\par Description 
Synonym for VLF ROEXISTS. 

\par Name 	
BACK EXISTS 	
\par Value 	
0x4000 	
\par Description 
Synonym for VLF BACKEXISTS. 

\par Name 	
NEW REPSITE 	
\par Value 	
0x01 	
\par Description 
Synonym for VLSF NEWREPSITE. 

\par Name 	
ITSROVOL 	
\par Value 	
0x02 	
\par Description 
Synonym for VLFS ROVOL. 

\par Name 	
ITSRWVOL 	
\par Value 	
0x04 	
\par Description 
Synonym for VLSF RWVOL. 

\par Name 	
ITSBACKVOL 	
\par Value 	
0x08 	
\par Description 
Synonym for VLSF BACKVOL. 

	\subsection sec5-2-7 Section 5.2.7: Validity Checks 

\par
These values are used for performing validity checks. The first one appears
only within the partFlags field within objects of type partList (see Section
5.4.3). The rest (except VOK and VBUSY) appear in the volFlags field within an
object of type struct volDescription. These latter defintions are used within
the volFlags field to mark whether the rest of the fields within the struct
volDescription are valid. Note that while several constants are defined, only
some are actually used internally by the Volume Server code. 

\par Name 	
PARTVALID 	
\par Value 	
0x01 	
\par Description 
The indicated partition is valid. 

\par Name 	
CLONEVALID 	
\par Value 	
0x02 	
\par Description 
The indicated clone (field volCloneId) is a valid one. 

\par Name 	
CLONEZAPPED 	
\par Value 	
0x04 	
\par Description 
The indicated clone volume (field volCloneId) has been deleted. 

\par Name 	
IDVALID 	
\par Value 	
0x08 	
\par Description 
The indicated volume ID (field volId) is valid. 

\par Name 	
NAMEVALID 	
\par Value 	
0x10 	
\par Description 
The indicted volume name (field volName) is valid. Not used internally by the
Volume Server. 

\par Name 	
SIZEVALID 	
\par Value 	
0x20 	
\par Description 
The indicated volume size (field volSize) is valid. Not used internally by the
Volume Server. 

\par Name 	
ENTRYVALID 	
\par Value 	
0x40 	
\par Description 
The struct volDescription refers to a valid volume. 

\par Name 	
REUSECLONEID 	
\par Value 	
0x80 	
\par Description 
The indicated clone ID (field volCloneId) should be reused. 

\par Name 	
VOK 	
\par Value 	
0x02 	
\par Description 
Used in the status field of struct volintInfo to show that everything is OK. 

\par Name 	
VBUSY 	
\par Value 	
110 	
\par Description 
Used in the status field of struct volintInfo to show that the volume is
currently busy. 

	\subsection sec5-2-8 Section 5.2.8: Miscellaneous 

\par
This section covers the set of exported Volume Server definitions that don't
easily fall into the above categories. 

\par Name 	
SIZE 	
\par Value 	
1,024 	
\par Description 
Not used internally by the Volume Server; used as a maxi¬mum size for internal
character arrays. 

\par Name 	
MAXHELPERS 	
\par Value 	
10 	
\par Description 
Size of an internal Volume Server character array (busyFlags), it marks the
maximum number of threads within the server. 

\par Name 	
STDERR 	
\par Value 	
stderr 	
\par Description 
Synonym for the unix standard input file descriptor. 

\par Name 	
STDOUT 	
\par Value 	
stdout 	
\par Description 
Synonym for the unix standard output file descriptor. 

	\section sec5-3 Section 5.3: Exported Variables 

\par
This section describes the single variable that the Volume Server exports to
its applications. 
\par
The QI GlobalWriteTrans exported variable represents a pointer to the head of
the global queue of transaction structures for operations being handled by a
Volume Server. Each object in this list is of type struct volser trans (see
Section 5.4.1 below). 

	\section sec5-4 Section 5.4: Structures and Typedefs 

\par
This section describes the major exported Volume Server data structures of
interest to application programmers, along with some of the the typedefs based
on those structures. Please note that typedefs in shose definitions angle
brackets appear are those fed through the Rxgen RPC stub generator. Rxgen uses
these angle brackets to specify an array of indefinite size. 

	\subsection sec5-4-1 Section 5.4.1: struct volser trans 

\par
This structure defines the transaction record for all volumes upon which an
active operation is proceeding. 
\n \b Fields 
\li struct volser trans *next - Pointer to the next transaction structure in
the queue. 
\li long tid - Transaction ID. 
\li long time - The time this transaction was last active, for timeout
purposes. 
\li This is the standard unix time format. 
\li long creationTime - The time a which this transaction started. 
\li long returnCode - The overall transaction error code. 
\li struct Volume *volume - Pointer to the low-level object describing the
associated volume. This is included here for the use of lower-level support
code. 
\li long volid - The associated volume's numerical ID. 
\li long partition - The partition on which the given volume resides. 
\li long dumpTransId - Not used. 
\li long dumpSeq - Not used. 
\li short refCount - Reference count on this structure. 
\li short iflags - Initial attach mode flags. 
\li char vflags - Current volume status flags. 
\li char tflags - Transaction flags. 
\li char incremental - If non-zero, indicates that an incremental restore
operation should be performed. 
\li char lastProcName[] - Name of the last internal Volume Server procedure
that used this transaction. This field may be up to 30 characters long,
including the trailing null, and is intended for debugging purposes only. 
\li struct rx call *rxCallPtr - Pointer to latest associated rx call. This
field is intended for debugging purposes only. 

	\subsection sec5-4-2 Section 5.4.2: struct volDescription 

\par
This structure is used by the AFS backup system to group certain key fields of
volume information. 
\n \b Fields 
\li char volName[] -The name of the given volume; maximum length of this string
is VOLSER MAXVOLNAME characters, including the trailing null. 
\li long volId -The volume's numerical ID. 
\li int volSize -The size of the volume, in bytes. 
\li long volFlags -Keeps validity information on the given volume and its
clones. This field takes on values from the set defined in Section 5.2.7 
\li long volCloneId -The volume's current clone ID. 

	\subsection sec5-4-3 Section 5.4.3: struct partList 

\par
This structure is used by the backup system and the vos tool to keep track of
the state of the AFS disk partitions on a given server. 
\n \b Fields 
\li long partId[] -Set of 26 partition IDs. 
\li long partFlags[] -Set to PARTVALID if the associated partition slot
corresponds to a valid partition. There are 26 entries in this array. 

	\subsection sec5-4-4 Section 5.4.4: struct volser status 

\par
This structure holds the status of a volume as it is known to the Volume
Server, and is passed to clients through the AFSVolGetStatus() interface call. 
\par
Two fields appearing in this structure, accessDate and updateDate, deserve a
special note. In particular, it is important to observe that these fields are
not kept in full synchrony with reality. When a File Server provides one of its
client Cache Managers with a chunk of a file on which to operate, it is
incapable of determining exactly when the data in that chunk is accessed, or
exactly when it is updated. This is because the manipulations occur on the
client machine, without any information on these accesses or updates passed
back to the server. The only time these fields can be modified is when the
chunk of a file resident within the given volume is delivered to a client (in
the case of accessDate), or when a client writes back a dirty chunk to the File
Server (in the case of updateDate). 
\n \b Fields 
\li long volID - The volume's numerical ID, unique within the cell. 
\li long nextUnique - Next value to use for a vnode uniquifier within this
volume. 
\li int type - Basic volume class, one of RWVOL, ROVOL, or BACKVOL. 
\li long parentID - Volume ID of the parent, if this volume is of type ROVOL or
BACKVOL. 
\li long cloneID - ID of the latest read-only clone, valid iff the type field
is set to RWVOL. 
\li long backupID - Volume ID of the latest backup of this read-write volume. 
\li long restoredFromID - The volume ID contained in the dump from which this
volume was restored. This field is used to simply make sure that an incremental
dump is not restored on top of something inappropriate. Note that this field
itself is not dumped. 
\li long maxQuota - The volume's maximum quota, in 1Kbyte blocks. 
\li long minQuota - The volume's minimum quota, in 1Kbyte blocks. 
\li long owner - The user ID of the person responsible for this volume. 
\li long creationDate - For a volume of type RWVOL, this field marks its
creation date.  For the original copy of a clone, this field represents the
cloning date. 
\li long accessDate - Last access time by a user for this volume. This value is
expressed as a standard unix longword date quantity. 
\li long updateDate - Last modification time by a user for this volume. This
value is expressed as a standard unix longword date quantity. 
\li long expirationDate - Expiration date for this volume. If the volume never
expires, then this field is set to zero. 
\li long backupDate - The last time a backup clone was created for this volume. 
\li long copyDate - The time that this copy of this volume was created. 

	\subsection sec5-4-5 Section 5.4.5: struct destServer 

\par
Used to specify the destination server in an AFSVolForward() invocation (see
Section 5.7.7). 
\n \b Fields 
\li long destHost - The IP address of the destination server. 
\li long destPort - The UDP port for the Volume Server Rx service there. 
\li long destSSID - Currently, this field is always set to 1. 

	\subsection sec5-4-6 Section 5.4.6: struct volintInfo 

\par
This structure is used to communicate volume information to the Volume Server's
RPC clients. It is used to build the volEntries object, which appears as a
parameter to the AFSVolListVolumes() call. 
\par
The comments in Section 5.4.4 concerning the accessDate and updateDate fields
are equally valid for the analogue fields in this structure. 
\n \b Fields 
\li char name[] - The null-terminated name for the volume, which can be no
longer than VNAMESIZE (32) characters, including the trailing null. 
\li long volid - The volume's numerical ID. 
\li long type - The volume's basic class, one of RWVOL, ROVOL, or BACKVOL. 
\li long backupID - The latest backup volume's ID. 
\li long parentID - The parent volume's ID. 
\li long cloneID - The latest clone volume's ID. 
\li long status - Status of the volume; may be one of VOK or VBUSY. 
\li long copyDate - The time that this copy of this volume was created. 
\li unsigned char inUse - If non-zero, an indication that this volume is
online. 
\li unsigned char needsSalvaged - If non-zero, an indication that this volume
needs to be salvaged. 
\li unsigned char destroyMe - If non-zero, an indication that this volume
should be destroyed. 
\li long creationDate - Creation date for a read/write volume; cloning date for
the original copy of a read-only volume. 
\li long accessDate - Last access time by a user for this volume. 
\li long updateDate - Last modification time by a user for this volume. 
\li long backupDate - Last time a backup copy was made of this volume. 
\li int dayUse - Number of times this volume was accessed since midnight of the
current day. 
\li int filecount - the number of file system objects contained within the
volume. 
\li int maxquota - The upper limit on the number of 1-Kbyte disk blocks of
storage that this volume may obtain. 
\li int size - Not known. 
\li long flags - Values used by the backup system are stored here. 
\li long spare1 -spare3 -Spare fields, reserved for future use. 

	\subsection sec5-4-7 Section 5.4.7: struct transDebugInfo 

\par
This structure is provided for monitoring and debugging purposes. It is used to
compose the transDebugEntries variable-sized object, which in turn appears as a
parameter to the AFSVolMonitor() interface call. 
\n \b Fields 
\li long tid - The transaction ID. 
\li long time - The time when the transaction was last active, for timeout
purposes. 
\li long creationTime - The time the transaction started. 
\li long returnCode - The overall transaction error code. 
\li long volid - The open volume's ID. 
\li long partition - The open volume's partition. 
\li short iflags - Initial attach mode flags (IT*). 
\li char vflags - Current volume status flags (VT*). 
\li char tflags - Transaction flags (TT*). 
\li char lastProcName[] - The string name of the last procedure which used
transaction. This field may be up to 30 characters long, including the trailing
null, and is intended for debugging purposes only. 
\li int callValid - Flag which determines if the following fields are valid. 
\li long readNext - Sequence number of the next Rx packet to be read. 
\li long transmitNext - Sequence number of the next Rx packet to be
transmitted. 
\li int lastSendTime - The last time anything was sent over the wire for this
transaction. 
\li int lastReceiveTime - The last time anything was received over the wire for
this transaction. 

	\subsection sec5-4-8 Section 5.4.8: struct pIDs 

\par
Used by the AFSVolListPartitions() interface call, this structure is used to
store information on all of the partitions on a given Volume Server. 
\n \b Fields 
\li long partIds[] - One per letter of the alphabet (/vicepa through /vicepz).
Filled with 0 for "/vicepa", 25 for "/vicepz". Invalid partition slots are
filled in with a -1. 

	\subsection sec5-4-9 Section 5.4.9: struct diskPartition 

\par
This structure contains information regarding an individual AFS disk partition.
It is returned as a parameter to the AFSVolPartitionInfo() call. 
\n \b Fields 
\li char name[] -Mounted partition name, up to 32 characters long including the
trailing null. 
\li char devName[] -Device name on which the partition lives, up to 32
characters long including the trailing null. 
\li int lock fd -A lock used for mutual exclusion to the named partition. A
value of -1 indicates the lock is not currently being held. Otherwise, it has
the file descriptor resulting from the unix open() call on the file specified
in the name field used to "acquire" the lock. 
\li int totalUsable - The number of blocks within the partition which are
available. 
\li int free - The number of free blocks in the partition. 
\li int minFree - The minimum number of blocks that must remain free regardless
of allocation requests. 

	\section sec5-4-10 Section 5.4.10: struct restoreCookie 

\par
Used as a parameter to both AFSVolRestore() and AFSVolForward(),a restoreCookie
keeps information that must be preserved between various Volume Server
operations. 
\n \b Fields 
\li char name[] - The volume name, up to 32 characters long including the
trailing null. 
\li long type - The volume type, one of RWVOL, ROVOL, and BACKVOL. 
\li long clone - The current read-only clone ID for this volume. 
\li long parent - The parent ID for this volume. 

	\section sec5-4-11 Section 5.4.11: transDebugEntries 

\code
typedef transDebugInfo transDebugEntries<>; 
\endcode
\par
This typedef is used to generate a variable-length object which is passed as a
parameter to the AFSVolMonitor() interface function. Thus, it may carry any
number of descriptors for active transactions on the given Volume Server.
Specifi, it causes a C structure of the same name to be defined with the
following fields: 
\n \b Fields 
\li u int transDebugEntries len - The number of struct transDebugInfo (see
Section 5.4.7) objects appearing at the memory location pointed to by the
transDebugEntries val field. 
\li transDebugInfo *transDebugEntries val - A pointer to a region of memory
containing an array of transDebugEntries len objects of type struct
transDebugInfo. 

	\subsection sec5-4-12 Section 5.4.12: volEntries 

\code
typedef volintInfo volEntries<>; 
\endcode
\par
This typedef is used to generate a variable-length object which is passed as a
parameter to AFSVolListVolumes(). Thus, it may carry any number of descriptors
for volumes on the given Volume Server. Specifically, it causes a C structure
of the same name to be defined with the following fields: 
\n \b Fields 
\li u int volEntries len - The number of struct volintInfo (see Section 5.4.6)
objects appearing at the memory location pointed to by the volEntries val
field. 
\li volintInfo *volEntries val -A pointer to a region of memory containing an
array of volEntries len objects of type struct volintInfo. 

	\section sec5-5 Section 5.5: Error Codes 

\par
The Volume Server advertises two groups of error codes. The first set consists
of the standard error codes defined by the package. The second is a collection
of lower-level return values which are exported here for convenience. 

\par Name 	
VOLSERTRELE ERROR 	
\par Value 	
1492325120L 	
\par Description 
internal error releasing transaction. 

\par Name 	
VOLSERNO OP 	
\par Value 	
1492325121L 	
\par Description 
unknown internal error. 

\par Name 	
VOLSERREAD DUMPERROR 	
\par Value 	
1492325122L 	
\par Description 
badly formatted dump. 

\par Name 	
VOLSERDUMPERROR 	
\par Value 	
1492325123L 	
\par Description 
badly formatted dump(2). 

\par Name 	
VOLSERATTACH ERROR 	
\par Value 	
1492325124L 	
\par Description 
could not attach volume. 

\par Name 	
VOLSERILLEGAL PARTITION 	
\par Value 	
1492325125L 	
\par Description 
illegal partition. 

\par Name 	
VOLSERDETACH ERROR 	
\par Value 	
1492325126L 	
\par Description 
could not detach volume. 

\par Name 	
VOLSERBAD ACCESS 	
\par Value 	
1492325127L 	
\par Description 
insufficient privilege for volume operation. 

\par Name 	
VOLSERVLDB ERROR 	
\par Value 	
1492325128L 	
\par Description 
error from volume location database. 

\par Name 	
VOLSERBADNAME 	
\par Value 	
1492325129L 	
\par Description 
bad volume name. 

\par Name 	
VOLSERVOLMOVED 	
\par Value 	
1492325130L 	
\par Description 
volume moved. 

\par Name 	
VOLSERBADOP 	
\par Value 	
1492325131L 	
\par Description 
illegal volume operation. 

\par Name 	
VOLSERBADRELEASE 	
\par Value 	
1492325132L 	
\par Description 
volume release failed. 

\par Name 	
VOLSERVOLBUSY 	
\par Value 	
1492325133L 	
\par Description 
volume still in use by volserver. 

\par Name 	
VOLSERNO MEMORY 	
\par Value 	
1492325134L 	
\par Description 
out of virtual memory in volserver. 

\par Name 	
VOLSERNOVOL 	
\par Value 	
1492325135L 	
\par Description 
no such volume. 

\par Name 	
VOLSERMULTIRWVOL 	
\par Value 	
1492325136L 	
\par Description 
more than one read/write volume. 

\par Name 	
VOLSERFAILEDOP 	
\par Value 	
1492325137L 	
\par Description 
failed volume server operation. 

	\subsection sec5-5-1 Section 5.5.1: Standard 

\par
The error codes described in this section were defined by the Volume Server to
describe exceptional conditions arising in the course of RPC call handling. 

	\subsection sec5-5-2 Section 5.5.2: Low-Level 

\par
These error codes are duplicates of those defined from a package which is
internal to the Volume Server. They are re-defined here to make them visible to
Volume Server clients. 

\par Name 	
VSALVAGE 	
\par Value 	
101 	
\par Description 
Volume needs to be salvaged. 

\par Name 	
VNOVNODE 	
\par Value 	
102 	
\par Description 
Bad vnode number encountered. 

\par Name 	
VNOVOL 	
\par Value 	
103 	
\par Description 
The given volume is either not attached, doesn't exist, or is not online. 

\par Name 	
VVOLEXISTS 	
\par Value 	
104 	
\par Description 
The given volume already exists. 

\par Name 	
VNOSERVICE 	
\par Value 	
105 	
\par Description 
The volume is currently not in service. 

\par Name 	
VOFFLINE 	
\par Value 	
106 	
\par Description 
The specified volume is offline, for the reason given in the offline message
field (a subfield within the volume field in struct volser trans). 

\par Name 	
VONLINE 	
\par Value 	
107 	
\par Description 
Volume is already online. 

\par Name 	
VDISKFULL 	
\par Value 	
108 	
\par Description 
The disk partition is full. 

\par Name 	
VOVERQUOTA 	
\par Value 	
109 	
\par Description 
The given volume's maximum quota, as expressed in the maxQuota field of the
struct volintInfo, has been exceeded. 

\par Name 	
VBUSY 	
\par Value 	
110 	
\par Description 
The named volume is temporarily unavailable, and the client is encouraged to
retry the operation shortly. 

\par Name 	
VMOVED 	
\par Value 	
111 	
\par Description 
The given volume has moved to a new server. 

\par
The VICE SPECIAL ERRORS constant is defined to be the lowest of these error
codes. 

	\section sec5-6 Section 5.6: Macros 

\par
The Volume Server defines a small number of macros, as described in this
section. 
	\subsection sec5-6-1 Section 5.6.1: THOLD() 

\code
#define THOLD(tt) ((tt)->refCount++) 
\endcode
\par
This macro is used to increment the reference count field, refCount, in an
object of type struct volser trans. Thus, the associated transaction is
effectively "held" insuring it won't be garbage-collected. The counterpart to
this operation, TRELE(), is implemented by the Volume Server as a function. 

	\subsection sec5-6-2 Section 5.6.2: ISNAMEVALID() 

\code
#define ISNAMEVALID(name) (strlen(name) < (VOLSER_OLDMAXVOLNAME -9)) 
\endcode
\par
This macro checks to see if the given name argument is of legal length. It must
be no more than the size of the container, which is at most VOLSER
OLDMAXVOLNAME characters, minus the length of the longest standardized volume
name postfix known to the system. That postfix is the 9-character .restored
string, which is tacked on to the name of a volume that has been restored from
a dump. 

	\section sec5-7 Section 5.7: Functions 

\par
This section covers the Volume Server RPC interface routines, defined by and
generated from the volint.xg Rxgen file. The following is a summary of the
interface functions and their purpose: 

\par Fcn Name 	
AFSVolCreateVolume 	
\par Description 
Create a volume. 

\par Fcn Name 	
AFSVolDeleteVolume 	
\par Description 
Delete a volume. 

\par Fcn Name 	
AFSVolNukeVolume 	
\par Description 
Obliterate a volume completely. 

\par Fcn Name 	
AFSVolDump 	
\par Description 
Dump (i.e., save) the contents of a volume. 

\par Fcn Name 	
AFSVolSignalRestore 	
\par Description 
Show intention to call AFSVolRestore(). 

\par Fcn Name 	
AFSVolRestore 	
\par Description 
Recreate a volume from a dump. 

\par Fcn Name 	
AFSVolForward 	
\par Description 
Dump a volume, then restore to a given server and volume. 

\par Fcn Name 	
AFSVolClone 	
\par Description 
Clone (and optionally purge) a volume. 

\par Fcn Name 	
AFSVolReClone 	
\par Description 
Re-clone a volume. 

\par Fcn Name 	
AFSVolSetForwarding 	
\par Description 
Set forwarding info for a moved volume. 

\par Fcn Name 	
AFSVolTransCreate 	
\par Description 
Create transaction for a [volume, partition]. 

\par Fcn Name 	
AFSVolEndTrans 	
\par Description 
End a transaction. 

\par Fcn Name 	
AFSVolGetFlags 	
\par Description 
Get volume flags for a transaction. 

\par Fcn Name 	
AFSVolSetFlags 	
\par Description 
Set volume flags for a transaction. 

\par Fcn Name 	
AFSVolGetName 	
\par Description 
Get the volume name associated with a transaction. 

\par Fcn Name 	
AFSVolGetStatus 	
\par Description 
Get status of a transaction/volume. 

\par Fcn Name 	
AFSVolSetIdsTypes 	
\par Description 
Set header info for a volume. 

\par Fcn Name 	
AFSVolSetDate 	
\par Description 
Set creation date in a volume. 

\par Fcn Name 	
AFSVolListPartitions 	
\par Description 
Return a list of AFS partitions on a server. 

\par Fcn Name 	
AFSVolPartitionInfo 	
\par Description 
Get partition information. 

\par Fcn Name 	
AFSVolListVolumes 	
\par Description 
Return a list of volumes on the server. 

\par Fcn Name 	
AFSVolListOneVolume 	
\par Description 
Return header info for a single volume. 

\par Fcn Name 	
AFSVolGetNthVolume 	
\par Description 
Get volume header given its index. 

\par Fcn Name 	
AFSVolMonitor 	
\par Description 
Collect server transaction state. 


\par
There are two general comments that apply to most of the Volume Server
interface routines: 
\li 1. AFS partitions are identified by integers ranging from 0 to 25,
corresponding to the letters "a" through "z". By convention, AFS partitions are
named /vicepx, where x is any lower-case letter. 
\li 2. Legal volume types to pass as parameters are RWVOL, ROVOL, and BACKVOL,
as defined in Section 3.2.4. 

	\subsection sec5-7-1 Section 5.7.1: AFSVolCreateVolume - Create a
volume 

\code
int AFSVolCreateVolume(IN struct rx connection *z conn, 
			IN long partition, 
			IN char *name, 
			IN long type, 
			IN long parent, 
			INOUT long *volid, 
			OUT long *trans) 
\endcode
\par Description 
Create a volume named name, with numerical identifier volid, and of type type.
The new volume is to be placed in the specified partition for the server
machine as identified by the Rx connection information pointed to by z conn. If
a value of 0 is provided for the parent argument, it will be set by the Volume
Server to the value of volid itself. The trans parameter is set to the Volume
Location Server transaction ID corresponding to the volume created by this
call, if successful. 
The numerical volume identifier supplied in the volid parameter must be
generated beforehand by calling VL GetNewVolumeID() (see Section 3.6.5). After
AFSVolCreateVolume() completes correctly, the new volume is marked as offline.
It must be explicitly brought online through a call to AFSVolSetFlags() (see
Section 5.7.14) while passing the trans transaction ID generated by
AFSVolCreateVolume(). The "hold" on the new volume guaranteed by the trans
transaction may be "released" by calling AFSVolEnd-Trans(). Until then, no
other process may operate on the volume. 
Upon creation, a volume's maximum quota (as specified in the maxquota field of
a struct volintInfo) is set to 5,000 1-Kbyte blocks. 
Note that the AFSVolCreateVolume() routine is the only Volume Server function
that manufactures its own transaction. All others must have already acquired a
transaction ID via either a previous call to AFSVolCreateVolume() or
AFSVolTransCreate(). 
\par Error Codes 
VOLSERBADNAME The volume name parameter was longer than 31 characters plus the
trailing null. 
\n VOLSERBAD ACCESS The caller is not authorized to create a volume. 
\n EINVAL The type parameter was illegal. E2BIG A value of 0 was provided in
the volid parameter. VOLSERVOLBUSY A transaction could not be created, thus the
given volume was busy. 
\n EIO The new volume entry could not be created. 
\n VOLSERTRELE ERROR The trans transaction's reference count could not be
dropped to the proper level. 
\n <misc> If the partition parameter is unintelligible, this routine will
return a low-level unix error. 

	\subsection sec5-7-2 Section 5.7.2: AFSVolDeleteVolume - Delete a
volume 

\code
int AFSVolDeleteVolume(IN struct rx connection *z conn, IN long trans) 
\endcode
\par Description 
Delete the volume associated with the open transaction ID specified within
trans. All of the file system objects contained within the given volume are
destroyed, and the on-disk volume metadata structures are reclaimed. In
addition, the in-memory volume descriptor's vflags field is set to VTDeleted,
indicating that it has been deleted. 
\par
Under some circumstances, a volume should be deleted by calling
AFSVolNukeVolume() instead of this routine. See Section 5.7.3 for more details. 
\par Error Codes 
VOLSERBAD ACCESS The caller is not authorized to delete a volume. 
\n ENOENT The trans transaction was not found. 
\n VOLSERTRELE ERROR The trans transaction's reference count could not be
dropped to the proper level. 

	\subsection sec5-7-3 Section 5.7.3: AFSVolNukeVolume - Obliterate a
volume completely 

\code
int AFSVolNukeVolume(IN struct rx connection *z conn, 
			IN long partID, 
			IN long volID) 
\endcode
\par Description 
Completely obliterate the volume living on partition partID whose ID is volID.
This involves scanning all inodes on the given partition and removing those
marked with the specified volID. If the volume is a read-only clone, only the
header inodes are removed, since they are the only ones stamped with the
read-only ID. To reclaim the space taken up by the actual data referenced
through a read-only clone, this routine should be called on the read-write
master. Note that calling AFSVolNukeVolume() on a read-write volume effectively
destroys all the read-only volumes cloned from it, since everything except for
their indicies to the (now-deleted) data will be gone. 
\par
Under normal circumstances, it is preferable to use AFSVolDeleteVolume()
instead of AFSVolNukeVolume() to delete a volume. The former is much more
efficient, as it only touches those objects in the partition that belong to the
named volume, walking the on-disk volume metadata structures. However,
AFSVolNukeVolume() must be used in situations where the volume metadata
structures are known to be damaged. Since a complete scan of all inodes in the
partition is performed, all disconnected or unreferenced portions of the given
volume will be reclaimed. 
\par Error Codes 
VOLSERBAD ACCESS The caller is not authorized to call this routine. 
\n VOLSERNOVOL The partition specified by the partID argument is illegal. 

	\subsection sec5-7-4 Section 5.7.4: AFSVolDump - Dump (i.e., save) the
contents of a volume 

\code
int AFSVolDump(IN struct rx connection *z conn, 
		IN long fromTrans, 
		IN long fromDate) 
\endcode
\par Description 
Generate a canonical dump of the contents of the volume associated with
transaction fromTrans as of calendar time fromDate. If the given fromDate is
zero, then a full dump will be carried out. Otherwise, the resulting dump will
be an incremental one. 
\par
This is specified as a split function within the volint.xg Rxgen interface
file. This specifies that two routines are generated, namely StartAFSVolDump()
and EndAFSVolDump(). The former is used to marshall the IN arguments, and the
latter is used to unmarshall the return value of the overall operation. The
actual dump data appears in the Rx stream for the call (see the section
entitled Example Server and Client in the companion AFS-3 Programmer's
Reference: Specification for the Rx Remote Procedure Call Facility document). 
\par Error Codes 
VOLSERBAD ACCESS The caller is not authorized to dump a volume. 
\n ENOENT The fromTrans transaction was not found. 
\n VOLSERTRELE ERROR The trans transaction's reference count could not be
dropped to the proper level. 

	\subsection sec5-7-5 Section 5.7.5: AFSVolSignalRestore - Show
intention to call AFSVolRestore() 

\code
int AFSVolSignalRestore(IN struct rx connection *z conn, 
			IN char *name, 
			IN int type, 
			IN long pid, 
			IN long cloneid) 
\endcode
\par Description 
Show an intention to the Volume Server that the client will soon call
AFSVolRestore(). The parameters, namely the volume name, type, parent ID pid
and clone ID cloneid are stored in a well-known set of global variables. These
values are used to set the restored volume's header, overriding those values
present in the dump from which the volume will be resurrected. 
\par Error Codes 
VOLSERBAD ACCESS The caller is not authorized to call this routine. 
\n VOLSERBADNAME The volume name contained in name was longer than 31
characters plus the trailing null. 

	\subsection sec5-7-6 Section 5.7.6: AFSVolRestore - Recreate a volume
from a dump 

\code
int AFSVolRestore(IN struct rx connection *z conn, 
			IN long toTrans, 
			IN long flags, 
			IN struct restoreCookie *cookie) 
\endcode
\par Description 
Interpret a canonical volume dump (generated as the result of calling
AFSVolDumpVolume()), passing it to the volume specified by the toTrans
transaction. Only the low bit in the flags argument is inspected. If this low
bit is turned on, the dump will be restored as incremental; otherwise, a full
restore will be carried out. 
\par
All callbacks to the restored volume are broken. 
\par
This is specified as a split function within the volint.xg Rxgen interface
file. This specifies that two routines are generated, namely
StartAFSVolRestore() and EndAFSVolRestore() . The former is used to marshall
the IN arguments, and the latter is used to unmarshall the return value of the
overall operation. The actual dump data flows over the Rx stream for the call
(see the section entitled Example Server and Client in the companion AFS-3
Programmer's Reference: Specification for the Rx Remote Procedure Call Facility
document). 
\par
The AFSVolSignalRestore() routine (see Section 5.7.5) should be called before
invoking this function in order to signal the intention to restore a particular
volume. 
\par Error Codes 
VOLSERREAD DUMPERROR Dump data being restored is corrupt. 
\n VOLSERBAD ACCESS The caller is not authorized to restore a volume. 
\n ENOENT The fromTrans transaction was not found. 
\n VOLSERTRELE ERROR The trans transaction's reference count could not be
dropped to the proper level. 

	\subsection sec5-7-7 Section 5.7.7: AFSVolForward - Dump a volume, then
restore to given server and volume 

\code
int AFSVolForward(IN struct rx connection *z conn, 
			IN long fromTrans, 
			IN long fromDate, 
			IN struct destServer *destination, 
			IN long destTrans, 
			IN struct restoreCookie *cookie) 
\endcode
\par Description 
Dumps the volume associated with transaction fromTrans from the given fromDate.
The dump itself is sent to the server described by destination, where it is
restored as the volume associated with transaction destTrans. In reality, an Rx
connection is set up to the destServer, StartAFSVolRestore() directs writing to
the Rx call's stream, and then EndAFSVolRestore() is used to deliver the dump
for the volume corresponding to fromTrans. If a non-zero fromDate is provided,
then the dump will be incremental from that date. Otherwise, a full dump will
be delivered. 
\par
The Rx connection set up for this task is always destroyed before the function
returns. The destination volume should exist before carrying out this
operation, and the invoking process should have started transactions on both
participating volumes. 
\par Error Codes 
VOLSERBAD ACCESS The caller is not authorized to forward a volume. 
\n ENOENT The fromTrans transaction was not found. 
\n VOLSERTRELE ERROR The trans transaction's reference count could not be
dropped to the proper level. 
\n ENOTCONN An Rx connection to the destination server could not be
established. 

	\subsection sec5-7-8 Section 5.7.8: AFSVolClone - Clone (and optionally
purge) a volume 

\code
int AFSVolClone(IN struct rx connection *z conn, 
		IN long trans, 
		IN long purgeVol, 
		IN long newType, 
		IN char *newName, 
		INOUT long *newVol) 
\endcode
\par Description 
Make a clone of the read-write volume associated with transaction trans, giving
the cloned volume a name of newName. The newType parameter specifies the type
for the new clone, and may be either ROVOL or BACKVOL. If purgeVol is set to a
non-zero value, then that volume will be purged during the clone operation.
This may be more efficient that separate clone and purge calls when making
backup volumes. The newVol parameter sets the new clone's ID. It is illegal to
pass a zero in newVol. 
\par Error Codes 
VOLSERBADNAME The volume name contained in newName was longer than 31
characters plus the trailing null. 
\n VOLSERBAD ACCESS The caller is not authorized to clone a volume. 
\n ENOENT The fromTrans transaction was not found. 
\n VOLSERTRELE ERROR The trans transaction's reference count could not be
dropped to the proper level. 
\n VBUSY The given transaction was already in use; indicating that someone else
is currently manipulating the specified clone. 
\n EROFS The volume associated with the given trans is read-only (either ROVOL
or BACKVOL). 
\n EXDEV The volume associated with the trans transaction and the one specified
by purgeVol must be on the same disk device, and they must be cloned from the
same parent volume. 
\n EINVAL The purgeVol must be read-only, i.e. either type ROVOL or BACKVOL. 

	\subsection sec5-7-9 Section 5.7.9: AFSVolReClone - Re-clone a volume 

\code
int AFSVolReClone(IN struct rx connection *z conn, 
			IN long tid, 
			IN long cloneID) 
\endcode
\par Description 
Recreate an existing clone, with identifier cloneID, from the volume associated
with transaction tid. 
\par Error Codes 
VOLSERBAD ACCESS The caller is not authorized to clone a volume. 
\n ENOENT The tid transaction was not found. 
\n VOLSERTRELE ERROR The tid transaction's reference count could not be dropped
to the proper level. 
\n VBUSY The given transaction was already in use; indicating that someone else
is currently manipulating the specified clone. 
\n EROFS The volume to be cloned must be read-write (of type RWVOL). 
\n EXDEV The volume to be cloned and the named clone itself must be on the same
device. Also, cloneID must have been cloned from the volume associated with
transaction tid. 
\n EINVAL The target clone must be a read-only volume (i.e., of type ROVOL or
BACKVOL). 

	\subsection sec5-7-10 Section 5.7.10: AFSVolSetForwarding - Set
forwarding info for a moved volume 

\code
int AFSVolSetForwarding(IN struct rx connection *z conn, 
			IN long tid, 
			IN long newsite) 
\endcode
\par Description 
Record the IP address specified within newsite as the location of the host
which now hosts the volume associated with transaction tid, formerly resident
on the current host. This is intended to gently guide Cache Managers who have
stale volume location cached to the volume's new site, ensuring the move is
transparent to clients using that volume. 
\par Error Codes 
VOLSERBAD ACCESS The caller is not authorized to create a forwarding address. 
\n ENOENT The trans transaction was not found. 

	\subsection sec5-7-11 Section 5.7.11: AFSVolTransCreate - Create
transaction for a [volume, partition] 

\code
int AFSVolTransCreate(IN struct rx connection *z conn, 
			IN long volume, 
			IN long partition, 
			IN long flags, 
			OUT long *trans) 
\endcode
\par Description 
Create a new Volume Server transaction associated with volume ID volume on
partition partition. The type of volume transaction is specified by the flags
parameter. The values in flags specify whether the volume should be treated as
busy (ITBusy), offline (ITOffline), or in shared read-only mode (ITReadOnly).
The identifier for the new transaction built by this function is returned in
trans. 
\par
Creating a transaction serves as a signal to other agents that may be
interested in accessing a volume that it is unavailable while the Volume Server
is manipulating it. This prevents the corruption that could result from
multiple simultaneous operations on a volume. 
\par Error Codes 
EINVAL Illegal value encountered in flags. 
\n VOLSERVOLBUSY A transaction could not be created, thus the given [volume,
partition] pair was busy. 
\n VOLSERTRELE ERROR The trans transaction's reference count could not be
dropped to the proper level after creation. 

	\subsection sec5-7-12 Section 5.7.12: AFSVolEndTrans - End a
transaction 

\code
int AFSVolEndTrans(IN struct rx connection *z conn, 
			IN long trans, 
			OUT long *rcode) 
\endcode
\par Description 
End the transaction identified by trans, returning its final error code into
rcode. This makes the associated [volume, partition] pair eligible for further
Volume Server operations. 
\par Error Codes 
VOLSERBAD ACCESS The caller is not authorized to create a transaction. 
\n ENOENT The trans transaction was not found. 

	\subsection sec5-7-13 Section 5.7.13: AFSVolGetFlags - Get volume flags
for a transaction 

\code
int AFSVolGetFlags(IN struct rx connection *z conn, 
			IN long trans, 
			OUT long *flags) 
\endcode
\par Description 
Return the value of the vflags field of the struct volser trans object
describing the transaction identified as trans. The set of values placed in the
flags parameter is described in Section 5.2.3.1. Briefly, they indicate whether
the volume has been deleted (VTDeleted), out of service (VTOutOfService), or
marked delete-on-salvage (VTDeleteOnSalvage). 
\par Error Codes 
ENOENT The trans transaction was not found. 
\n VOLSERTRELE ERROR The trans transaction's reference count could not be
dropped to the proper level. 

	\subsection sec5-7-14 Section 5.7.14: AFSVolSetFlags - Set volume flags
for a transaction 

\code
int AFSVolSetFlags(IN struct rx connection *z conn, 
			IN long trans, 
			IN long flags) 
\endcode
\par Description 
Set the value of the vflags field of the struct volser trans object describing
the transaction identified as trans to the contents of flags. The set of legal
values for the flags parameter is described in Section 5.2.3.1. Briefly, they
indicate whether the volume has been deleted (VTDeleted), out of service
(VTOutOfService), or marked delete-onsalvage (VTDeleteOnSalvage). 
\par Error Codes 
ENOENT The trans transaction was not found. 
\n EROFS Updates to this volume are not allowed. 
\n VOLSERTRELE ERROR The trans transaction's reference count could not be
dropped to the proper level. 

	\subsection sec5-7-15 Section 5.7.15: AFSVolGetName - Get the volume
name associated with a transaction 

\code
int AFSVolGetName(IN struct rx connection *z conn, 
			IN long tid, 
			OUT char **tname) 
\endcode
\par Description 
Given a transaction identifier tid, return the name of the volume associated
with the given transaction. The tname parameter is set to point to the address
of a string buffer of at most 256 chars containing the desired information,
which is created for this purpose. Note: the caller is responsible for freeing
the buffer pointed to by tname when its information is no longer needed. 
\par Error Codes 
ENOENT The tid transaction was not found, or a volume was not associated with
it (VSrv internal error). 
\n E2BIG The volume name was too big (greater than or equal to SIZE (1,024)
characters. 
\n VOLSERTRELE ERROR The trans transaction's reference count could not be
dropped to the proper level. 

	\subsection sec5-7-16 Section 5.7.16: AFSVolGetStatus - Get status of a
transaction/volume 

\code
int AFSVolGetStatus(IN struct rx connection *z conn, 
			IN long tid, 
			OUT struct volser status *status) 
\endcode
\par Description 
This routine fills the status structure passed as a parameter with status
information for the volume identified by the transaction identified by tid, if
it exists. Included in this status information are the volume's ID, its type,
disk quotas, the IDs of its clones and backup volumes, and several other
administrative details. 
\par Error Codes 
ENOENT The tid transaction was not found. 
\n VOLSERTRELE ERROR The tid transaction's reference count could not be dropped
to the proper level. 

	\subsection sec5-7-17 Section 5.7.17: AFSVolSetIdsTypes - Set header
info for a volume 

\code
int AFSVolSetIdsTypes(IN struct rx connection *z conn, 
			IN long tId 
			IN char *name, 
			IN long type, 
			IN long pId, 
			IN long cloneId, 
			IN long backupId) 
\endcode
\par Description 
The transaction identifed by tId is located, and the values supplied for the
volume name, volume type, parent ID pId, clone ID cloneId and backup ID
backupId are recorded into the given transaction. 
\par Error Codes 
ENOENT The tId transaction was not found. 
\n VOLSERBAD ACCESS The caller is not authorized to call this routine. 
\n VOLSERBADNAME The volume name contained in name was longer than 31
characters plus the trailing null. 
\n VOLSERTRELE ERROR The tId transaction's reference count could not be dropped
to the proper level. 

	\subsection sec5-7-18 Section 5.7.18: AFSVolSetDate - Set creation date
in a volume 

\code
int AFSVolSetDate(IN struct rx connection *z conn, 
			IN long tid, 
			IN long newDate) 
\endcode
\par Description 
Set the creationDate of the struct volintInfo describing the volume associated
with transaction tid to newDate. 
\par Error Codes 
VOLSERBAD ACCESS The caller is not authorized to call this routine. 
\n ENOENT The tId transaction was not found. 
\n VOLSERTRELE ERROR The tid transaction's reference count could not be dropped
to the proper level. 

	\subsection sec5-7-19 Section 5.7.19: AFSVolListPartitions - Return a
list of AFS partitions on a server 

\code
int AFSVolListPartitions(IN struct rx connection *z conn, 
				OUT struct pIDs *partIDs) 
\endcode
\par Description 
Return a list of AFS partitions in use by the server processing this call. The
output parameter is the fixed-length partIDs array, with one slot for each of
26 possible partitions. By convention, AFS partitions are named /vicepx, where
x is any letter. The /vicepa partition is represented by a zero in this array,
/vicepa bya1, andsoon. Unused partitions are represented by slots filled with a
-1. 
\par Error Codes 
---None. 

	\subsection sec5-7-20 Section 5.7.20: AFSVolPartitionInfo - Get
partition information 

\code
int AFSVolPartitionInfo(IN struct rx connection *z conn, 
			IN char *name, 
			OUT struct diskPartition *partition) 
\endcode
\par Description 
Collect information regarding the partition with the given character string
name, and place it into the partition object provided. 
\par Error Codes 
VOLSERBAD ACCESS The caller is not authorized to call this routine. 
\n VOLSERILLEGAL PARTITION An illegal partition was specified by name 

	\subsection sec5-7-21 Section 5.7.21: AFSVolListVolumes - Return a list
of volumes on the server 

\code
int AFSVolListVolumes(IN struct rx connection *z conn, 
			IN long partID, 
			IN long flags, 
			OUT volEntries *resultEntries) 
\endcode
\par Description 
Sweep through all the volumes on the partition identified by partid, filling in
consecutive records in the resultEntries object. If the flags parameter is set
to a non-zero value, then full status information is gathered. Otherwise, just
the volume ID field is written for each record. The fields for a volEntries
object like the one pointed to by resultEntries are described in Section 5.4.6,
which covers the struct volintInfo definition. 
\par Error Codes 
VOLSERILLEGAL PARTITION An illegal partition was specified by partID 
\n VOLSERNO MEMORY Not enough memory was available to hold all the required
entries within resultEntries. 

	\subsection sec5-7-22 Section 5.7.22: AFSVolListOneVolume - Return
header info for a single volume 

\code
int AFSVolListOneVolume(IN struct rx connection *z conn, 
			IN long partID, 
			IN long volid, 
			OUT volEntries *resultEntries) 
\endcode
\par Description 
Find the information for the volume living on partition partID whose ID is
volid, and place a single struct volintInfo entry within the variable-size
resultEntries object. 
\par
This is similar to the AFSVolListVolumes() call, which returns information on
all volumes on the specified partition. The full volume information is always
written into the returned entry (equivalent to setting the flags argument to
AFSVolListVolumes() to a non-zero value). 
\par Error Codes 
VOLSERILLEGAL PARTITION An illegal partition was specified by partID 
\n ENODEV The given volume was not found on the given partition. 

	\subsection sec5-7.23 Section 5.7.23: AFSVolGetNthVolume - Get volume
header given its index 

\code
int AFSVolGetNthVolume(IN struct rx connection *z conn, 
			IN long index, 
			OUT long *volume, 
			OUT long *partition) 
\endcode
\par Description 
Using index as a zero-based index into the set of volumes hosted by the server
chosen by the z conn argument, return the volume ID and partition of residence
for the given index. 
\Note This functionality has not yet been implemented. 
\par Error Codes 
VOLSERNO OP Not implemented. 

	\subsection sec5-7.24 Section 5.7.24: AFSVolMonitor - Collect server
transaction state 

\code
int AFSVolMonitor(IN struct rx connection *z conn, 
			OUT transDebugEntries *result) 
\endcode
\par Description 
This call allows the transaction state of a Volume Server to be monitored for
debugging purposes. Anyone wishing to supervise this Volume Server state may
call this routine, causing all active transactions to be recorded in the given
result object. 
\par Error Codes 
---None. 

	\page biblio Bibliography 

\li [1] Transarc Corporation. AFS 3.0 System Administrator's Guide,
F-30-0-D102, Pittsburgh, PA, April 1990. 
\li [2] Transarc Corporation. AFS 3.0 Command Reference Manual, F-30-0-D103,
Pittsburgh, PA, April 1990. 
\li [3] CMU Information Technology Center. 	Synchronization and Caching
Issues in the Andrew File System, USENIX Proceedings, Dallas, TX, Winter 1988. 
\li [4] Information Technology Center, Carnegie Mellon University. Ubik -A
Library For Managing Ubiquitous Data, ITCID, Pittsburgh, PA, Month, 1988. 
\li [5] Information Technology Center, Carnegie Mellon University. Quorum
Completion, ITCID, Pittsburgh, PA, Month, 1988. 

  @}
*/
