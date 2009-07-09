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


#include <rx/rx_queue.h>

struct myq {
    struct rx_queue queue_header;
    int value;
};

void
qprint(char *s, struct myq *qe)
{
    printf("%s/%x: next:%x, prev:%x, value=%d\n", s, qe, queue_Next(qe, myq),
	   queue_Prev(qe, myq), qe->value);
}

void
qremove(char *s, struct myq *q)
{
    struct myq *qe, *nqe;
    printf("*head* ");
    qprint(s, q);
    for (queue_Scan(q, qe, nqe, myq)) {
	if (qe->value <= 10)
	    queue_Remove(qe);
	else
	    qprint(s, qe);
    }
}

/* Separate test for the splice macros */
struct rx_queue *
createQueue(int n)
{
    int i;
    struct rx_queue *q;
    struct myq *qe;
    q = (struct rx_queue *)malloc(sizeof(struct rx_queue));
    queue_Init(q);
    for (i = 0; i < 3; i++) {
	qe = (struct myq *)malloc(sizeof(struct myq));
	qe->value = n * 1000 + i;
	queue_Append(q, qe);
    }
    return q;
}

void
testSplice(void)
{
    struct rx_queue *q[10];
    struct myq *qe, *nqe;
    int i;
    for (i = 0; i < 10; i++)
	q[i] = createQueue(i);
    for (i = 0; i < 9; i++) {
	if (i & 1)
	    queue_SplicePrepend(q[0], q[i + 1]);
	else
	    queue_SpliceAppend(q[0], q[i + 1]);
    }
    /* Move the queue to the middle (splice non-empty onto empty) */
    queue_SpliceAppend(q[7], q[0]);
    queue_SplicePrepend(q[6], q[0]);
    /* Splice some empty&non-empty queues onto empty&non-empty queues */
    for (i = 0; i < 9; i++)
	queue_SpliceAppend(q[i], q[i + 1]);
    for (i = 0; i < 9; i++)
	queue_SplicePrepend(q[i], q[i + 1]);
    printf("All queues except 5 should be empty\n");
    for (i = 0; i < 10; i++) {
	printf("Forwards, i=%d:", i);
	for (queue_Scan(q[i], qe, nqe, myq))
	    printf(" %d", qe->value);
	printf("\n");
    }
    for (i = 0; i < 10; i++) {
	printf("Backwards, i=%d:", i);
	for (queue_ScanBackwards(q[i], qe, nqe, myq))
	    printf(" %d", qe->value);
	printf("\n");
    }
}

void
testAppend(void)
{
    int i;
    struct myq x;
    struct myq xa[20];
    struct myq y;
    struct myq ya[20];

    queue_Init(&x);
    x.value = 100001;
    for (i = 0; i < 20; i++)
	queue_Prepend(&x, &xa[i]), xa[i].value = i + 1;
    queue_Init(&y);
    y.value = 100002;
    for (i = 0; i < 20; i++)
	queue_Append(&y, &ya[i]), ya[i].value = i + 1;
    qremove("x, first pass", &x);
    qremove("x, later", &x);
    qremove("y, first pass", &y);
    qremove("y, later", &y);
}

int
main(int argc, char **argv)
{

    if (argc > 1) {
	testSplice();
    } else {
	testAppend();
    }
    exit(0);
}
