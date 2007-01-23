#include <stdlib.h>

void *osi_Alloc(n)
{
	void *p = malloc(n+100);
	if (p) p += 100;
	return p;
}

void afs_osi_Free(void *p, int n)
{
	free(p-100);
}
