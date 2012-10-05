/*
 * Copyright (c) 2008, 2009, 2010, 2011 Kernel Drivers, LLC.
 * Copyright (c) 2009, 2010, 2011 Your File System, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice,
 *   this list of conditions and the following disclaimer in the
 *   documentation
 *   and/or other materials provided with the distribution.
 * - Neither the names of Kernel Drivers, LLC and Your File System, Inc.
 *   nor the names of their contributors may be used to endorse or promote
 *   products derived from this software without specific prior written
 *   permission from Kernel Drivers, LLC and Your File System, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AFS_STRUCTS_H
#define _AFS_STRUCTS_H

//
// File: AFSStructs.h
//

typedef struct _AFS_DIR_HDR
{

    struct _AFS_DIRECTORY_CB *CaseSensitiveTreeHead;

    struct _AFS_DIRECTORY_CB *CaseInsensitiveTreeHead;

    ERESOURCE               *TreeLock;

    LONG                     ContentIndex;

} AFSDirHdr;

//
// Worker pool header
//

typedef struct _AFS_WORKER_QUEUE_HDR
{

    struct _AFS_WORKER_QUEUE_HDR    *fLink;

    KEVENT           WorkerThreadReady;

    void            *WorkerThreadObject;

    ULONG            State;

} AFSWorkQueueContext, *PAFSWorkQueueContext;

//
// These are the context control blocks for the open instance
//

typedef struct _AFS_NONPAGED_CCB
{

    ERESOURCE           CcbLock;

} AFSNonPagedCcb;


typedef struct _AFS_CCB
{

    USHORT        Size;

    USHORT        Type;

    ULONG         Flags;

    AFSNonPagedCcb  *NPCcb;

    AFSListEntry  ListEntry;

    //
    // Directory enumeration informaiton
    //

    UNICODE_STRING MaskName;

    struct _AFS_DIRECTORY_SS_HDR    *DirectorySnapshot;

    ULONG          CurrentDirIndex;

    //
    // PIOCtl and share interface request id
    //

    ULONG          RequestID;

    //
    // Full path of how the instance was opened
    //

    UNICODE_STRING FullFileName;

    //
    // Name array for this open
    //

    struct _AFS_NAME_ARRAY_HEADER  *NameArray;

    //
    // Pointer to this entries meta data
    //

    struct _AFS_DIRECTORY_CB  *DirectoryCB;

    //
    // Notification name
    //

    UNICODE_STRING          NotifyMask;

    ACCESS_MASK             GrantedAccess;

    //
    // File unwind info
    //

    struct
    {

        ULONG           FileAttributes;

        LARGE_INTEGER   CreationTime;

        LARGE_INTEGER   LastAccessTime;

        LARGE_INTEGER   LastWriteTime;

        LARGE_INTEGER   ChangeTime;

    } FileUnwindInfo;

    //
    // Granted File Access
    //

    ULONG               FileAccess;

    //
    // Authentication group GUID
    //

    GUID                AuthGroup;

} AFSCcb;

//
// Object information block
//

typedef struct _AFS_NONPAGED_OBJECT_INFO_CB
{

    ERESOURCE           DirectoryNodeHdrLock;

} AFSNonPagedObjectInfoCB;

typedef struct _AFS_OBJECT_INFORMATION_CB
{

    AFSBTreeEntry             TreeEntry;

    AFSListEntry              ListEntry;

    ULONG                     Flags;

    LONG                      ObjectReferenceCount;

    AFSNonPagedObjectInfoCB  *NonPagedInfo;

    //
    // The VolumeCB where this entry resides
    //

    struct _AFS_VOLUME_CB    *VolumeCB;

    //
    // Parent object information
    //

    struct _AFS_OBJECT_INFORMATION_CB   *ParentObjectInformation;

    //
    // Pointer to the current Fcb, if available
    //

    AFSFcb                   *Fcb;

    //
    // Last access time.
    //

    LARGE_INTEGER           LastAccessCount;

    //
    // Per file metadata information
    //

    AFSFileID	            FileId;

    AFSFileID               TargetFileId;

    LARGE_INTEGER           Expiration;		/* FILETIME */

    LARGE_INTEGER           DataVersion;

    ULONG                   FileType;		/* File, Dir, MountPoint, Symlink */

    LARGE_INTEGER           CreationTime;	/* FILETIME */

    LARGE_INTEGER           LastAccessTime;	/* FILETIME */

    LARGE_INTEGER           LastWriteTime;	/* FILETIME */

    LARGE_INTEGER           ChangeTime;		/* FILETIME */

    ULONG                   FileAttributes;	/* NTFS FILE_ATTRIBUTE_xxxx see below */

    LARGE_INTEGER           EndOfFile;

    LARGE_INTEGER           AllocationSize;

    ULONG                   EaSize;

    ULONG                   Links;

    //
    // Directory and file specific information
    //

	union
	{

		struct
		{

			//
			// The directory search and listing information for the node
			//

			AFSDirHdr                  DirectoryNodeHdr;

			struct _AFS_DIRECTORY_CB  *DirectoryNodeListHead;

			struct _AFS_DIRECTORY_CB  *DirectoryNodeListTail;

                        LONG                       DirectoryNodeCount;

			struct _AFS_DIRECTORY_CB  *ShortNameTree;

                        //
                        // PIOCtl directory cb entry
                        //

                        struct _AFS_DIRECTORY_CB  *PIOCtlDirectoryCB;

                        //
                        // Open handle and reference count for this object
                        //

                        LONG                       ChildOpenHandleCount;

                        LONG                       ChildOpenReferenceCount;

                        //
                        // Index for the PIOCtl and share open count
                        //

                        LONG                OpenRequestIndex;

		} Directory;

		struct
		{

                        ULONG       Reserved;

		} File;

	} Specific;

} AFSObjectInfoCB;

//
// Volume control block structure
//

typedef struct _AFS_NON_PAGED_VOLUME_CB
{

    ERESOURCE           VolumeLock;

    ERESOURCE           ObjectInfoTreeLock;

    ERESOURCE           DirectoryNodeHdrLock;

}AFSNonPagedVolumeCB;

typedef struct _AFS_VOLUME_CB
{

    //
    // Our tree entry.
    //

    AFSBTreeEntry             TreeEntry;

    //
    // This is the linked list of nodes processed asynchronously by the respective worker thread
    //

    AFSListEntry              ListEntry;

    ULONG                     Flags;

    AFSNonPagedVolumeCB      *NonPagedVcb;

    ERESOURCE                *VolumeLock;

    //
    // Reference count on the object
    //

    LONG                      VolumeReferenceCount;

    //
    // Object information tree
    //

    AFSTreeHdr                ObjectInfoTree;

    AFSObjectInfoCB          *ObjectInfoListHead;

    AFSObjectInfoCB          *ObjectInfoListTail;

    //
    // Object information for the volume
    //

    AFSObjectInfoCB           ObjectInformation;

    //
    // Root directory cb for this volume
    //

    struct _AFS_DIRECTORY_CB *DirectoryCB;

    //
    // The Fcb for this volume
    //

    AFSFcb                   *RootFcb;

    //
    // Volume worker thread
    //

    AFSWorkQueueContext       VolumeWorkerContext;

    //
    // Volume information
    //

    AFSVolumeInfoCB           VolumeInformation;

} AFSVolumeCB;

typedef struct _AFS_NAME_INFORMATION_CB
{

    CCHAR           ShortNameLength;

    WCHAR           ShortName[12];

    UNICODE_STRING  FileName;

    UNICODE_STRING  TargetName;

} AFSNameInfoCB;

typedef struct _AFS_NON_PAGED_DIRECTORY_CB
{

    ERESOURCE       Lock;

} AFSNonPagedDirectoryCB;

typedef struct _AFS_DIRECTORY_CB
{

    AFSBTreeEntry    CaseSensitiveTreeEntry;    // For entries in the NameEntry tree, the
                                                // Index is a CRC on the name. For Volume,
                                                // MP and SL nodes, the Index is the Cell, Volume
                                                // For all others it is the vnode, uniqueid

    AFSBTreeEntry    CaseInsensitiveTreeEntry;

    AFSListEntry     CaseInsensitiveList;

    ULONG            Flags;

    //
    // Current open reference count on the directory entry. This count is used
    // for tear down
    //

    LONG             OpenReferenceCount;

    //
    // File index used in directory enumerations
    //

    ULONG            FileIndex;

    //
    // Name information for this entry
    //

    AFSNameInfoCB    NameInformation;

    //
    // List entry for the directory enumeration list in a parent node
    //

    AFSListEntry     ListEntry;

    //
    // Back pointer to the ObjectInfo block for this entry
    //

    AFSObjectInfoCB *ObjectInformation;

    //
    // Non paged pointer
    //

    AFSNonPagedDirectoryCB  *NonPaged;

    //
    // Type specific information
    //

    union
    {

        struct
        {

            AFSBTreeEntry    ShortNameTreeEntry;

        } Data;

        struct
        {

            ULONG           Reserved;

        } MountPoint;

        struct
        {

            ULONG           Reserved;

        } SymLink;

    } Type;

} AFSDirectoryCB;

// Read and writes can fan out and so they are syncrhonized via one of
// these structures
//

typedef struct _AFS_GATHER_READWRITE
{
    KEVENT     Event;

    LONG       Count;

    NTSTATUS   Status;

    PIRP       MasterIrp;

    BOOLEAN    Synchronous;

    BOOLEAN    CompleteMasterIrp;

    AFSFcb    *Fcb;

} AFSGatherIo;

typedef struct _AFS_IO_RUNS {

    LARGE_INTEGER CacheOffset;
    PIRP          ChildIrp;
    ULONG         ByteCount;
} AFSIoRun;

//
// Name array element and header
//

typedef struct _AFS_NAME_ARRAY_ELEMENT
{

    UNICODE_STRING      Component;

    AFSFileID           FileId;

    AFSDirectoryCB     *DirectoryCB;

    ULONG               Flags;

} AFSNameArrayCB;

typedef struct _AFS_NAME_ARRAY_HEADER
{

    AFSNameArrayCB      *CurrentEntry;

    LONG                 Count;

    LONG                 LinkCount;

    ULONG                Flags;

    ULONG                MaxElementCount;

    AFSNameArrayCB       ElementArray[ 1];

} AFSNameArrayHdr;

typedef struct _AFS_FILE_INFO_CB
{

    ULONG           FileAttributes;

    LARGE_INTEGER   AllocationSize;

    LARGE_INTEGER   EndOfFile;

    LARGE_INTEGER   CreationTime;

    LARGE_INTEGER   LastAccessTime;

    LARGE_INTEGER   LastWriteTime;

    LARGE_INTEGER   ChangeTime;

} AFSFileInfoCB;

//
// Work item
//

typedef struct _AFS_WORK_ITEM
{

    struct _AFS_WORK_ITEM *next;

    ULONG    RequestType;

    ULONG    RequestFlags;

    NTSTATUS Status;

    KEVENT   Event;

    ULONG    Size;

    ULONGLONG   ProcessID;

    GUID     AuthGroup;

    union
    {
        struct
        {
            PIRP Irp;

        } ReleaseExtents;

        struct
        {
            AFSFcb *Fcb;

            AFSFcb **TargetFcb;

            AFSFileInfoCB   FileInfo;

            struct _AFS_NAME_ARRAY_HEADER *NameArray;

        } Fcb;

        struct
        {
            PIRP Irp;

            PDEVICE_OBJECT Device;

            HANDLE CallingProcess;

        } AsynchIo;

        struct
        {

            UCHAR FunctionCode;

            ULONG RequestFlags;

            struct _AFS_IO_RUNS    *IoRuns;

            ULONG RunCount;

            struct _AFS_GATHER_READWRITE *GatherIo;

            FILE_OBJECT *CacheFileObject;

        } CacheAccess;

        struct
        {

            AFSObjectInfoCB *ObjectInfo;

            ULONG            InvalidateReason;

        } Invalidate;

        struct
        {
            char     Context[ 1];

        } Other;

    } Specific;

} AFSWorkItem, *PAFSWorkItem;

//
// Directory snapshot structures
//

typedef struct _AFS_DIRECTORY_SS_ENTRY
{

    ULONG       NameHash;

} AFSSnapshotEntry;

typedef struct _AFS_DIRECTORY_SS_HDR
{

    ULONG       EntryCount;

    AFSSnapshotEntry    *TopEntry;

} AFSSnapshotHdr;

typedef struct _AFS_BYTE_RANGE
{

    LARGE_INTEGER       FileOffset;

    LARGE_INTEGER       Length;

} AFSByteRange;

#endif /* _AFS_STRUCTS_H */
