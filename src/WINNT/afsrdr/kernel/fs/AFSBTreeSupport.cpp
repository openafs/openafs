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
AFSLocateHashEntry( IN AFSBTreeEntry *TopNode,
                    IN ULONGLONG HashIndex,
                    IN OUT AFSBTreeEntry **TreeEntry)
{

    NTSTATUS         ntStatus = STATUS_SUCCESS;
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

            AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                          AFS_TRACE_LEVEL_WARNING,
                          "AFSInsertHashEntry Invalid root node\n"));

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

                AFSDbgTrace(( AFS_SUBSYSTEM_FILE_PROCESSING,
                              AFS_TRACE_LEVEL_WARNING,
                              "AFSInsertHashEntry Attempt to re-insert a CRC %I64X\n",
                              FileIDEntry->HashIndex));

                ASSERT( FALSE);

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
