#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

  typedef BYTE lana_number_t;

#define LANA_INVALID 0xff

  int lana_GetNameFromGuid(char *Guid, char **Name);

  lana_number_t lana_FindLanaByName(const char *LanaName);

  lana_number_t lana_FindLoopback(void);

  BOOL lana_IsLoopback(lana_number_t lana);

#ifdef __cplusplus
}
#endif
