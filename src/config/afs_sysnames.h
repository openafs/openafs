/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 *
 * Portions Copyright (c) 2003 Apple Computer, Inc.
 */

/*
 * File: afs_sysnames.h
 *
 * Macros defining AFS systypes and their associated IDs, and generating
 * them from the value of SYS_NAME.
 *
 * Groups of 100 IDs have been allocated to each major system type.
 */

#ifndef __AFS_SYSNAMES_INCL_ENV_
#define __AFS_SYSNAMES_INCL_ENV_ 1

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
#define SYS_NAME_ID_hp_ux11i             416
#define SYS_NAME_ID_ia64_hpux1122	 417
#define SYS_NAME_ID_ia64_hpux1123	 418
#define SYS_NAME_ID_hp_ux1123		 419

#define SYS_NAME_ID_mac2_51		 500
#define SYS_NAME_ID_mac_aux10		 501
#define SYS_NAME_ID_mac_mach51		 502
#define SYS_NAME_ID_ppc_darwin_12        503
#define SYS_NAME_ID_ppc_darwin_13        504
#define SYS_NAME_ID_ppc_darwin_14        505
#define SYS_NAME_ID_ppc_darwin_60        506
#define SYS_NAME_ID_ppc_darwin_70        507
#define SYS_NAME_ID_ppc_darwin_80        508
#define SYS_NAME_ID_x86_darwin_80        509
#define SYS_NAME_ID_ppc_darwin_90        510
#define SYS_NAME_ID_x86_darwin_90        511
#define SYS_NAME_ID_ppc_darwin_100       512
#define SYS_NAME_ID_ppc64_darwin_100     513
#define SYS_NAME_ID_x86_darwin_100       514
#define SYS_NAME_ID_amd64_darwin_100     515
#define SYS_NAME_ID_arm_darwin_100       516
#define SYS_NAME_ID_x86_darwin_110       517
#define SYS_NAME_ID_amd64_darwin_110     518
#define SYS_NAME_ID_arm_darwin_110       519
#define SYS_NAME_ID_x86_darwin_120       520
#define SYS_NAME_ID_amd64_darwin_120     521
#define SYS_NAME_ID_arm_darwin_120       522
#define SYS_NAME_ID_x86_darwin_130       523
#define SYS_NAME_ID_amd64_darwin_130     524
#define SYS_NAME_ID_arm_darwin_130       525
#define SYS_NAME_ID_x86_darwin_140       526
#define SYS_NAME_ID_amd64_darwin_140     527
#define SYS_NAME_ID_arm_darwin_140       528
#define SYS_NAME_ID_x86_darwin_150       529
#define SYS_NAME_ID_amd64_darwin_150     530
#define SYS_NAME_ID_arm_darwin_150       531
#define SYS_NAME_ID_x86_darwin_160       532
#define SYS_NAME_ID_amd64_darwin_160     533
#define SYS_NAME_ID_arm_darwin_160       534
#define SYS_NAME_ID_x86_darwin_170       535
#define SYS_NAME_ID_amd64_darwin_170     536
#define SYS_NAME_ID_arm_darwin_170       537
#define SYS_NAME_ID_x86_darwin_180       538
#define SYS_NAME_ID_amd64_darwin_180     539
#define SYS_NAME_ID_arm_darwin_180       540
#define SYS_NAME_ID_x86_darwin_190       541
#define SYS_NAME_ID_amd64_darwin_190     542
#define SYS_NAME_ID_arm_darwin_190       543
#define SYS_NAME_ID_x86_darwin_200       544
#define SYS_NAME_ID_amd64_darwin_200     545
#define SYS_NAME_ID_arm_darwin_200       546
#define SYS_NAME_ID_amd64_darwin_210     547
#define SYS_NAME_ID_arm_darwin_210       548
#define SYS_NAME_ID_amd64_darwin_220     549
#define SYS_NAME_ID_arm_darwin_220       550
#define SYS_NAME_ID_amd64_darwin_230     551
#define SYS_NAME_ID_arm_darwin_230       552
#define SYS_NAME_ID_amd64_darwin_240     553
#define SYS_NAME_ID_arm_darwin_240       554

#define SYS_NAME_ID_next_mach20		 601
#define SYS_NAME_ID_next_mach30		 602

#define SYS_NAME_ID_rs_aix32		 701
#define SYS_NAME_ID_rs_aix41		 702
#define SYS_NAME_ID_rs_aix42		 703
#define SYS_NAME_ID_rs_aix51		 704
#define SYS_NAME_ID_rs_aix43		 705
#define SYS_NAME_ID_rs_aix52		 706
#define SYS_NAME_ID_rs_aix53		 707
#define SYS_NAME_ID_rs_aix61		 708
#define SYS_NAME_ID_rs_aix72		 709
#define SYS_NAME_ID_rs_aix71		 710
#define SYS_NAME_ID_rs_aix73		 711

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
#define SYS_NAME_ID_sun4x_58		 939
#define SYS_NAME_ID_sun4x_59		 940
#define SYS_NAME_ID_sun4x_510		 941
#define SYS_NAME_ID_sun4x_511		 942

/* Sigh. If I leave a gap here IBM will do this sequentially. If I don't
   they won't allocate sunx86 IDs at all. So leave a gap and pray. */
#define SYS_NAME_ID_sunx86_57		 950
#define SYS_NAME_ID_sunx86_58		 951
#define SYS_NAME_ID_sunx86_59		 952
#define SYS_NAME_ID_sunx86_510		 953
#define SYS_NAME_ID_sunx86_511		 954

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
#define SYS_NAME_ID_alpha_dux50		1206
#define SYS_NAME_ID_alpha_dux51		1207

#define SYS_NAME_ID_ncrx86_20		1300
#define SYS_NAME_ID_ncrx86_30		1301

#define SYS_NAME_ID_i386_nt35		1400
#define SYS_NAME_ID_i386_win95	        1401
#define SYS_NAME_ID_i386_w2k            1402

#define SYS_NAME_ID_i386_linux2		1500
#define SYS_NAME_ID_i386_linux22	1501
#define SYS_NAME_ID_i386_linux24        1502
#define SYS_NAME_ID_i386_linux26        1503

#define SYS_NAME_ID_ppc_linux2          1600
#define SYS_NAME_ID_ppc_linux22         1601
#define SYS_NAME_ID_ppc_linux24         1602
#define SYS_NAME_ID_ppc_linux26         1603

#define SYS_NAME_ID_sparc_linux2	1700
#define SYS_NAME_ID_sparc_linux22	1701
#define SYS_NAME_ID_sparc_linux24	1702
#define SYS_NAME_ID_sparc_linux26	1703

#define SYS_NAME_ID_sparc64_linux2	1800
#define SYS_NAME_ID_sparc64_linux22	1801
#define SYS_NAME_ID_sparc64_linux24	1802
#define SYS_NAME_ID_sparc64_linux26	1803

#define SYS_NAME_ID_s390_linux2         1900
#define SYS_NAME_ID_s390_linux22        1901
#define SYS_NAME_ID_s390_linux24        1902
#define SYS_NAME_ID_s390_linux26        1907
#define SYS_NAME_ID_s390x_linux2         1903
#define SYS_NAME_ID_s390x_linux22        1904
#define SYS_NAME_ID_s390x_linux24        1905
#define SYS_NAME_ID_s390x_linux26        1906

#define SYS_NAME_ID_alpha_linux_2       2000
#define SYS_NAME_ID_alpha_linux_22      2001
#define SYS_NAME_ID_alpha_linux_24      2002
#define SYS_NAME_ID_alpha_linux_26      2003

#define SYS_NAME_ID_i386_fbsd_42        2100
#define SYS_NAME_ID_i386_fbsd_43        2101
#define SYS_NAME_ID_i386_fbsd_44        2102
#define SYS_NAME_ID_i386_fbsd_45        2103
#define SYS_NAME_ID_i386_fbsd_46        2103
#define SYS_NAME_ID_i386_fbsd_47        2104
#define SYS_NAME_ID_i386_fbsd_50        2105
#define SYS_NAME_ID_i386_fbsd_51        2106
#define SYS_NAME_ID_i386_fbsd_52        2107
#define SYS_NAME_ID_i386_fbsd_53        2108
#define SYS_NAME_ID_i386_fbsd_60        2112
#define SYS_NAME_ID_i386_fbsd_61        2113
#define SYS_NAME_ID_i386_fbsd_62        2114
#define SYS_NAME_ID_i386_fbsd_70        2115
#define SYS_NAME_ID_i386_fbsd_71        2116
#define SYS_NAME_ID_i386_fbsd_72        2130
#define SYS_NAME_ID_i386_fbsd_73        2131
#define SYS_NAME_ID_i386_fbsd_74        2132
#define SYS_NAME_ID_i386_fbsd_80        2117
#define SYS_NAME_ID_i386_fbsd_81        2118
#define SYS_NAME_ID_i386_fbsd_82        2119
#define SYS_NAME_ID_i386_fbsd_83        2121
#define SYS_NAME_ID_i386_fbsd_84        2123
#define SYS_NAME_ID_i386_fbsd_90        2120
#define SYS_NAME_ID_i386_fbsd_91        2122
#define SYS_NAME_ID_i386_fbsd_92        2124
#define SYS_NAME_ID_i386_fbsd_93        2126
#define SYS_NAME_ID_i386_fbsd_100      2150
#define SYS_NAME_ID_i386_fbsd_101      2151
#define SYS_NAME_ID_i386_fbsd_102      2152
#define SYS_NAME_ID_i386_fbsd_103      2153
#define SYS_NAME_ID_i386_fbsd_104      2154
#define SYS_NAME_ID_i386_fbsd_110      2140
#define SYS_NAME_ID_i386_fbsd_111      2141
#define SYS_NAME_ID_i386_fbsd_112      2142
#define SYS_NAME_ID_i386_fbsd_113      2143
#define SYS_NAME_ID_i386_fbsd_120      2160
#define SYS_NAME_ID_i386_fbsd_121      2161
#define SYS_NAME_ID_i386_fbsd_122      2162
#define SYS_NAME_ID_i386_fbsd_123      2163

#define SYS_NAME_ID_ia64_linux2		2200
#define SYS_NAME_ID_ia64_linux22	2201
#define SYS_NAME_ID_ia64_linux24	2202
#define SYS_NAME_ID_ia64_linux26	2203

#define SYS_NAME_ID_m68k_linux22        2301
#define SYS_NAME_ID_m68k_linux24        2302

#define SYS_NAME_ID_parisc_linux22      2401
#define SYS_NAME_ID_parisc_linux24      2402

#define SYS_NAME_ID_i386_nbsd15         2501
#define SYS_NAME_ID_alpha_nbsd15        2502
#define SYS_NAME_ID_sparc_nbsd15        2503
#define SYS_NAME_ID_sparc64_nbsd15      2504

#define SYS_NAME_ID_i386_nbsd16         2510
#define SYS_NAME_ID_alpha_nbsd16        2511
#define SYS_NAME_ID_sparc_nbsd16        2512
#define SYS_NAME_ID_sparc64_nbsd16      2513
#define SYS_NAME_ID_macppc_nbsd16       2514

#define SYS_NAME_ID_i386_nbsd20         2520
#define SYS_NAME_ID_alpha_nbsd20        2521
#define SYS_NAME_ID_sparc_nbsd20        2522
#define SYS_NAME_ID_sparc64_nbsd20      2523
#define SYS_NAME_ID_macppc_nbsd20       2524
#define SYS_NAME_ID_i386_nbsd21         2525
#define SYS_NAME_ID_i386_nbsd30         2526
#define SYS_NAME_ID_amd64_nbsd20        2527
#define SYS_NAME_ID_i386_nbsd40         2528
#define SYS_NAME_ID_i386_nbsd50         2529
#define SYS_NAME_ID_sparc64_nbsd30      2530
#define SYS_NAME_ID_sparc64_nbsd40      2531
#define SYS_NAME_ID_sparc64_nbsd50      2532
#define SYS_NAME_ID_amd64_nbsd30        2533
#define SYS_NAME_ID_amd64_nbsd40        2534
#define SYS_NAME_ID_amd64_nbsd50        2535
#define SYS_NAME_ID_alpha_nbsd30        2536
#define SYS_NAME_ID_alpha_nbsd40        2537
#define SYS_NAME_ID_alpha_nbsd50        2538
#define SYS_NAME_ID_macppc_nbsd30       2539
#define SYS_NAME_ID_macppc_nbsd40       2540
#define SYS_NAME_ID_macppc_nbsd50       2541
#define SYS_NAME_ID_amd64_nbsd60        2542
#define SYS_NAME_ID_i386_nbsd60         2543
#define SYS_NAME_ID_amd64_nbsd70        2544
#define SYS_NAME_ID_i386_nbsd70         2545

#define SYS_NAME_ID_i386_obsd31		2600
#define SYS_NAME_ID_i386_obsd32		2601
#define SYS_NAME_ID_i386_obsd33		2602
#define SYS_NAME_ID_i386_obsd34		2603
#define SYS_NAME_ID_i386_obsd35		2604
#define SYS_NAME_ID_i386_obsd36		2605
#define SYS_NAME_ID_i386_obsd37		2606
#define SYS_NAME_ID_i386_obsd38		2607
#define SYS_NAME_ID_i386_obsd39		2608
#define SYS_NAME_ID_i386_obsd40         2609
#define SYS_NAME_ID_i386_obsd41         2610
#define SYS_NAME_ID_i386_obsd42         2611
#define SYS_NAME_ID_i386_obsd43         2612
#define SYS_NAME_ID_i386_obsd44         2613
#define SYS_NAME_ID_i386_obsd45         2614
#define SYS_NAME_ID_i386_obsd46         2615
#define SYS_NAME_ID_i386_obsd47         2616
#define SYS_NAME_ID_i386_obsd48         2617
#define SYS_NAME_ID_i386_obsd49         2618
#define SYS_NAME_ID_i386_obsd50         2619
#define SYS_NAME_ID_i386_obsd51         2620
#define SYS_NAME_ID_i386_obsd52         2621
#define SYS_NAME_ID_i386_obsd53         2622
#define SYS_NAME_ID_i386_obsd54         2623

#define SYS_NAME_ID_amd64_linux2        2700
#define SYS_NAME_ID_amd64_linux22       2701
#define SYS_NAME_ID_amd64_linux24       2702
#define SYS_NAME_ID_amd64_linux26       2703

#define SYS_NAME_ID_i386_umlinux2	2800
#define SYS_NAME_ID_i386_umlinux22	2801
#define SYS_NAME_ID_i386_umlinux24	2802
#define SYS_NAME_ID_i386_umlinux26	2803

#define SYS_NAME_ID_ppc64_linux2	2900
#define SYS_NAME_ID_ppc64_linux22	2901
#define SYS_NAME_ID_ppc64_linux24	2902
#define SYS_NAME_ID_ppc64_linux26	2903

#define SYS_NAME_ID_amd64_fbsd_53       3008
#define SYS_NAME_ID_amd64_fbsd_70       3009
#define SYS_NAME_ID_amd64_fbsd_71       3010
#define SYS_NAME_ID_amd64_fbsd_72       3030
#define SYS_NAME_ID_amd64_fbsd_73       3031
#define SYS_NAME_ID_amd64_fbsd_74       3032
#define SYS_NAME_ID_amd64_fbsd_80       3011
#define SYS_NAME_ID_amd64_fbsd_81       3012
#define SYS_NAME_ID_amd64_fbsd_82       3013
#define SYS_NAME_ID_amd64_fbsd_83       3014
#define SYS_NAME_ID_amd64_fbsd_84       3015
#define SYS_NAME_ID_amd64_fbsd_90       3020
#define SYS_NAME_ID_amd64_fbsd_91       3022
#define SYS_NAME_ID_amd64_fbsd_92       3023
#define SYS_NAME_ID_amd64_fbsd_93       3024
#define SYS_NAME_ID_amd64_fbsd_100     3050
#define SYS_NAME_ID_amd64_fbsd_101     3051
#define SYS_NAME_ID_amd64_fbsd_102     3052
#define SYS_NAME_ID_amd64_fbsd_103     3053
#define SYS_NAME_ID_amd64_fbsd_104     3054
#define SYS_NAME_ID_amd64_fbsd_110     3040
#define SYS_NAME_ID_amd64_fbsd_111     3041
#define SYS_NAME_ID_amd64_fbsd_112     3042
#define SYS_NAME_ID_amd64_fbsd_113     3043
#define SYS_NAME_ID_amd64_fbsd_120     3060
#define SYS_NAME_ID_amd64_fbsd_121     3061
#define SYS_NAME_ID_amd64_fbsd_122     3062
#define SYS_NAME_ID_amd64_fbsd_123     3063

#define SYS_NAME_ID_amd64_w2k           3400

#define SYS_NAME_ID_i64_w2k             3500

#define SYS_NAME_ID_arm_linux2          3800
#define SYS_NAME_ID_arm_linux22         3801
#define SYS_NAME_ID_arm_linux24         3802
#define SYS_NAME_ID_arm_linux26         3803

#define SYS_NAME_ID_i386_dfbsd_22        3900
#define SYS_NAME_ID_i386_dfbsd_23        3901
#define SYS_NAME_ID_i386_dfbsd_24        3902
#define SYS_NAME_ID_i386_dfbsd_25        3903
#define SYS_NAME_ID_i386_dfbsd_26        3904
#define SYS_NAME_ID_i386_dfbsd_27        3905

#define SYS_NAME_ID_amd64_obsd36        4005
#define SYS_NAME_ID_amd64_obsd37        4006
#define SYS_NAME_ID_amd64_obsd38        4007
#define SYS_NAME_ID_amd64_obsd39        4008
#define SYS_NAME_ID_amd64_obsd40        4009
#define SYS_NAME_ID_amd64_obsd41        4010
#define SYS_NAME_ID_amd64_obsd42        4011
#define SYS_NAME_ID_amd64_obsd43        4012
#define SYS_NAME_ID_amd64_obsd44        4013
#define SYS_NAME_ID_amd64_obsd45        4014
#define SYS_NAME_ID_amd64_obsd46        4015
#define SYS_NAME_ID_amd64_obsd47        4016
#define SYS_NAME_ID_amd64_obsd48        4017
#define SYS_NAME_ID_amd64_obsd49        4018
#define SYS_NAME_ID_amd64_obsd50        4019
#define SYS_NAME_ID_amd64_obsd51        4020
#define SYS_NAME_ID_amd64_obsd52        4021
#define SYS_NAME_ID_amd64_obsd53        4022
#define SYS_NAME_ID_amd64_obsd54        4023

#define SYS_NAME_ID_arm64_linux2	4100
#define SYS_NAME_ID_arm64_linux26	4103

#define SYS_NAME_ID_ppc64le_linux26	4203

#define	AFS_REALM_SZ	64

/* Specifies the number of equivalent local realm names */
#define AFS_NUM_LREALMS 4

#endif /* __AFS_SYSNAMES_INCL_ENV_ */
