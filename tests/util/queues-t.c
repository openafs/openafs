#include <afsconfig.h>
#include <afs/param.h>

#include <stdlib.h>

#include <tap/basic.h>

#include <opr/queue.h>


struct charqueue {
    struct opr_queue entry;
    char item;
};


struct charqueue *
newEntry(char string)
{
    struct charqueue *entry;

    entry=malloc(sizeof(struct charqueue));
    entry->item=string;
    return(entry);
}

char *
queueAsString(struct opr_queue *head) {
    static char items[255];
    struct opr_queue *cursor;
    int pos = 0;

    for(opr_queue_Scan(head, cursor)) {
	items[pos] = opr_queue_Entry(cursor, struct charqueue, entry)->item;
	pos++;
	if (pos==254)
	    break;
    }
    items[pos]='\0';

    return items;
}

void
stringIntoQueue(char *string, struct opr_queue *q) {
    int i;

    i = 0;
    while (string[i]!='\0') {
        opr_queue_Append(q, &(newEntry(string[i])->entry));
	i++;
    }
}

int
main(void)
{
    struct opr_queue q1, q2, *cursor;

    plan(12);

    opr_queue_Init(&q1);
    opr_queue_Init(&q2);

    opr_queue_Append(&q1, &(newEntry('A')->entry));
    opr_queue_Append(&q1, &(newEntry('B')->entry));
    opr_queue_Append(&q1, &(newEntry('C')->entry));
    is_string(queueAsString(&q1), "ABC", "Append works as expected");

    opr_queue_Prepend(&q1, &(newEntry('D')->entry));
    opr_queue_Prepend(&q1, &(newEntry('E')->entry));
    is_string(queueAsString(&q1), "EDABC", "Prepend works");

    opr_queue_Append(&q2, &(newEntry('1')->entry));
    opr_queue_Append(&q2, &(newEntry('2')->entry));

    opr_queue_SpliceAppend(&q1, &q2);
    is_string(queueAsString(&q1), "EDABC12", "SpliceAppend works");
    ok(opr_queue_IsEmpty(&q2),
	   "IsEmpty works (and splice empties queues)");

    opr_queue_Append(&q2, &(newEntry('8')->entry));
    opr_queue_Append(&q2, &(newEntry('9')->entry));
    is_string(queueAsString(&q2), "89", "Append works again");

    opr_queue_SplicePrepend(&q1, &q2);
    is_string(queueAsString(&q1), "89EDABC12", "SplicePrepend works");

    /* Now for some trickier stuff */
    stringIntoQueue("XYZ", &q2);

    /* Find the A in the string above */
    for (opr_queue_Scan(&q1, cursor)) {
	if (opr_queue_Entry(cursor, struct charqueue, entry)->item == 'A')
	    break;
    }

    opr_queue_InsertBefore(cursor, &(newEntry('M')->entry));
    is_string("89EDMABC12", queueAsString(&q1),
	      "InsertBefore functions correctly");
    opr_queue_SplitBeforeAppend(&q1, &q2, cursor);
    is_string("ABC12", queueAsString(&q1),
	      "SplitBefore leaves old queue in correct state");
    is_string("XYZ89EDM", queueAsString(&q2),
	      "SplitBefore correctly appends to new queue");

    /* Find the 9 in q2 */
    for (opr_queue_Scan(&q2, cursor)) {
	if (opr_queue_Entry(cursor, struct charqueue, entry)->item == '9')
	    break;
    }
    opr_queue_InsertAfter(cursor, &(newEntry('N')->entry));
    is_string("XYZ89NEDM", queueAsString(&q2), "InsertAfter");

    opr_queue_SplitAfterPrepend(&q2, &q1, cursor);
    is_string("NEDMABC12", queueAsString(&q1), "SplitAfterPrepend Q1");
    is_string("XYZ89", queueAsString(&q2), "SplitAfterPrepend Q2");

    return 0;
}


