#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

__declspec(dllexport) INT GetWebPage(LPSTR lpErrMsg,LPSTR lpFile,LPSTR lpCmdLine);

__declspec(dllexport) INT GetUserLogon(LPSTR lpUserName);

__declspec(dllexport) INT BrowseFile(HWND hwndOwner,LPSTR lpstrTitle,LPSTR lpFileFullName,INT fullsize);

#ifdef __cplusplus
}
#endif

