#ifndef _PROTO_H_
#define _PROTO_H_

#include <common.h>
#include <includes.h>

int nb_unlink(char *fname);
int nb_createx(char *fname, 
		unsigned create_options, unsigned create_disposition, int handle);
int nb_SetLocker(char *Locker);
int nb_Mkdir(char *Directory);
int nb_Xrmdir(char *Directory, char *type);
int nb_Attach(char *Locker, char *Drive);
int nb_Detach(char *Name, char *type);
int nb_CreateFile(char *path, DWORD Size);
int nb_CopyFile(char *source, char *destination);
int nb_DeleteFile(char *fname);
int nb_Move(char *Source, char *Destination);
int nb_xcopy(char *Source, char *Destination);
ssize_t nb_write(int fnum, char *buf, DWORD offset, size_t size);
int nb_writex(int handle, int offset, int size, int ret_size);
ssize_t nb_read(int fnum, char *buf, DWORD offset, size_t size);
int nb_readx(int handle, int offset, int size, int ret_size);
int nb_close(int handle);
BOOL nb_close1(int fnum);
int nb_rmdir(char *fname);
int nb_rename(char *old, char *new);
int nb_qpathinfo(char *fname, int Type);
int nb_qfileinfo(int fnum);
int nb_qfsinfo(int level);
int nb_findfirst(char *mask);
int nb_flush(int fnum);
int nb_deltree(char *dname);
int nb_cleanup(char *cname);
int nb_lock(int handle, int offset, int size, int timeout, unsigned char locktype, NTSTATUS exp);

#endif /*  _PROTO_H_  */
