#ifndef AFS_PAM_MSG_H
#define AFS_PAM_MSG_H


int pam_afs_printf(struct pam_conv *pam_convp, int error, int fmt_msgid, ...);

int pam_afs_prompt(struct pam_conv *pam_convp, char **response,
		   int echo, int fmt_msgid, ...);


#endif
