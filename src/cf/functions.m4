AC_DEFUN([OPENAFS_FUNCTION_CHECKS],[
AC_CHECK_FUNCS([ \
	daemon \
	flock \
	fseeko64 \
	ftello64 \
	getcwd \
	getprogname \
	getrlimit \
	mkstemp \
	poll \
	pread \
	preadv \
	preadv64 \
	pwrite \
	pwritev \
	pwritev64 \
	regcomp \
	regerror \
	regexec \
	setenv \
	setprogname \
	setvbuf \
	sigaction \
	snprintf \
	strcasestr \
	strerror \
	strlcat \
	strlcpy \
	strnlen \
	timegm \
	tsearch \
	unsetenv \
	vsnprintf \
	vsyslog \
])
])
