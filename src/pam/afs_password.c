#include <security/pam_appl.h>
#include <security/pam_modules.h>

extern int
pam_sm_chauthtok(
	pam_handle_t	*pamh,
	int		flags,
	int		argc,
	const char	**argv)
{
    return PAM_PERM_DENIED;
}
