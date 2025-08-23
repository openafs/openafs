/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*
	(Multiple) readers & writers test of LWP stuff.

Created: 11/1/83, J. Rosenberg

*/

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>

#include "lwp.h"
#include "afs/afs_lock.h"

#define DEFAULT_READERS	5

#define STACK_SIZE	(16*1024)

/* The shared queue */
typedef struct QUEUE {
    struct QUEUE *prev, *next;
    char *data;
    struct Lock lock;
} queue;

queue *
init(void)
{
    queue *q;

    q = malloc(sizeof(queue));
    q->prev = q->next = q;
    return (q);
}

char
empty(queue *q)
{
    return (q->prev == q && q->next == q);
}

void
insert(queue * q, char *s)
{
    queue *new;

    new = malloc(sizeof(queue));
    new->data = s;
    new->prev = q->prev;
    q->prev->next = new;
    q->prev = new;
    new->next = q;
}

char *
Remove(queue *q)
{
    queue *old;
    char *s;

    if (empty(q)) {
	printf("Remove from empty queue");
	exit(0);
    }

    old = q->next;
    q->next = old->next;
    q->next->prev = q;
    s = old->data;
    free(old);
    return (s);
}

queue *q;

int asleep;			/* Number of processes sleeping -- used for
				 * clean termination */

static void *
read_process(void *arg)
{
    int *id = (int *) arg;
    printf("\t[Reader %d]\n", *id);
    LWP_DispatchProcess();	/* Just relinquish control for now */

    for (;;) {
	int i;

	/* Wait until there is something in the queue */
	asleep++;
	ObtainReadLock(&q->lock);
	while (empty(q)) {
	    ReleaseReadLock(&q->lock);
	    LWP_WaitProcess(q);
	    ObtainReadLock(&q->lock);
	}
	asleep--;
	for (i = 0; i < 10000; i++);
	printf("[%d: %s]\n", *id, Remove(q));
	ReleaseReadLock(&q->lock);
	LWP_DispatchProcess();
    }
    return 0;
}

static void *
write_process(void *dummy)
{
    static char *messages[] = {
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	"Mary had a little lamb,",
	"Its fleece was white as snow,",
	"And everywhere that Mary went,",
	"The lamb was sure to go",
	0
    };
    char **mesg;

    printf("\t[Writer]\n");

    /* Now loop & write data */
    for (mesg = messages; *mesg != 0; mesg++) {
	ObtainWriteLock(&q->lock);
	insert(q, *mesg);
	ReleaseWriteLock(&q->lock);
	LWP_SignalProcess(q);
    }

    asleep++;
    return 0;
}

/*
	Arguments:
		0:	Unix junk, ignore
		1:	Number of readers to create (default is DEFAULT_READERS)
		2:	# msecs for interrupt (to satisfy Larry)
		3:	Present if lwp_debug to be set
*/

#include "AFS_component_version_number.c"

int
main(int argc, char **argv)
{
    int nreaders, i;
    PROCESS pid;
    PROCESS *readers;
    int *readerid;
    PROCESS writer;

    printf("\n*Readers & Writers*\n\n");
    setbuf(stdout, 0);

    /* Determine # readers */
    if (argc == 1)
	nreaders = DEFAULT_READERS;
    else
	sscanf(*++argv, "%d", &nreaders);
    printf("[There will be %d readers]\n", nreaders);

    if (argc == 4)
	lwp_debug = 1;
    LWP_InitializeProcessSupport(0, &pid);
    printf("[Support initialized]\n");

    /* Initialize queue */
    q = init();

    /* Initialize lock */
    Lock_Init(&q->lock);

    asleep = 0;
    /* Now create readers */
    printf("[Creating Readers...\n");
    readers = (PROCESS *) calloc(nreaders, sizeof(PROCESS));
    readerid = calloc(nreaders, sizeof(i));
    for (i = 0; i < nreaders; i++) {
	readerid[i] = i;
	LWP_CreateProcess(read_process, STACK_SIZE, 0, (void *)&readerid[i],
			  "Reader", &readers[i]);
    }
    printf("done]\n");

    printf("\t[Creating Writer...\n");
    LWP_CreateProcess(write_process, STACK_SIZE, 1, 0, "Writer", &writer);
    printf("done]\n");

    /* Now loop until everyone's done */
    while (asleep != nreaders + 1)
	LWP_DispatchProcess();
    /* Destroy the readers */
    for (i = nreaders - 1; i >= 0; i--)
	LWP_DestroyProcess(readers[i]);
    LWP_DestroyProcess(writer);

    LWP_TerminateProcessSupport();

    printf("\n*Exiting*\n");

    exit(0);
}
