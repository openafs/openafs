
#define ERROR_EXIT(evalue)                                      \
	{                                                       \
            code = evalue;                                      \
            goto error_exit;                                    \
        }

#define ERROR_EXIT2(evalue)                                     \
	{                                                       \
            code = evalue;                                      \
            goto error_exit2;                                   \
        }

#define ABORT_EXIT(evalue)                                      \
	{                                                       \
            code = evalue;                                      \
            goto abort_exit;                                    \
        }
