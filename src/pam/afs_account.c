#include <security/pam_appl.h>
#include <security/pam_modules.h>

extern int
pam_sm_acct_mgmt(
	pam_handle_t	*pamh,
	int		flags,
	int		argc,
	const char	**argv)
{
    return PAM_SUCCESS;
}
