#include <security/pam_appl.h>
#include <security/pam_modules.h>

extern int
pam_sm_open_session(
	pam_handle_t	*pamh,
	int		flags,
	int		argc,
	const char	**argv)
{
    return PAM_SUCCESS;
}


extern int
pam_sm_close_session(
	pam_handle_t	*pamh,
	int		flags,
	int		argc,
	const char	**argv)
{
    return PAM_SUCCESS;
}
