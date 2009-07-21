/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include <afs/param.h>


#include	<stdio.h>
#include 	<assert.h>

#include "listVicepx.h"

DirEnt *hash[MAX_HASH_SIZE];
static char *stack[MAX_STACK_SIZE];
static int stackSize;
static char fileName[2048];

/* hashes a number in the range 1.. MAX_HASH_SIZE */
mountHash(num)
     int num;
{
    return (num % MAX_HASH_SIZE);
}


/* insert entry in hash table */
insertHash(dir)
     DirEnt *dir;
{
    int h;
    h = mountHash(dir->vnode);

    /* insert in hash table */
    dir->next = hash[h];
    hash[h] = dir;
}

DirEnt *
lookup(vnode)
     int vnode;
{
    DirEnt *ptr;
    ptr = hash[mountHash(vnode)];
    while (ptr)
	if (ptr->vnode == vnode)
	    return ptr;
	else
	    ptr = ptr->next;
    return 0;
}

char *
getDirName(dir, node)
     DirEnt *dir;
     int node;
{
    int i;
    for (i = 0; i < dir->numEntries; i++)
	if (dir->vnodeName[i].vnode == node)
	    return dir->vnodeName[i].name;
    return 0;
}


/* this shud be called on a vnode for a file only */
char *
getFileName(dir, unique)
     DirEnt *dir;
     int unique;
{
    /* go down the linked list */
    int i;
    for (i = 0; i < dir->numEntries; i++)
	if (dir->vnodeName[i].vunique == unique)
	    return dir->vnodeName[i].name;
    return 0;
}

/* for debugging */
printHash()
{
    int i, j;
    for (i = 0; i < MAX_HASH_SIZE; i++) {
	DirEnt *ptr = hash[i];
	while (ptr) {
#ifdef DEBUG
	    printf("Vnode: %d Parent Vnode : %d \n", ptr->vnode,
		   ptr->vnodeParent);
#endif
	    for (j = 0; j < ptr->numEntries; j++)
		printf("\t %s %d %d\n", ptr->vnodeName[j].name,
		       ptr->vnodeName[j].vnode, ptr->vnodeName[j].vunique);
	    ptr = ptr->next;
	}
    }
}

pushStack(name)
     char *name;
{
    assert(stackSize < MAX_STACK_SIZE);
    assert(stack[stackSize] = (char *)malloc(strlen(name) + 1));
    strcpy(stack[stackSize], name);
    stackSize++;
}

char *
popStack()
{
    if (stackSize == 0)
	return 0;		/* stack empty */
    return stack[--stackSize];
}

char *
printStack()
{
    char *name;
    fileName[0] = 0;
    while (name = popStack()) {
	strcat(fileName, "/");
	strcat(fileName, name);
	free(name);
    }
    return fileName;
}
