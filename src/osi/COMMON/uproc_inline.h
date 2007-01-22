/*
 * Copyright 2006, Sine Nomine Associates and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#ifndef _OSI_COMMON_UPROC_INLINE_H
#define _OSI_COMMON_UPROC_INLINE_H 1

/*
 * osi process userspace api
 * inlines
 */

osi_inline_define(
osi_result
osi_proc_env_set(const char * key,
                 const char * value,
		 int allow_overwrite)
{
    return (setenv(key, value, allow_overwrite)==0) ? OSI_OK : OSI_FAIL;
}
)
osi_inline_prototype(
osi_result
osi_proc_env_set(const char * key,
                 const char * value,
		 int allow_overwrite)
)

osi_inline_define(
osi_result
osi_proc_env_unset(const char * key)
{
    return (unsetenv(key)==0) ? OSI_OK : OSI_FAIL;
}
)
osi_inline_prototype(
osi_result
osi_proc_env_unset(const char * key)
)

osi_inline_define(
osi_result
osi_proc_env_get(const char * key,
                 char ** val_out)
{
    char * value;

    *val_out = value = getenv(key);

    return (value != osi_NULL) ? OSI_OK : OSI_FAIL;
}
)
osi_inline_prototype(
osi_result
osi_proc_env_get(const char * key,
                 char ** val_out)
)

#endif /* _OSI_COMMON_UPROC_INLINE_H */
