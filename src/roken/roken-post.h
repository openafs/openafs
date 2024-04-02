/*
 * roken defines an empty __attribute__() macro when the format
 * attribute is not available.  Undefine this empty __attribute__() macro
 * to to allow other types of attributes in code which includes roken.h.
 */
#ifdef __attribute__
#undef __attribute__
#endif
#endif /* OPENAFS_ROKEN_H */
