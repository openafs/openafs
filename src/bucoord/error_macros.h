
#undef ERROR
#define ERROR(evalue)                                           \
	{                                                       \
            code = evalue;                                      \
            goto error_exit;                                    \
        }

#undef ABORT
#define ABORT(evalue)                                           \
	{                                                       \
            code = evalue;                                      \
            goto abort_exit;                                    \
        }

