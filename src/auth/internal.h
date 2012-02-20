extern int _afsconf_Check(struct afsconf_dir *adir);
extern int _afsconf_Touch(struct afsconf_dir *adir);
extern int _afsconf_IntGetKeys(struct afsconf_dir *adir);
extern int _afsconf_IsClientConfigDirectory(const char *path);
extern int _afsconf_LoadKeys(struct afsconf_dir *adir);
extern void _afsconf_InitKeys(struct afsconf_dir *adir);
extern void _afsconf_FreeAllKeys(struct afsconf_dir *adir);
extern int _afsconf_GetLocalCell(struct afsconf_dir *adir, char **pname, int check);
