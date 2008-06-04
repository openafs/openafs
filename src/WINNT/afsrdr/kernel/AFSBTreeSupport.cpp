//
// File: AFSBTreeSupport.cpp
//

#include "AFSCommon.h"

NTSTATUS
AFSLocateDirEntry( IN AFSDirEntryCB *RootNode,
                   IN ULONG Index,
                   IN OUT AFSDirEntryCB **DirEntry)
{
    
    NTSTATUS    ntStatus = STATUS_SUCCESS;
    AFSDirEntryCB   *pEntry = NULL;
    AFSDirEntryCB   *pCurrentEntry = NULL;

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

        if( RootNode->TreeEntry.Index == Index)
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

            if( Index > pCurrentEntry->TreeEntry.Index)
            {

                //
                // Go to the next RIGHT entry, if there is one
                //

                if( pCurrentEntry->TreeEntry.rightLink != NULL)
                {

                    pCurrentEntry = (AFSDirEntryCB *)pCurrentEntry->TreeEntry.rightLink;
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
            else if( Index < pCurrentEntry->TreeEntry.Index)
            {

                //
                // Go to the next LEFT entry, if one exists
                //

                if( pCurrentEntry->TreeEntry.leftLink != NULL)
                {

                    pCurrentEntry = (AFSDirEntryCB *)pCurrentEntry->TreeEntry.leftLink;
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
AFSInsertDirEntry( IN AFSDirEntryCB *RootNode,
                   IN AFSDirEntryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirEntryCB *pCurrentEntry = NULL;

    pCurrentEntry = RootNode;

    __Enter
    {

        //
        // If we have no root node then we can;t start the search.
        //

        if( pCurrentEntry == NULL)
        {

            AFSPrint("AFSInsertDirEntry Invalid root node\n");

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

            if( DirEntry->TreeEntry.Index > pCurrentEntry->TreeEntry.Index)
            {
            
                //
                // Go to the next RIGHT entry, if it exists
                //

                if( pCurrentEntry->TreeEntry.rightLink != NULL)
                {
                    pCurrentEntry = (AFSDirEntryCB *)pCurrentEntry->TreeEntry.rightLink;
                }
                else
                {

                    //
                    // Located the end of the branch line so insert the node
                    //

                    pCurrentEntry->TreeEntry.rightLink = (void *)DirEntry;
                        
                    DirEntry->TreeEntry.parentLink = (void *)pCurrentEntry;

                    break;
                }
            }
            else if( DirEntry->TreeEntry.Index < pCurrentEntry->TreeEntry.Index)
            {

                //
                // Go to the next LEFT entry, if it exists
                //

                if( pCurrentEntry->TreeEntry.leftLink != NULL)
                {
                    pCurrentEntry = (AFSDirEntryCB *)pCurrentEntry->TreeEntry.leftLink;
                }
                else
                {

                    //
                    // Located the branch line end so insert the node here
                    //

                    pCurrentEntry->TreeEntry.leftLink = (void *)DirEntry;
                           
                    DirEntry->TreeEntry.parentLink = (void *)pCurrentEntry;

                    break;
                }
            }
            else
            {

                AFSPrint("AFSInsertDirEntry Attempt to re-insert a CRC %08lX\n", DirEntry->TreeEntry.Index);

                ASSERT( FALSE);

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSRemoveDirEntry( IN AFSDirEntryCB **RootNode,
                   IN AFSDirEntryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    AFSDirEntryCB *pRightNode = NULL;
    AFSDirEntryCB *pLeftNode = NULL;
    AFSDirEntryCB *pCurrentNode = NULL;
    AFSDirEntryCB *pParentNode = NULL;

    pRightNode = (AFSDirEntryCB *)DirEntry->TreeEntry.rightLink;
    pLeftNode = (AFSDirEntryCB *)DirEntry->TreeEntry.leftLink;
    pParentNode = (AFSDirEntryCB *)DirEntry->TreeEntry.parentLink;

    __Enter
    {

        if( (pRightNode == NULL) && (pLeftNode == NULL))
        {

            if( pParentNode != NULL)
            {

                if( pParentNode->TreeEntry.leftLink == DirEntry)
                {

                    pParentNode->TreeEntry.leftLink = NULL;
                }
                else
                {

                    pParentNode->TreeEntry.rightLink = NULL;
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
                    if( pParentNode->TreeEntry.rightLink == DirEntry)
                    {

                        pParentNode->TreeEntry.rightLink = pRightNode;
                    }
                    else
                    {

                        pParentNode->TreeEntry.leftLink = pRightNode;
                    }
                }
                else
                {

                    *RootNode = pRightNode;

                    pRightNode->TreeEntry.parentLink = NULL;
                }

                pRightNode->TreeEntry.parentLink = pParentNode;
            }

            if( pLeftNode != NULL)
            {

                // To connect the left node, we must walk the chain of the 
                // right nodes left side until we reach the end.  
                // At the end attach the leftNode
                if( pRightNode != NULL)
                {

                    pCurrentNode = pRightNode;

                    while( pCurrentNode->TreeEntry.leftLink != NULL)
                    {

                        pCurrentNode = (AFSDirEntryCB *)pCurrentNode->TreeEntry.leftLink;
                    }

                    pCurrentNode->TreeEntry.leftLink = pLeftNode;

                    pLeftNode->TreeEntry.parentLink = pCurrentNode;
                }
                else
                {

                    if( pParentNode != NULL)
                    {

                        // This is where we have a left node with no right node.  
                        // So, attach the left node to the parent of
                        // the removed nodes branch
                        if( pParentNode->TreeEntry.rightLink == DirEntry)
                        {

                            pParentNode->TreeEntry.rightLink = pLeftNode;
                        }
                        else
                        {

                            pParentNode->TreeEntry.leftLink = pLeftNode;
                        }

                        pLeftNode->TreeEntry.parentLink = pParentNode;
                    }
                    else
                    {

                        *RootNode = pLeftNode;

                        pLeftNode->TreeEntry.parentLink = NULL;
                    }
                }
            }
        }

        //
        // Cleanup the just removed node
        //

        DirEntry->TreeEntry.leftLink = NULL;
        DirEntry->TreeEntry.parentLink = NULL;
        DirEntry->TreeEntry.rightLink = NULL;

        ntStatus = STATUS_SUCCESS;
    }

    return ntStatus;
}

NTSTATUS
AFSLocateShortNameDirEntry( IN AFSDirEntryCB *RootNode,
                            IN ULONG Index,
                            IN OUT AFSDirEntryCB **DirEntry)
{
    
    NTSTATUS    ntStatus = STATUS_SUCCESS;
    AFSDirEntryCB   *pEntry = NULL;
    AFSDirEntryCB   *pCurrentEntry = NULL;

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

        if( RootNode->Type.Data.ShortNameTreeEntry.Index == Index)
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

            if( Index > pCurrentEntry->Type.Data.ShortNameTreeEntry.Index)
            {

                //
                // Go to the next RIGHT entry, if there is one
                //

                if( pCurrentEntry->Type.Data.ShortNameTreeEntry.rightLink != NULL)
                {

                    pCurrentEntry = (AFSDirEntryCB *)pCurrentEntry->Type.Data.ShortNameTreeEntry.rightLink;
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
            else if( Index < pCurrentEntry->Type.Data.ShortNameTreeEntry.Index)
            {

                //
                // Go to the next LEFT entry, if one exists
                //

                if( pCurrentEntry->Type.Data.ShortNameTreeEntry.leftLink != NULL)
                {

                    pCurrentEntry = (AFSDirEntryCB *)pCurrentEntry->Type.Data.ShortNameTreeEntry.leftLink;
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
AFSInsertShortNameDirEntry( IN AFSDirEntryCB *RootNode,
                            IN AFSDirEntryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_SUCCESS;
    AFSDirEntryCB *pCurrentEntry = NULL;

    pCurrentEntry = RootNode;

    __Enter
    {

        //
        // If we have no root node then we can;t start the search.
        //

        if( pCurrentEntry == NULL)
        {

            AFSPrint("AFSInsertDirEntry Invalid root node\n");

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

            if( DirEntry->Type.Data.ShortNameTreeEntry.Index > pCurrentEntry->Type.Data.ShortNameTreeEntry.Index)
            {
            
                //
                // Go to the next RIGHT entry, if it exists
                //

                if( pCurrentEntry->Type.Data.ShortNameTreeEntry.rightLink != NULL)
                {
                    pCurrentEntry = (AFSDirEntryCB *)pCurrentEntry->Type.Data.ShortNameTreeEntry.rightLink;
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
            else if( DirEntry->Type.Data.ShortNameTreeEntry.Index < pCurrentEntry->Type.Data.ShortNameTreeEntry.Index)
            {

                //
                // Go to the next LEFT entry, if it exists
                //

                if( pCurrentEntry->Type.Data.ShortNameTreeEntry.leftLink != NULL)
                {
                    pCurrentEntry = (AFSDirEntryCB *)pCurrentEntry->Type.Data.ShortNameTreeEntry.leftLink;
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

                AFSPrint("AFSInsertDirEntry Attempt to re-insert a CRC %08lX\n", DirEntry->Type.Data.ShortNameTreeEntry.Index);

                ASSERT( FALSE);

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSRemoveShortNameDirEntry( IN AFSDirEntryCB **RootNode,
                            IN AFSDirEntryCB *DirEntry)
{

    NTSTATUS ntStatus = STATUS_UNSUCCESSFUL;
    AFSDirEntryCB *pRightNode = NULL;
    AFSDirEntryCB *pLeftNode = NULL;
    AFSDirEntryCB *pCurrentNode = NULL;
    AFSDirEntryCB *pParentNode = NULL;

    pRightNode = (AFSDirEntryCB *)DirEntry->Type.Data.ShortNameTreeEntry.rightLink;
    pLeftNode = (AFSDirEntryCB *)DirEntry->Type.Data.ShortNameTreeEntry.leftLink;
    pParentNode = (AFSDirEntryCB *)DirEntry->Type.Data.ShortNameTreeEntry.parentLink;

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

                        pCurrentNode = (AFSDirEntryCB *)pCurrentNode->Type.Data.ShortNameTreeEntry.leftLink;
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
AFSLocateFileIDEntry( IN AFSBTreeEntry *TopNode,
                      IN ULONG Index,
                      IN OUT AFSFcb **Fcb)
{
    
    NTSTATUS         ntStatus = STATUS_SUCCESS;
    AFSBTreeEntry   *pEntry = NULL;
    AFSBTreeEntry   *pCurrentEntry = NULL;

    pCurrentEntry = TopNode;

    __Enter
    {

        *Fcb = NULL;

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

        if( TopNode->Index == Index)
        {

            *Fcb = (AFSFcb *)((char *)TopNode - FIELD_OFFSET( AFSFcb, FileIDTreeEntry));

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

            if( Index > pCurrentEntry->Index)
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
            else if( Index < pCurrentEntry->Index)
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

                *Fcb = (AFSFcb *)((char *)pCurrentEntry - FIELD_OFFSET( AFSFcb, FileIDTreeEntry));

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSInsertFileIDEntry( IN AFSBTreeEntry *TopNode,
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

            AFSPrint("AFSInsertFileIDEntry Invalid root node\n");

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

            if( FileIDEntry->Index > pCurrentEntry->Index)
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
            else if( FileIDEntry->Index < pCurrentEntry->Index)
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

                AFSPrint("AFSInsertFileIDEntry Attempt to re-insert a CRC %08lX\n", FileIDEntry->Index);

                ASSERT( FALSE);

                break;
            }
        }

try_exit:

        NOTHING;
    }

    return ntStatus;
}

NTSTATUS
AFSRemoveFileIDEntry( IN AFSBTreeEntry **TopNode,
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
