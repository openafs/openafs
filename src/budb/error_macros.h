
#define ERROR(evalue)                                           \
	{                                                       \
            code = evalue;                                      \
            goto error_exit;                                    \
        }

#define ABORT(evalue)                                           \
	{                                                       \
            code = evalue;                                      \
            goto abort_exit;                                    \
        }

#define BUDB_EXIT(evalue)                                       \
	{                                                       \
            osi_audit(BUDB_ExitEvent, evalue, AUD_END);         \
	    exit(evalue);                                       \
        }
