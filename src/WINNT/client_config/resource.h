/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#define IDS_TITLE_NT                    0
#define IDS_TITLE_95                    1 
#define IDS_STATE_STOPPED               2 
#define IDS_STATE_RUNNING               3 
#define IDS_STATE_STARTING              4 
#define IDS_STATE_STOPPING              5 
#define IDS_STATE_UNKNOWN               6 
#define IDS_SERVICE_FAIL_START          7 
#define IDS_SERVICE_FAIL_STOP           8 
#define IDS_WARN_STOPPED                9
#define IDS_WARN_ADMIN                  10
#define IDS_CELL_UNKNOWN                11
#define IDS_GATEWAY_UNKNOWN             12
                                        
#define IDS_BADLOOKUP_DESC              16
#define IDS_NEWSUB_DESC                 17
#define IDS_BADSUB_DESC                 18
#define IDS_BADGATEWAY_DESC             19
#define IDS_BADGWCELL_DESC              20
#define IDS_TITLE_CAUTION_NT            21
#define IDS_TITLE_CAUTION_95            22
#define IDS_TITLE_ERROR_NT              23
#define IDS_TITLE_ERROR_95              24
#define IDS_SHRINKCACHE                 25
#define IDS_BADCELL_DESC_CC             26
                                        
#define IDS_STOP_DESC                   32
#define IDS_PREFCOL_SERVER              33
#define IDS_PREFCOL_RANK                34
#define IDS_TIP_PREFS                   35
#define IDS_PREFERROR_RESOLVE           36
#define IDS_FILTER_TXT                  37
#define IDS_HOSTREM_MANY                38
#define IDS_HOSTREM_ONE                 39
#define IDS_CELLEDIT_TITLE              40
#define IDS_CELLADD_TITLE               41
#define IDS_SVRCOL_SERVER               42
#define IDS_SVRCOL_COMMENT              43
#define IDS_TIP_DRIVES                  44
#define IDS_MAP_LETTER                  45
                                        
#define IDS_DRIVE_MAP                   48
#define IDS_ERROR_MAP                   49
#define IDS_ERROR_MAP_DESC              50
#define IDS_ERROR_UNMAP                 51
#define IDS_ERROR_UNMAP_DESC            52
#define IDS_ADDSERVER_TITLE             53
#define IDS_EDITSERVER_TITLE            54
#define IDS_SUBCOL_SHARE                55
#define IDS_SUBCOL_PATH                 56
#define IDS_SUBMOUNTS_TITLE             57
#define IDS_BADCELL_DESC                58
#define IDS_BADCELL_DESC2               59
#define IDS_KB_IN_USE                   60
#define IDS_NOGATEWAY_TITLE             61
#define IDS_NOGATEWAY_DESC              62

#define IDS_NOCELL_DESC                 64
#define IDS_STOPPED_NOCELL              65
#define IDS_OKSTOP_DESC                 66
#define IDS_BADMAP_DESC                 67
#define IDS_RESTART_TITLE               68
#define IDS_RESTART_DESC                69
#define IDS_KB_ONLY                     70
#define IDS_FAILCONFIG_AUTHENT          71
#define IDS_FAILCONFIG_PREFS            72
#define IDS_FAILCONFIG_CACHE            73
#define IDS_FAILCONFIG_PROBE            74
#define IDS_FAILCONFIG_SYSNAME          75

#define IDS_BADCELL_DESC_CC2            80
#define IDS_NOCELL_DESC_CC              81
#define IDS_TITLE_CAUTION_CCENTER       82
#define IDS_TITLE_ERROR_CCENTER         83
#define IDS_TITLE_CCENTER               84
#define IDS_YES                         85
#define IDS_NO                          86
#define IDS_DRIVE                       87

#define IDD_GENERAL_NT                  101
#define IDI_MAIN                        101
#define IDD_GENERAL_95                  102
#define IDI_UP                          102
#define IDD_PREFS_NT                    103
#define IDI_DOWN                        103
#define IDD_HOSTS_NT                    104
#define IDD_DRIVES                      105
#define IDD_DRIVES_NT                   105
#define IDD_ADVANCED_NT                 106
#define IDD_PREFS_EDIT                  107
#define IDD_CELL_EDIT                   108
#define IDD_SERVER_EDIT                 109
#define IDD_DRIVE_EDIT                  110
#define IDD_SUBMOUNTS                   111
#define IDD_SUBMOUNT_EDIT               112
#define IDD_DRIVES_95                   113
#define IDD_HOSTS_95                    114
#define IDD_STARTSTOP                   115
#define IDD_HOSTS_CCENTER               116
#define IDD_MISC_CONFIG_PARMS           117
#define IDD_DIAG_PARMS                  118
#define IDD_LOGIN_CONFIG_PARMS          119
#define IDD_GLOBAL_DRIVES               120
#define IDD_GLOBAL_DRIVES_ADDEDIT       121
#define IDD_BINDING_CONFIG_PARAMS       122
#define IDC_STATUS                      1000
#define IDC_SERVICE_STOP                1001
#define IDC_SERVICE_START               1002
#define IDC_CELL                        1003
#define IDC_LOGON                       1004
#define IDC_TRAYICON                    1005
#define IDC_GATEWAY_CONN                1006
#define IDC_GATEWAY                     1007
#define IDC_SHOW_VLS                    1008
#define IDC_UP                          1009
#define IDC_DOWN                        1010
#define IDC_REMOVE                      1011
#define IDC_EDIT                        1012
#define IDC_ADD                         1013
#define IDC_LIST                        1014
#define IDC_ADVANCED                    1015
#define IDC_IMPORT                      1015
#define IDC_WARN                        1016
#define IDC_CACHE_SIZE                  1017
#define IDC_INUSE                       1018
#define IDC_CHUNK_SIZE                  1019
#define IDC_SHOW_FS                     1020
#define IDC_STAT_ENTRIES                1021
#define IDC_PROBE                       1022
#define IDC_PROBE2                      1023
#define IDC_LAN_ADAPTER                 1023
#define IDC_THREADS                     1024
#define IDC_DAEMONS                     1025
#define IDC_SYSNAME                     1026
#define IDC_VOLUME                      1027
#define IDC_MOUNTDIR                    1028
#define IDC_REFRESH                     1029
#define IDC_SERVER                      1031
#define IDC_RANK                        1032
#define IDC_COMMENT                     1035
#define IDC_DRIVE                       1041
#define IDC_PATH                        1042
#define IDC_PERSISTENT                  1043
#define IDC_SUBMOUNT                    1044
#define IDC_DESC                        1044
#define IDC_MAPPING                     1045
#define IDC_STARTING                    1046
#define IDC_STOPPING                    1047
#define IDC_CACHE_PATH                  1047
#define IDC_ADDR_SPECIFIC               1048
#define IDC_MISC_PARMS                  1048
#define IDC_ADDR_LOOKUP                 1049
#define IDC_LOGON_PARMS                 1049
#define IDC_TRACE_LOG_BUF_SIZE          1050
#define IDC_AUTOMAP_PARMS               1051
#define IDC_LOGIN_RETRY_INTERVAL        1052
#define IDC_BINDING_PARMS               1053
#define IDC_TRAP_ON_PANIC               1054
#define IDC_REPORT_SESSION_STARTUPS     1055
#define IDC_FAIL_SILENTLY               1056
#define IDC_GLOBAL_DRIVE_LIST           1058
#define IDC_CHANGE                      1059
#define IDC_DIAG_PARMS                  1060
#define IDC_ROOTVOLUME					1061
#define IDC_AUTOLANA                    1062
#define IDC_DEFAULTNIC                  1062
#define IDC_STATICLANA                  1063
#define IDC_STATICSUBMOUNT              1064
#define IDC_NICSELECTION                1065
#define IDC_BINDING_MESSAGE             1066
#define IDC_STATIC                      -1

// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NO_MFC                     1
#define _APS_3D_CONTROLS                     1
#define _APS_NEXT_RESOURCE_VALUE        123
#define _APS_NEXT_COMMAND_VALUE         40001
#define _APS_NEXT_CONTROL_VALUE         1067
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
