#ifndef __AFS_SYSNAMES_INCL_ENV_
#define __AFS_SYSNAMES_INCL_ENV_ 1

/*
 * (C) Copyright Transarc Corporation 1998, 1992
 * Licensed Materials - Property of Transarc
 * All Rights Reserved.
 *
 * File: afs_sysnames.h
 *
 * Macros defining AFS systypes and their associated IDs, and generating
 * them from the value of SYS_NAME.
 *
 * Groups of 100 IDs have been allocated to each major system type.
 */
#ifndef	IGNORE_STDS_H
#include "stds.h"
#endif

#define	SYS_NAME_ID_UNDEFINED		   0

	
#define SYS_NAME_ID_aux_10		 200

#define SYS_NAME_ID_pmax_ul4		 305
#define SYS_NAME_ID_pmax_ul42		 307
#define SYS_NAME_ID_pmax_ul42a		 308
#define SYS_NAME_ID_pmax_ul43		 309
#define SYS_NAME_ID_pmax_ul43a		 310

#define SYS_NAME_ID_hp700_ux90		 407
#define SYS_NAME_ID_hp300_ux90		 408
#define SYS_NAME_ID_hp800_ux90		 409
#define SYS_NAME_ID_hp700_ux100	 	 410
#define SYS_NAME_ID_hp800_ux100	 	 411
#define SYS_NAME_ID_hp700_ux101	 	 412
#define SYS_NAME_ID_hp800_ux101	 	 413
#define SYS_NAME_ID_hp_ux102	 	 414
#define SYS_NAME_ID_hp_ux110	 	 415

#define SYS_NAME_ID_mac2_51		 500	
#define SYS_NAME_ID_mac_aux10		 501
#define SYS_NAME_ID_mac_mach51		 502

#define SYS_NAME_ID_next_mach20		 601
#define SYS_NAME_ID_next_mach30		 602

#define SYS_NAME_ID_rs_aix32		 701
#define SYS_NAME_ID_rs_aix41		 702
#define SYS_NAME_ID_rs_aix42		 703

#define SYS_NAME_ID_sun3_411		 906
#define SYS_NAME_ID_sun3x_411		 912
#define SYS_NAME_ID_sun4_411		 917
#define SYS_NAME_ID_sun4c_411		 919
#define SYS_NAME_ID_sun4c_51		 920
#define SYS_NAME_ID_sun4m_51		 921
#define SYS_NAME_ID_sun4c_52		 923
#define SYS_NAME_ID_sun4m_52		 924
/* For sun4_52 */
#define SYS_NAME_ID_sun4c_53		 926
#define SYS_NAME_ID_sun4m_53		 927
#define SYS_NAME_ID_sun4_52		 928
#define SYS_NAME_ID_sun4_53		 929
#define SYS_NAME_ID_sun4_54		 930
#define SYS_NAME_ID_sun4m_54		 931
#define SYS_NAME_ID_sun4c_54		 932
#define SYS_NAME_ID_sunx86_54		 933
#define SYS_NAME_ID_sun4x_55           	 934
#define SYS_NAME_ID_sun4x_56           	 935
#define SYS_NAME_ID_sunx86_56          	 936
#define SYS_NAME_ID_sunx86_55          	 937
#define SYS_NAME_ID_sun4x_57		 938


#define SYS_NAME_ID_vax_ul4		1003
#define SYS_NAME_ID_vax_ul40		1004
#define SYS_NAME_ID_vax_ul43		1005

#define SYS_NAME_ID_sgi_50		1100
#define SYS_NAME_ID_sgi_51		1101
#define	SYS_NAME_ID_sgi_52		1102
#define	SYS_NAME_ID_sgi_53		1103
#define	SYS_NAME_ID_sgi_61		1104
#define	SYS_NAME_ID_sgi_62		1105
#define	SYS_NAME_ID_sgi_63		1106
#define	SYS_NAME_ID_sgi_64		1107
#define	SYS_NAME_ID_sgi_65		1108

#define SYS_NAME_ID_alpha_osf1		1200
#define SYS_NAME_ID_alpha_osf20		1201
#define SYS_NAME_ID_alpha_osf30		1202
#define SYS_NAME_ID_alpha_osf32		1203
#define SYS_NAME_ID_alpha_osf32c	1204
#define SYS_NAME_ID_alpha_dux40		1205

#define SYS_NAME_ID_ncrx86_20		1300
#define SYS_NAME_ID_ncrx86_30		1301

#define SYS_NAME_ID_i386_nt35		1400

#define SYS_NAME_ID_i386_linux2		1500
#define SYS_NAME_ID_i386_linux22	1501

/*
 * Placeholder to keep system-wide standard flags since this file is included by all 
 * files (i.e in afs/param.h)
 */
#ifdef	notdef	
/* Should be enabled by src sites that are compiling afs in a kerberos environment
 * (i.e. use their headers and libs) and want to use the realm-related kerberos code
 */
#define	AFS_ATHENA_STDENV		1   
#endif
#ifdef	AFS_ATHENA_STDENV
/* So that they don't get our own version of the kerb-related function  in libutil.a */
#define	afs_krb_get_lrealm(r,n)	krb_get_lrealm(r,n)
#define	AFS_REALM_SZ		REALM_SZ
#endif

/* Should be enabled by src sites that are compiling afs without the kerb headers/libs
 * but want to use the kerberos realm-related code
 */
#define	AFS_KERBREALM_ENV	1   
#ifdef	AFS_KERBREALM_ENV
#define	AFS_REALM_SZ		64
#endif
#endif /* __AFS_SYSNAMES_INCL_ENV_ */
