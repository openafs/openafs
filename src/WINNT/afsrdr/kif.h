/* copyright (c) 2005
 * the regents of the university of michigan
 * all rights reserved
 * 
 * permission is granted to use, copy, create derivative works and
 * redistribute this software and such derivative works for any purpose,
 * so long as the name of the university of michigan is not used in
 * any advertising or publicity pertaining to the use or distribution
 * of this software without specific, written prior authorization.  if
 * the above copyright notice or any other identification of the
 * university of michigan is included in any copy of any portion of
 * this software, then the disclaimer below must also be included.
 * 
 * this software is provided as is, without representation from the
 * university of michigan as to its fitness for any purpose, and without
 * warranty by the university of michigan of any kind, either express 
 * or implied, including without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  the regents
 * of the university of michigan shall not be liable for any damages,   
 * including special, indirect, incidental, or consequential damages, 
 * with respect to any claim arising out or in connection with the use
 * of the software, even if it has been or is hereafter advised of the
 * possibility of such damages.
 */

/* versioning history
 * 
 * 03-jun 2005 (eric williams) entered into versioning
 */


/* this is based on BUF_FILEHASH, but we were not getting unique hashes */
#define FID_HASH_FN(fidp)		((((fidp)->vnode + \
					   ((fidp)->unique << 13) + ((fidp)->unique >> (32-13))	+ \
					   (fidp)->volume + \
					   (fidp)->cell)))


/* dirent information */
struct readdir_data
	{
	LARGE_INTEGER cookie;
	long offset;
	LARGE_INTEGER creation, access, write, change, size;
	ULONG attribs, name_length;			/* chars */
	CCHAR short_name_length;			/* chars */
	WCHAR short_name[14];
	WCHAR name[];
	};
typedef struct readdir_data readdir_data_t;


/* error codes */
#define IFSL_SUCCESS_BASE			0x00000000
#define IFSL_FAIL_BASE				0x80000000

#define IFSL_SUCCESS				(IFSL_SUCCESS_BASE + 0)
#define IFSL_DOES_NOT_EXIST			(IFSL_FAIL_BASE + 1)
#define IFSL_NOT_IMPLEMENTED		(IFSL_FAIL_BASE + 2)
#define IFSL_END_OF_ENUM			(IFSL_SUCCESS_BASE + 3)
#define IFSL_CANNOT_MAKE			(IFSL_FAIL_BASE + 4)
#define IFSL_END_OF_FILE			(IFSL_SUCCESS_BASE + 5)
#define IFSL_NO_ACCESS				(IFSL_FAIL_BASE + 6)
#define IFSL_BUFFER_TOO_SMALL		(IFSL_FAIL_BASE + 7)
#define IFSL_SHARING_VIOLATION		(IFSL_FAIL_BASE + 8)
#define IFSL_BAD_INPUT				(IFSL_FAIL_BASE + 9)
#define IFSL_GENERIC_FAILURE		(IFSL_FAIL_BASE + 10)
#define IFSL_OPEN_CREATED			(IFSL_SUCCESS_BASE + 11)
#define IFSL_OPEN_EXISTS			(IFSL_FAIL_BASE + 12)
#define IFSL_OPEN_OPENED			(IFSL_SUCCESS_BASE + 13)
#define IFSL_OPEN_OVERWRITTEN		(IFSL_SUCCESS_BASE + 14)
#define IFSL_OPEN_SUPERSCEDED		(IFSL_SUCCESS_BASE + 15)
#define IFSL_BADFILENAME			(IFSL_FAIL_BASE + 16)
#define IFSL_READONLY				(IFSL_FAIL_BASE + 17)
#define IFSL_IS_A_DIR				(IFSL_FAIL_BASE + 18)
#define IFSL_PATH_DOES_NOT_EXIST	(IFSL_FAIL_BASE + 19)
#define IFSL_IS_A_FILE				(IFSL_FAIL_BASE + 20)
#define IFSL_NO_FILE				(IFSL_FAIL_BASE + 21)
#define IFSL_NOT_EMPTY				(IFSL_FAIL_BASE + 22)
#define IFSL_RPC_TIMEOUT			(IFSL_FAIL_BASE + 23)
#define IFSL_OVERQUOTA				(IFSL_FAIL_BASE + 24)
#define IFSL_MEMORY					(IFSL_FAIL_BASE + 25)
#define IFSL_UNSPEC					(IFSL_FAIL_BASE + 26)


/* ioctl codes */
#define IOCTL_AFSRDR_IOCTL		CTL_CODE(IOCTL_DISK_BASE, 0x007, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)
#define IOCTL_AFSRDR_DOWNCALL	CTL_CODE(IOCTL_DISK_BASE, 0x008, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)
#define IOCTL_AFSRDR_GET_PATH	CTL_CODE(IOCTL_DISK_BASE, 0x009, METHOD_OUT_DIRECT, FILE_ANY_ACCESS)


/* upcalls */
long uc_namei(WCHAR *name, ULONG *fid);
long uc_check_access(ULONG fid, ULONG access, ULONG *granted);
long uc_create(WCHAR *str, ULONG attribs, LARGE_INTEGER alloc, ULONG access, ULONG *granted, ULONG *fid);
long uc_stat(ULONG fid, ULONG *attribs, LARGE_INTEGER *size, LARGE_INTEGER *creation, LARGE_INTEGER *access, LARGE_INTEGER *change, LARGE_INTEGER *written);
long uc_setinfo(ULONG fid, ULONG attribs, LARGE_INTEGER creation, LARGE_INTEGER access, LARGE_INTEGER change, LARGE_INTEGER written);
long uc_trunc(ULONG fid, LARGE_INTEGER size);
long uc_read(ULONG fid, LARGE_INTEGER offset, ULONG length, ULONG *read, char *data);
long uc_write(ULONG fid, LARGE_INTEGER offset, ULONG length, ULONG *written, char *data);
long uc_readdir(ULONG fid, LARGE_INTEGER cookie_in, WCHAR *filter, ULONG *count, char *data, ULONG_PTR *len);
long uc_close(ULONG fid);
long uc_unlink(WCHAR *name);
long uc_ioctl_write(ULONG length, char *data, ULONG_PTR *key);
long uc_ioctl_read(ULONG_PTR key, ULONG *length, char *data);
long uc_rename(ULONG fid, WCHAR *curr, WCHAR *new_dir, WCHAR *new_name, ULONG *new_fid);
long uc_flush(ULONG fid);


/* downcalls */
long dc_break_callback(ULONG fid);
long dc_release_hooks(void);
