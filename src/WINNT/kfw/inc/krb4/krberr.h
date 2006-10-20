/*
 * krberr.h
 * This file is the #include file for krberr.et.
 * Please do not edit it as it is automatically generated.
 */

#ifdef WINDOWS
extern void initialize_krb_error_func(err_func func,HANDLE *);
#else
extern void initialize_krb_error_func(err_func func,struct et_list **);
#endif
#define ERROR_TABLE_BASE_krb (39525376L)

/* for compatibility with older versions... */
#define init_krb_err_func(erf) initialize_krb_error_func(erf,&_et_list)
#define krb_err_base ERROR_TABLE_BASE_krb

#ifdef WINDOWS
extern HANDLE _et_list;
#else
extern struct et_list *_et_list;
#endif
