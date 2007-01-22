/*
 * Copyright 2006-2007, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <osi/osi.h>
#include <osi/osi_trace.h>
#include <trace/consumer/consumer.h>
#include <trace/consumer/activation.h>

/*
 * osi tracing framework
 * trace consumer process
 */

#include <stdio.h>
#include <errno.h>

void
usage(const char * prog)
{
    fprintf(stderr, "%s: <gen id> <probe name>\n", (prog) ? prog : "disable_probe");
}

int
main(int argc, char ** argv)
{
    osi_result code;
    osi_uint32 gen_id, nhits;
    int ret;

    osi_Assert(OSI_RESULT_OK(osi_PkgInit(osi_ProgramType_TraceCollector, osi_NULL)));
    osi_Assert(OSI_RESULT_OK(osi_Trace_PkgInit()));

    if (argc != 3) {
	usage(argv[0]);
	exit(-1);
    }

    sscanf(argv[1], "%u", &gen_id);
    nhits = 0;
    code = osi_TracePoint_DisableByFilter((osi_trace_gen_id_t)gen_id,
					  argv[2],
					  &nhits);
    if (OSI_RESULT_FAIL(code)) {
	fprintf(stderr, "DisableByFilter failed with code %d (errno %d)\n",
		code, errno);
	ret = -1;
    } else {
	printf("DisableByFilter returned success; nhits=%u\n", nhits);
	ret = 0;
    }

    osi_Assert(OSI_RESULT_OK(osi_Trace_PkgShutdown()));
    osi_Assert(OSI_RESULT_OK(osi_PkgShutdown()));

    return 0;
}
