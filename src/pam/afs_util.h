#ifndef PAM_AFS_UTIL_H
#define PAM_AFS_UTIL_H


extern	char	*pam_afs_ident;
extern	char	*pam_afs_lh;


void lc_cleanup(
	pam_handle_t	*pamh,
	void		*data,
	int		pam_end_status);

void nil_cleanup(
	pam_handle_t	*pamh,
	void		*data,
	int		pam_end_status);

extern char*	cv2string();

#if	defined(AFS_HPUX_ENV)

#if !defined(AFS_HPUX110_ENV)
#define	PAM_NEW_AUTHTOK_REQD	PAM_AUTHTOKEN_REQD
#endif /* ! AFS_HPUX110_ENV */
#define vsyslog(a,b,c)		syslog(a,b,c)
#define pam_get_user(a,b,c)	pam_get_item(a, PAM_USER, (void **)b)
#define pam_putenv(a,b)		!PAM_SUCCESS

#endif	/* AFS_HPUX_ENV */

#endif
