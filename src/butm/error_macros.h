
#define ERROR_EXIT(evalue)                                      \
	{                                                       \
            code = evalue;                                      \
            goto error_exit;                                    \
        }

#define ABORT_EXIT(evalue)                                      \
	{                                                       \
            code = evalue;                                      \
            goto abort_exit;                                    \
        }

