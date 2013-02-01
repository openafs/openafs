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

//
// File: AFSBTreeSupport.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSLocateCaseSensitiveDirEntry( IN AFSDirectoryCB *RootNode,
                                IN ULONG Index,
                                IN OUT AFSDirectoryCB **DirEntry)
{

    NTSTATUS    ntStatus = STATUS_SUCCESS;
    AFSDirectoryCB   *pCurrentEntry = NULL;

    pCurrentEntry = RootNode;

    __Enter
    {

        *DirEntry = NULL;

        //
        // If the rootnode passed is null then the directory is empty
        //

        if( RootNode == NULL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // If the requestor is looking for the root node itself, then return it.
        //

        if( RootNode->CaseSensitiveTreeEntry.HashIndex == Index)
        {

            *DirEntry = RootNode;

            try_return( ntStatus);
        }

        //
        // Loop through the nodes in the tree
        //

        while( pCurrentEntry != NULL)
        {

            //
            // Greater values are to the right link.
            //

            if( Index > pCurrentEntry->CaseSensitiveTreeEntry.HashIndex)
            {

                //
                // Go to the next RIGHT entry, if there is one
                //

                if( pCurrentEntry->CaseSensitiveTreeEntry.rightLink != NULL)
                {

                    pCurrentEntry = (AFSDirectoryCB *)pCurrentEntry->CaseSensitiveTreeEntry.rightLink;
                }
                else
                {

                    //
                    // Came to the end of the branch so bail
                    //

                    pCurrentEntry = NULL;

                    break;
                }
            }
            else if( Index < pCurrentEntry->CaseSensitiveTreeEntry.HashIndex)
            {

                //
                // Go to the next LEFT entry, if one exists
                //

                if( pCurrentEntry->CaseSensitiveTreeEntry.leftLink != NULL)
                {

                    pCurrentEntry = (AFSDirectoryCB *)pCurrentEntry->CaseSensitiveTreeEntry.leftLink;
                }
                else
                {

                    //
                    // End of the branch ...
                    //

                    pCurrentEntry = NULL;

                    break;
                }
            }
            else
            {

                //
                // Found the entry.
                //

                *DirEntry = pCurrentEntry;

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSLocateCaseInsensitiveDirEntry( IN AFSDirectoryCB *RootNode,
                                  IN ULONG Index,
                                  IN OUT AFSDirectoryCB **DirEntry)
{

    NTSTATUS    ntStatus = STATUS_SUCCESS;
    AFSDirectoryCB   *pCurrentEntry = NULL;

    pCurrentEntry = RootNode;

    __Enter
    {

        *DirEntry = NULL;

        //
        // If the rootnode passed is null then the directory is empty
        //

        if( RootNode == NULL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // If the requestor is looking for the root node itself, then return it.
        //

        if( RootNode->CaseInsensitiveTreeEntry.HashIndex == Index)
        {

            *DirEntry = RootNode;

            try_return( ntStatus);
        }

        //
        // Loop through the nodes in the tree
        //

        while( pCurrentEntry != NULL)
        {

            //
            // Greater values are to the right link.
            //

            if( Index > pCurrentEntry->CaseInsensitiveTreeEntry.HashIndex)
            {

                //
                // Go to the next RIGHT entry, if there is one
                //

                if( pCurrentEntry->CaseInsensitiveTreeEntry.rightLink != NULL)
                {

                    pCurrentEntry = (AFSDirectoryCB *)pCurrentEntry->CaseInsensitiveTreeEntry.rightLink;
                }
                else
                {

                    //
                    // Came to the end of the branch so bail
                    //

                    pCurrentEntry = NULL;

                    break;
                }
            }
            else if( Index < pCurrentEntry->CaseInsensitiveTreeEntry.HashIndex)
            {

                //
                // Go to the next LEFT entry, if one exists
                //

                if( pCurrentEntry->CaseInsensitiveTreeEntry.leftLink != NULL)
                {

                    pCurrentEntry = (AFSDirectoryCB *)pCurrentEntry->CaseInsensitiveTreeEntry.leftLink;
                }
                else
                {

                    //
                    // End of the branch ...
                    //

                    pCurrentEntry = NULL;

                    break;
                }
            }
            else
            {

                //
                // Found the entry.
                //

                *DirEntry = pCurrentEntry;

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSInsertCaseSensitiveDirEntry( IN AFSDirectoryCB *RootNode,
                                IN AFSDirectoryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirectoryCB *pCurrentEntry = NULL;

    pCurrentEntry = RootNode;

    __Enter
    {

        //
        // If we have no root node then we can;t start the search.
        //

        if( pCurrentEntry == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSInsertCaseSensitiveDirEntry Invalid root node\n");

            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        //
        // Locate the branch end to insert the node
        //

        while( pCurrentEntry != NULL)
        {

            //
            // Greater vlued indices are to the right link
            //

            if( DirEntry->CaseSensitiveTreeEntry.HashIndex > pCurrentEntry->CaseSensitiveTreeEntry.HashIndex)
            {

                //
                // Go to the next RIGHT entry, if it exists
                //

                if( pCurrentEntry->CaseSensitiveTreeEntry.rightLink != NULL)
                {
                    pCurrentEntry = (AFSDirectoryCB *)pCurrentEntry->CaseSensitiveTreeEntry.rightLink;
                }
                else
                {

                    //
                    // Located the end of the branch line so insert the node
                    //

                    pCurrentEntry->CaseSensitiveTreeEntry.rightLink = (void *)DirEntry;

                    DirEntry->CaseSensitiveTreeEntry.parentLink = (void *)pCurrentEntry;

                    break;
                }
            }
            else if( DirEntry->CaseSensitiveTreeEntry.HashIndex < pCurrentEntry->CaseSensitiveTreeEntry.HashIndex)
            {

                //
                // Go to the next LEFT entry, if it exists
                //

                if( pCurrentEntry->CaseSensitiveTreeEntry.leftLink != NULL)
                {
                    pCurrentEntry = (AFSDirectoryCB *)pCurrentEntry->CaseSensitiveTreeEntry.leftLink;
                }
                else
                {

                    //
                    // Located the branch line end so insert the node here
                    //

                    pCurrentEntry->CaseSensitiveTreeEntry.leftLink = (void *)DirEntry;

                    DirEntry->CaseSensitiveTreeEntry.parentLink = (void *)pCurrentEntry;

                    break;
                }
            }
            else
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInsertCaseSensitiveDirEntry Collision with DE %p for %wZ\n",
                              pCurrentEntry,
                              &pCurrentEntry->NameInformation.FileName);

                ntStatus = STATUS_UNSUCCESSFUL;

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSInsertCaseInsensitiveDirEntry( IN AFSDirectoryCB *RootNode,
                                  IN AFSDirectoryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirectoryCB *pCurrentEntry = NULL;

    pCurrentEntry = RootNode;

    __Enter
    {

        //
        // If we have no root node then we can;t start the search.
        //

        if( pCurrentEntry == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSInsertCaseInsensitiveDirEntry Invalid root node\n");

            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        //
        // Locate the branch end to insert the node
        //

        while( pCurrentEntry != NULL)
        {

            //
            // Greater vlued indices are to the right link
            //

            if( DirEntry->CaseInsensitiveTreeEntry.HashIndex > pCurrentEntry->CaseInsensitiveTreeEntry.HashIndex)
            {

                //
                // Go to the next RIGHT entry, if it exists
                //

                if( pCurrentEntry->CaseInsensitiveTreeEntry.rightLink != NULL)
                {
                    pCurrentEntry = (AFSDirectoryCB *)pCurrentEntry->CaseInsensitiveTreeEntry.rightLink;
                }
                else
                {

                    //
                    // Located the end of the branch line so insert the node
                    //

                    pCurrentEntry->CaseInsensitiveTreeEntry.rightLink = (void *)DirEntry;

                    DirEntry->CaseInsensitiveTreeEntry.parentLink = (void *)pCurrentEntry;

                    SetFlag( DirEntry->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD);

                    break;
                }
            }
            else if( DirEntry->CaseInsensitiveTreeEntry.HashIndex < pCurrentEntry->CaseInsensitiveTreeEntry.HashIndex)
            {

                //
                // Go to the next LEFT entry, if it exists
                //

                if( pCurrentEntry->CaseInsensitiveTreeEntry.leftLink != NULL)
                {
                    pCurrentEntry = (AFSDirectoryCB *)pCurrentEntry->CaseInsensitiveTreeEntry.leftLink;
                }
                else
                {

                    //
                    // Located the branch line end so insert the node here
                    //

                    pCurrentEntry->CaseInsensitiveTreeEntry.leftLink = (void *)DirEntry;

                    DirEntry->CaseInsensitiveTreeEntry.parentLink = (void *)pCurrentEntry;

                    SetFlag( DirEntry->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD);

                    break;
                }
            }
            else
            {

                //
                // Insert the the entry at the end of the insensitive list
                //

                while( pCurrentEntry->CaseInsensitiveList.fLink != NULL)
                {

                    pCurrentEntry = (AFSDirectoryCB *)pCurrentEntry->CaseInsensitiveList.fLink;
                }

                pCurrentEntry->CaseInsensitiveList.fLink = (void *)DirEntry;

                DirEntry->CaseInsensitiveList.bLink = (void *)pCurrentEntry;

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSRemoveCaseSensitiveDirEntry( IN AFSDirectoryCB **RootNode,
                                IN AFSDirectoryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    AFSDirectoryCB *pRightNode = NULL;
    AFSDirectoryCB *pLeftNode = NULL;
    AFSDirectoryCB *pCurrentNode = NULL;
    AFSDirectoryCB *pParentNode = NULL;

    pRightNode = (AFSDirectoryCB *)DirEntry->CaseSensitiveTreeEntry.rightLink;
    pLeftNode = (AFSDirectoryCB *)DirEntry->CaseSensitiveTreeEntry.leftLink;
    pParentNode = (AFSDirectoryCB *)DirEntry->CaseSensitiveTreeEntry.parentLink;

    __Enter
    {

        if( (pRightNode == NULL) && (pLeftNode == NULL))
        {

            if( pParentNode != NULL)
            {

                if( pParentNode->CaseSensitiveTreeEntry.leftLink == DirEntry)
                {

                    pParentNode->CaseSensitiveTreeEntry.leftLink = NULL;
                }
                else
                {

                    pParentNode->CaseSensitiveTreeEntry.rightLink = NULL;
                }
            }
            else
            {

                //
                // Removing the top node
                //

                *RootNode = NULL;
            }
        }
        else
        {

            if( pRightNode != NULL)
            {

                if( pParentNode != NULL)
                {

                    // Replace the parent node where this entry was.
                    if( pParentNode->CaseSensitiveTreeEntry.rightLink == DirEntry)
                    {

                        pParentNode->CaseSensitiveTreeEntry.rightLink = pRightNode;
                    }
                    else
                    {

                        pParentNode->CaseSensitiveTreeEntry.leftLink = pRightNode;
                    }
                }
                else
                {

                    *RootNode = pRightNode;

                    pRightNode->CaseSensitiveTreeEntry.parentLink = NULL;
                }

                pRightNode->CaseSensitiveTreeEntry.parentLink = pParentNode;
            }

            if( pLeftNode != NULL)
            {

                // To connect the left node, we must walk the chain of the
                // right nodes left side until we reach the end.
                // At the end attach the leftNode
                if( pRightNode != NULL)
                {

                    pCurrentNode = pRightNode;

                    while( pCurrentNode->CaseSensitiveTreeEntry.leftLink != NULL)
                    {

                        pCurrentNode = (AFSDirectoryCB *)pCurrentNode->CaseSensitiveTreeEntry.leftLink;
                    }

                    pCurrentNode->CaseSensitiveTreeEntry.leftLink = pLeftNode;

                    pLeftNode->CaseSensitiveTreeEntry.parentLink = pCurrentNode;
                }
                else
                {

                    if( pParentNode != NULL)
                    {

                        // This is where we have a left node with no right node.
                        // So, attach the left node to the parent of
                        // the removed nodes branch
                        if( pParentNode->CaseSensitiveTreeEntry.rightLink == DirEntry)
                        {

                            pParentNode->CaseSensitiveTreeEntry.rightLink = pLeftNode;
                        }
                        else
                        {

                            pParentNode->CaseSensitiveTreeEntry.leftLink = pLeftNode;
                        }

                        pLeftNode->CaseSensitiveTreeEntry.parentLink = pParentNode;
                    }
                    else
                    {

                        *RootNode = pLeftNode;

                        pLeftNode->CaseSensitiveTreeEntry.parentLink = NULL;
                    }
                }
            }
        }

        //
        // Cleanup the just removed node
        //

        DirEntry->CaseSensitiveTreeEntry.leftLink = NULL;
        DirEntry->CaseSensitiveTreeEntry.parentLink = NULL;
        DirEntry->CaseSensitiveTreeEntry.rightLink = NULL;

        ntStatus = STATUS_SUCCESS;
    }

    return ntStatus;
}

NTSTATUS
AFSRemoveCaseInsensitiveDirEntry( IN AFSDirectoryCB **RootNode,
                                  IN AFSDirectoryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    AFSDirectoryCB *pRightNode = NULL;
    AFSDirectoryCB *pLeftNode = NULL;
    AFSDirectoryCB *pCurrentNode = NULL;
    AFSDirectoryCB *pParentNode = NULL;
    AFSDirectoryCB *pNewHeadEntry = NULL;

    pRightNode = (AFSDirectoryCB *)DirEntry->CaseInsensitiveTreeEntry.rightLink;
    pLeftNode = (AFSDirectoryCB *)DirEntry->CaseInsensitiveTreeEntry.leftLink;
    pParentNode = (AFSDirectoryCB *)DirEntry->CaseInsensitiveTreeEntry.parentLink;

    __Enter
    {

        //
        // If this is not a head of list entry then just update the pointers for it
        //

        if( !BooleanFlagOn( DirEntry->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD))
        {

            ((AFSDirectoryCB *)DirEntry->CaseInsensitiveList.bLink)->CaseInsensitiveList.fLink = DirEntry->CaseInsensitiveList.fLink;

            if( DirEntry->CaseInsensitiveList.fLink != NULL)
            {

                ((AFSDirectoryCB *)DirEntry->CaseInsensitiveList.fLink)->CaseInsensitiveList.bLink = DirEntry->CaseInsensitiveList.bLink;
            }

            try_return( ntStatus);
        }

        if( DirEntry->CaseInsensitiveList.fLink != NULL)
        {

            //
            // Removing the head of a list of entries so just update pointers
            //

            pNewHeadEntry = (AFSDirectoryCB *)DirEntry->CaseInsensitiveList.fLink;

            if( pParentNode != NULL)
            {

                if( pParentNode->CaseInsensitiveTreeEntry.rightLink == DirEntry)
                {
                    pParentNode->CaseInsensitiveTreeEntry.rightLink = (void *)pNewHeadEntry;
                }
                else
                {
                    pParentNode->CaseInsensitiveTreeEntry.leftLink = (void *)pNewHeadEntry;
                }
            }
            else
            {
                *RootNode = pNewHeadEntry;
            }

            if( pRightNode != NULL)
            {

                pRightNode->CaseInsensitiveTreeEntry.parentLink = (void *)pNewHeadEntry;
            }

            if( pLeftNode != NULL)
            {

                pLeftNode->CaseInsensitiveTreeEntry.parentLink = (void *)pNewHeadEntry;
            }

            pNewHeadEntry->CaseInsensitiveList.bLink = NULL;

            pNewHeadEntry->CaseInsensitiveTreeEntry.parentLink = pParentNode;

            pNewHeadEntry->CaseInsensitiveTreeEntry.leftLink = pLeftNode;

            pNewHeadEntry->CaseInsensitiveTreeEntry.rightLink = pRightNode;

            SetFlag( pNewHeadEntry->Flags, AFS_DIR_ENTRY_CASE_INSENSTIVE_LIST_HEAD);

            try_return( ntStatus);
        }

        if( (pRightNode == NULL) && (pLeftNode == NULL))
        {

            if( pParentNode != NULL)
            {

                if( pParentNode->CaseInsensitiveTreeEntry.leftLink == DirEntry)
                {

                    pParentNode->CaseInsensitiveTreeEntry.leftLink = NULL;
                }
                else
                {

                    pParentNode->CaseInsensitiveTreeEntry.rightLink = NULL;
                }
            }
            else
            {

                //
                // Removing the top node
                //

                *RootNode = NULL;
            }
        }
        else
        {

            if( pRightNode != NULL)
            {

                if( pParentNode != NULL)
                {

                    // Replace the parent node where this entry was.
                    if( pParentNode->CaseInsensitiveTreeEntry.rightLink == DirEntry)
                    {

                        pParentNode->CaseInsensitiveTreeEntry.rightLink = pRightNode;
                    }
                    else
                    {

                        pParentNode->CaseInsensitiveTreeEntry.leftLink = pRightNode;
                    }
                }
                else
                {

                    *RootNode = pRightNode;

                    pRightNode->CaseInsensitiveTreeEntry.parentLink = NULL;
                }

                pRightNode->CaseInsensitiveTreeEntry.parentLink = pParentNode;
            }

            if( pLeftNode != NULL)
            {

                // To connect the left node, we must walk the chain of the
                // right nodes left side until we reach the end.
                // At the end attach the leftNode
                if( pRightNode != NULL)
                {

                    pCurrentNode = pRightNode;

                    while( pCurrentNode->CaseInsensitiveTreeEntry.leftLink != NULL)
                    {

                        pCurrentNode = (AFSDirectoryCB *)pCurrentNode->CaseInsensitiveTreeEntry.leftLink;
                    }

                    pCurrentNode->CaseInsensitiveTreeEntry.leftLink = pLeftNode;

                    pLeftNode->CaseInsensitiveTreeEntry.parentLink = pCurrentNode;
                }
                else
                {

                    if( pParentNode != NULL)
                    {

                        // This is where we have a left node with no right node.
                        // So, attach the left node to the parent of
                        // the removed nodes branch
                        if( pParentNode->CaseInsensitiveTreeEntry.rightLink == DirEntry)
                        {

                            pParentNode->CaseInsensitiveTreeEntry.rightLink = pLeftNode;
                        }
                        else
                        {

                            pParentNode->CaseInsensitiveTreeEntry.leftLink = pLeftNode;
                        }

                        pLeftNode->CaseInsensitiveTreeEntry.parentLink = pParentNode;
                    }
                    else
                    {

                        *RootNode = pLeftNode;

                        pLeftNode->CaseInsensitiveTreeEntry.parentLink = NULL;
                    }
                }
            }
        }

try_exit:

        //
        // Cleanup the just removed node
        //

        DirEntry->CaseInsensitiveTreeEntry.leftLink = NULL;
        DirEntry->CaseInsensitiveTreeEntry.parentLink = NULL;
        DirEntry->CaseInsensitiveTreeEntry.rightLink = NULL;

        DirEntry->CaseInsensitiveList.bLink = NULL;
        DirEntry->CaseInsensitiveList.fLink = NULL;

        ntStatus = STATUS_SUCCESS;
    }

    return ntStatus;
}

NTSTATUS
AFSLocateShortNameDirEntry( IN AFSDirectoryCB *RootNode,
                            IN ULONG Index,
                            IN OUT AFSDirectoryCB **DirEntry)
{

    NTSTATUS    ntStatus = STATUS_SUCCESS;
    AFSDirectoryCB   *pCurrentEntry = NULL;

    pCurrentEntry = RootNode;

    __Enter
    {

        *DirEntry = NULL;

        //
        // If the rootnode passed is null then the directory is empty
        //

        if( RootNode == NULL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // If the requestor is looking for the root node itself, then return it.
        //

        if( RootNode->Type.Data.ShortNameTreeEntry.HashIndex == Index)
        {

            *DirEntry = RootNode;

            try_return( ntStatus);
        }

        //
        // Loop through the nodes in the tree
        //

        while( pCurrentEntry != NULL)
        {

            //
            // Greater values are to the right link.
            //

            if( Index > pCurrentEntry->Type.Data.ShortNameTreeEntry.HashIndex)
            {

                //
                // Go to the next RIGHT entry, if there is one
                //

                if( pCurrentEntry->Type.Data.ShortNameTreeEntry.rightLink != NULL)
                {

                    pCurrentEntry = (AFSDirectoryCB *)pCurrentEntry->Type.Data.ShortNameTreeEntry.rightLink;
                }
                else
                {

                    //
                    // Came to the end of the branch so bail
                    //

                    pCurrentEntry = NULL;

                    break;
                }
            }
            else if( Index < pCurrentEntry->Type.Data.ShortNameTreeEntry.HashIndex)
            {

                //
                // Go to the next LEFT entry, if one exists
                //

                if( pCurrentEntry->Type.Data.ShortNameTreeEntry.leftLink != NULL)
                {

                    pCurrentEntry = (AFSDirectoryCB *)pCurrentEntry->Type.Data.ShortNameTreeEntry.leftLink;
                }
                else
                {

                    //
                    // End of the branch ...
                    //

                    pCurrentEntry = NULL;

                    break;
                }
            }
            else
            {

                //
                // Found the entry.
                //

                *DirEntry = pCurrentEntry;

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSInsertShortNameDirEntry( IN AFSDirectoryCB *RootNode,
                            IN AFSDirectoryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirectoryCB *pCurrentEntry = NULL;

    pCurrentEntry = RootNode;

    __Enter
    {

        //
        // If we have no root node then we can;t start the search.
        //

        if( pCurrentEntry == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSInsertShortNameDirEntry Invalid root node\n");

            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        //
        // Locate the branch end to insert the node
        //

        while( pCurrentEntry != NULL)
        {

            //
            // Greater valued indices are to the right link
            //

            if( DirEntry->Type.Data.ShortNameTreeEntry.HashIndex > pCurrentEntry->Type.Data.ShortNameTreeEntry.HashIndex)
            {

                //
                // Go to the next RIGHT entry, if it exists
                //

                if( pCurrentEntry->Type.Data.ShortNameTreeEntry.rightLink != NULL)
                {
                    pCurrentEntry = (AFSDirectoryCB *)pCurrentEntry->Type.Data.ShortNameTreeEntry.rightLink;
                }
                else
                {

                    //
                    // Located the end of the branch line so insert the node
                    //

                    pCurrentEntry->Type.Data.ShortNameTreeEntry.rightLink = (void *)DirEntry;

                    DirEntry->Type.Data.ShortNameTreeEntry.parentLink = (void *)pCurrentEntry;

                    break;
                }
            }
            else if( DirEntry->Type.Data.ShortNameTreeEntry.HashIndex < pCurrentEntry->Type.Data.ShortNameTreeEntry.HashIndex)
            {

                //
                // Go to the next LEFT entry, if it exists
                //

                if( pCurrentEntry->Type.Data.ShortNameTreeEntry.leftLink != NULL)
                {
                    pCurrentEntry = (AFSDirectoryCB *)pCurrentEntry->Type.Data.ShortNameTreeEntry.leftLink;
                }
                else
                {

                    //
                    // Located the branch line end so insert the node here
                    //

                    pCurrentEntry->Type.Data.ShortNameTreeEntry.leftLink = (void *)DirEntry;

                    DirEntry->Type.Data.ShortNameTreeEntry.parentLink = (void *)pCurrentEntry;

                    break;
                }
            }
            else
            {

                ntStatus = STATUS_UNSUCCESSFUL;

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_VERBOSE,
                              "AFSInsertShortNameDirEntry Collision with DE %p for shortname %S and %wZ\n",
                              pCurrentEntry,
                              pCurrentEntry->NameInformation.ShortName,
                              &pCurrentEntry->NameInformation.FileName);

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSRemoveShortNameDirEntry( IN AFSDirectoryCB **RootNode,
                            IN AFSDirectoryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    AFSDirectoryCB *pRightNode = NULL;
    AFSDirectoryCB *pLeftNode = NULL;
    AFSDirectoryCB *pCurrentNode = NULL;
    AFSDirectoryCB *pParentNode = NULL;

    pRightNode = (AFSDirectoryCB *)DirEntry->Type.Data.ShortNameTreeEntry.rightLink;
    pLeftNode = (AFSDirectoryCB *)DirEntry->Type.Data.ShortNameTreeEntry.leftLink;
    pParentNode = (AFSDirectoryCB *)DirEntry->Type.Data.ShortNameTreeEntry.parentLink;

    __Enter
    {

        if( (pRightNode == NULL) && (pLeftNode == NULL))
        {

            if( pParentNode != NULL)
            {

                if( pParentNode->Type.Data.ShortNameTreeEntry.leftLink == DirEntry)
                {

                    pParentNode->Type.Data.ShortNameTreeEntry.leftLink = NULL;
                }
                else
                {

                    pParentNode->Type.Data.ShortNameTreeEntry.rightLink = NULL;
                }
            }
            else
            {

                //
                // Removing the top node
                //

                *RootNode = NULL;
            }
        }
        else
        {

            if( pRightNode != NULL)
            {

                if( pParentNode != NULL)
                {

                    // Replace the parent node where this entry was.
                    if( pParentNode->Type.Data.ShortNameTreeEntry.rightLink == DirEntry)
                    {

                        pParentNode->Type.Data.ShortNameTreeEntry.rightLink = pRightNode;
                    }
                    else
                    {

                        pParentNode->Type.Data.ShortNameTreeEntry.leftLink = pRightNode;
                    }
                }
                else
                {

                    *RootNode = pRightNode;

                    pRightNode->Type.Data.ShortNameTreeEntry.parentLink = NULL;
                }

                pRightNode->Type.Data.ShortNameTreeEntry.parentLink = pParentNode;
            }

            if( pLeftNode != NULL)
            {

                // To connect the left node, we must walk the chain of the
                // right nodes left side until we reach the end.
                // At the end attach the leftNode
                if( pRightNode != NULL)
                {

                    pCurrentNode = pRightNode;

                    while( pCurrentNode->Type.Data.ShortNameTreeEntry.leftLink != NULL)
                    {

                        pCurrentNode = (AFSDirectoryCB *)pCurrentNode->Type.Data.ShortNameTreeEntry.leftLink;
                    }

                    pCurrentNode->Type.Data.ShortNameTreeEntry.leftLink = pLeftNode;

                    pLeftNode->Type.Data.ShortNameTreeEntry.parentLink = pCurrentNode;
                }
                else
                {

                    if( pParentNode != NULL)
                    {

                        // This is where we have a left node with no right node.
                        // So, attach the left node to the parent of
                        // the removed nodes branch
                        if( pParentNode->Type.Data.ShortNameTreeEntry.rightLink == DirEntry)
                        {

                            pParentNode->Type.Data.ShortNameTreeEntry.rightLink = pLeftNode;
                        }
                        else
                        {

                            pParentNode->Type.Data.ShortNameTreeEntry.leftLink = pLeftNode;
                        }

                        pLeftNode->Type.Data.ShortNameTreeEntry.parentLink = pParentNode;
                    }
                    else
                    {

                        *RootNode = pLeftNode;

                        pLeftNode->Type.Data.ShortNameTreeEntry.parentLink = NULL;
                    }
                }
            }
        }

        //
        // Cleanup the just removed node
        //

        DirEntry->Type.Data.ShortNameTreeEntry.leftLink = NULL;
        DirEntry->Type.Data.ShortNameTreeEntry.parentLink = NULL;
        DirEntry->Type.Data.ShortNameTreeEntry.rightLink = NULL;

        ntStatus = STATUS_SUCCESS;
    }

    return ntStatus;
}

NTSTATUS
AFSLocateHashEntry( IN AFSBTreeEntry *TopNode,
                    IN ULONGLONG HashIndex,
                    IN OUT AFSBTreeEntry **TreeEntry)
{

    NTSTATUS         ntStatus = STATUS_NOT_FOUND;
    AFSBTreeEntry   *pCurrentEntry = NULL;

    pCurrentEntry = TopNode;

    __Enter
    {

        //
        // If the rootnode passed is null then the directory is empty
        //

        if( TopNode == NULL)
        {

            try_return( ntStatus = STATUS_INVALID_PARAMETER);
        }

        //
        // If the requestor is looking for the root node itself, then return it.
        //

        if( TopNode->HashIndex == HashIndex)
        {

            *TreeEntry = TopNode;

            try_return( ntStatus = STATUS_SUCCESS);
        }

        //
        // Loop through the nodes in the tree
        //

        while( pCurrentEntry != NULL)
        {

            //
            // Greater values are to the right link.
            //

            if( HashIndex > pCurrentEntry->HashIndex)
            {

                //
                // Go to the next RIGHT entry, if there is one
                //

                if( pCurrentEntry->rightLink != NULL)
                {

                    pCurrentEntry = (AFSBTreeEntry *)pCurrentEntry->rightLink;
                }
                else
                {

                    //
                    // Came to the end of the branch so bail
                    //

                    pCurrentEntry = NULL;

                    break;
                }
            }
            else if( HashIndex < pCurrentEntry->HashIndex)
            {

                //
                // Go to the next LEFT entry, if one exists
                //

                if( pCurrentEntry->leftLink != NULL)
                {

                    pCurrentEntry = (AFSBTreeEntry *)pCurrentEntry->leftLink;
                }
                else
                {

                    //
                    // End of the branch ...
                    //

                    pCurrentEntry = NULL;

                    break;
                }
            }
            else
            {

                //
                // Found the entry.
                //

                *TreeEntry = pCurrentEntry;

                ntStatus = STATUS_SUCCESS;

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSInsertHashEntry( IN AFSBTreeEntry *TopNode,
                    IN AFSBTreeEntry *FileIDEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSBTreeEntry *pCurrentEntry = NULL;

    pCurrentEntry = TopNode;

    __Enter
    {

        //
        // If we have no root node then we can;t start the search.
        //

        if( pCurrentEntry == NULL)
        {

            AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSInsertHashEntry Invalid root node\n");

            try_return( ntStatus = STATUS_UNSUCCESSFUL);
        }

        //
        // Locate the branch end to insert the node
        //

        while( pCurrentEntry != NULL)
        {

            //
            // Greater vlued indices are to the right link
            //

            if( FileIDEntry->HashIndex > pCurrentEntry->HashIndex)
            {

                //
                // Go to the next RIGHT entry, if it exists
                //

                if( pCurrentEntry->rightLink != NULL)
                {
                    pCurrentEntry = (AFSBTreeEntry *)pCurrentEntry->rightLink;
                }
                else
                {

                    //
                    // Located the end of the branch line so insert the node
                    //

                    pCurrentEntry->rightLink = (void *)FileIDEntry;

                    FileIDEntry->parentLink = (void *)pCurrentEntry;

                    break;
                }
            }
            else if( FileIDEntry->HashIndex < pCurrentEntry->HashIndex)
            {

                //
                // Go to the next LEFT entry, if it exists
                //

                if( pCurrentEntry->leftLink != NULL)
                {
                    pCurrentEntry = (AFSBTreeEntry *)pCurrentEntry->leftLink;
                }
                else
                {

                    //
                    // Located the branch line end so insert the node here
                    //

                    pCurrentEntry->leftLink = (void *)FileIDEntry;

                    FileIDEntry->parentLink = (void *)pCurrentEntry;

                    break;
                }
            }
            else
            {

                AFSDbgLogMsg( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSInsertHashEntry Attempt to re-insert a CRC %I64X\n",
                              FileIDEntry->HashIndex);

                ntStatus = STATUS_UNSUCCESSFUL;

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSRemoveHashEntry( IN AFSBTreeEntry **TopNode,
                    IN AFSBTreeEntry *FileIDEntry)
{

    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    AFSBTreeEntry *pRightNode = NULL;
    AFSBTreeEntry *pLeftNode = NULL;
    AFSBTreeEntry *pCurrentNode = NULL;
    AFSBTreeEntry *pParentNode = NULL;

    pRightNode = (AFSBTreeEntry *)FileIDEntry->rightLink;
    pLeftNode = (AFSBTreeEntry *)FileIDEntry->leftLink;
    pParentNode = (AFSBTreeEntry *)FileIDEntry->parentLink;

    __Enter
    {

        if( (pRightNode == NULL) && (pLeftNode == NULL))
        {

            if( pParentNode != NULL)
            {

                if( pParentNode->leftLink == FileIDEntry)
                {

                    pParentNode->leftLink = NULL;
                }
                else
                {

                    pParentNode->rightLink = NULL;
                }
            }
            else
            {

                //
                // Removing the top node
                //

                *TopNode = NULL;
            }
        }
        else
        {

            if( pRightNode != NULL)
            {

                if( pParentNode != NULL)
                {

                    // Replace the parent node where this entry was.
                    if( pParentNode->rightLink == FileIDEntry)
                    {

                        pParentNode->rightLink = pRightNode;
                    }
                    else
                    {

                        pParentNode->leftLink = pRightNode;
                    }
                }
                else
                {

                    *TopNode = pRightNode;

                    pRightNode->parentLink = NULL;
                }

                pRightNode->parentLink = pParentNode;
            }

            if( pLeftNode != NULL)
            {

                // To connect the left node, we must walk the chain of the
                // right nodes left side until we reach the end.
                // At the end attach the leftNode
                if( pRightNode != NULL)
                {

                    pCurrentNode = pRightNode;

                    while( pCurrentNode->leftLink != NULL)
                    {

                        pCurrentNode = (AFSBTreeEntry *)pCurrentNode->leftLink;
                    }

                    pCurrentNode->leftLink = pLeftNode;

                    pLeftNode->parentLink = pCurrentNode;
                }
                else
                {

                    if( pParentNode != NULL)
                    {

                        // This is where we have a left node with no right node.
                        // So, attach the left node to the parent of
                        // the removed nodes branch
                        if( pParentNode->rightLink == FileIDEntry)
                        {

                            pParentNode->rightLink = pLeftNode;
                        }
                        else
                        {

                            pParentNode->leftLink = pLeftNode;
                        }

                        pLeftNode->parentLink = pParentNode;
                    }
                    else
                    {

                        *TopNode = pLeftNode;

                        pLeftNode->parentLink = NULL;
                    }
                }
            }
        }

        //
        // Cleanup the just removed node
        //

        FileIDEntry->leftLink = NULL;
        FileIDEntry->parentLink = NULL;
        FileIDEntry->rightLink = NULL;

        ntStatus = STATUS_SUCCESS;
    }

    return ntStatus;
}
