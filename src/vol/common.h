#ifndef AFS_SRC_VOL_COMMON_H
#define AFS_SRC_VOL_COMMON_H

/* common.c */
extern void Log(const char *format, ...)
	AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2);
extern void Abort(const char *format, ...)
	AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2)
	AFS_NORETURN;
extern void Quit(const char *format, ...)
	AFS_ATTRIBUTE_FORMAT(__printf__, 1, 2)
	AFS_NORETURN;

#endif
