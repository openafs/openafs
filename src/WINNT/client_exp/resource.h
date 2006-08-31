//{{NO_DEPENDENCIES}}
//
// Copyright 2000, International Business Machines Corporation and others.
// All Rights Reserved.
// 
// This software has been released under the terms of the IBM Public
// License.  For details, see the LICENSE file in the top-level source
// directory or online at http://www.openafs.org/dl/license10.html
//
// Used by afs_shl_ext.rc
//

#define ID_ACL_CLEAN                           0
#define ID_ACL_COPY                            1 
#define ID_ACL_LIST                            2 
#define ID_ACL_SET                             3 
#define ID_VOLUME_CHECK                        4 
#define ID_VOLUME_DISKFREE                     5 
#define ID_VOLUME_EXAMINE                      6 
#define ID_VOLUME_FLUSH                        7 
#define ID_VOLUME_QUOTA_BRIEF                  8 
#define ID_VOLUME_QUOTA_FULL                   9 
#define ID_VOLUME_QUOTA_SET                    10
#define ID_VOLUME_SET                          11

#define ID_FLUSH                               16
#define ID_WHEREIS                             17
#define ID_SHOWCELL                            18
#define ID_MOUNTPOINT_SHOW                     19
#define ID_MOUNTPOINT_REMOVE                   20
#define ID_SHOW_SERVER                         21
#define ID_AUTHENTICATE                        22
#define ID_SERVER_STATUS                       23
#define ID_VOLUME_PROPERTIES                   24
#define ID_VOLUME_REFRESH                      25
#define ID_VOLUMEPARTITION_UPDATENAMEIDTABLE   26
#define ID_ACL_COPY_FROM                       27
#define ID_ACL_COPY_TO                         28
#define ID_MOUNTPOINT_MAKE                     29
#define IDS_FLUSH_FAILED                       30
#define IDS_FLUSH_ERROR                        31

#define IDS_FLUSH_OK                           32
#define IDS_CANT_GET_CELL                      33
#define IDS_FLUSH_VOLUME_ERROR                 34
#define IDS_FLUSH_VOLUME_OK                    35
#define IDS_WHERE_IS                           36
#define IDS_CLEANACL_NOT_SUPPORTED             37
#define IDS_ACL_IS_FINE                        38
#define IDS_CLEANACL_INVALID_ARG               39
#define IDS_ANY_STRING                         40
#define IDS_CLEANACL_DONE                      41
#define ID_SUBMOUNTS                           42
#define IDS_GETRIGHTS_ERROR                    43
#define IDS_DFSACL_ERROR                       44
#define IDS_SAVE_ACL_ERROR                     45
#define IDS_SAVE_ACL_EINVAL_ERROR              46
#define IDS_ACL_READ_ERROR                     47

#define IDS_NO_DFS_COPY_ACL                    48
#define IDS_COPY_ACL_EINVAL_ERROR              49
#define IDS_COPY_ACL_ERROR                     50
#define IDS_NOT_MOUNT_POINT_ERROR              51
#define IDS_LIST_MOUNT_POINT_ERROR             52
#define IDS_MAKE_MP_NOT_AFS_ERROR              53
#define IDS_MOUNT_POINT_ERROR                  54
#define IDS_ERROR                              55
#define IDS_DELETED                            56
#define IDS_SET_QUOTA_ERROR                    57
#define ID_SUBMOUNTS_CREATE                    58
#define ID_SUBMOUNTS_EDIT                      59

#define IDS_CHECK_SERVERS_ERROR                64
#define IDS_ALL_SERVERS_RUNNING                65
#define IDS_CHECK_VOLUMES_OK                   66
#define IDS_CHECK_VOLUMES_ERROR                67

#define IDS_ACL_ENTRY_NAME_IN_USE              80
#define IDS_REALLY_DEL_MOUNT_POINTS            81
#define IDS_DIR_DOES_NOT_EXIST_ERROR           82
#define IDS_CLEANACL_MSG                       83
#define IDS_COPY_ACL_OK                        84
#define IDS_GET_TOKENS_NO_AFS_SERVICE          85
#define IDS_GET_TOKENS_UNEXPECTED_ERROR        86
#define IDS_GET_TOKENS_UNEXPECTED_ERROR2       87
#define IDS_ENTER_QUOTA                        88
#define IDS_AUTHENTICATION_ITEM                89
#define IDS_ACLS_ITEM                          90
#define IDS_VOL_PART_ITEM                      91
#define IDS_VOL_PART_PROPS_ITEM                92
#define IDS_VOL_PART_REFRESH_ITEM              93
#define IDS_MOUNT_POINT_ITEM                   94
#define IDS_MP_SHOW_ITEM                       95

#define IDS_MP_REMOVE_ITEM                     96
#define IDS_MP_MAKE_ITEM                       97
#define IDS_FLUSH_FILE_DIR_ITEM                98
#define IDS_FLUSH_VOLUME_ITEM                  99
#define IDS_SHOW_FILE_SERVERS_ITEM             100
#define IDS_SHOW_CELL_ITEM                     101
#define IDS_SHOW_SERVER_STATUS_ITEM            102
#define IDS_AFS_ITEM                           103
#define IDS_SUBMOUNTS_ITEM                     104
#define IDS_GET_SUBMT_INFO_ERROR               105
#define IDS_REALLY_DELETE_SUBMT                106
#define IDS_SUBMT_SAVE_FAILED                  107
#define IDS_SUBMOUNTS_CREATE_ITEM              108
#define IDS_SUBMOUNTS_EDIT_ITEM                109
#define IDS_EDIT_PATH_NAME                     110
#define IDS_SHOW_CELL                          111

#define IDS_SHOW_CELL_COLUMN                   112
#define IDS_SHOW_FS                            113
#define IDS_SHOW_FS_COLUMN                     114
#define IDS_REMOVE_MP                          115
#define IDS_REMOVE_MP_COLUMN                   116
#define IDS_REMOVE_SYMLINK_ITEM                117
#define IDS_REALLY_REMOVE_SYMLINK              118

#define IDS_SYMBOLICLINK_ADD                   128
#define IDS_SYMBOLICLINK_EDIT                  129
#define IDS_SYMBOLICLINK_REMOVE                130
#define IDS_SYMBOLIC_LINK_ITEM                 131
#define IDS_UNABLE_TO_CREATE_SYMBOLIC_LINK     132
#define IDS_UNABLE_TO_SET_CURRENT_DIRECTORY    133
#define IDS_CURRENT_DIRECTORY_PATH_TOO_LONG    134
#define IDS_CLEANACL_ERROR                     135
#define IDS_MAKE_LNK_NOT_AFS_ERROR             136
#define IDS_NOT_AFS_CLIENT_ADMIN_ERROR         137
#define IDS_WARNING			       138
#define IDS_VOLUME_NOT_IN_CELL_WARNING         139

#define IDM_AUTHENTICATION                    0
#define IDM_ACL_SET                           1
#define IDM_VOLUME_PROPERTIES                 2
#define IDM_VOLUMEPARTITION_UPDATENAMEIDTABLE 3
#define IDM_MOUNTPOINT_SHOW                   4
#define IDM_MOUNTPOINT_REMOVE                 5
#define IDM_MOUNTPOINT_MAKE                   6
#define IDM_FLUSH                             7
#define IDM_FLUSH_VOLUME                      8
#define IDM_SHOW_SERVER                       9
#define IDM_SHOWCELL                          10
#define IDM_SERVER_STATUS                     11
#define IDM_SYMBOLICLINK_REMOVE               12
#define IDM_SYMBOLICLINK_ADD                  13
#define IDM_SUBMOUNTS                         14
#define IDM_ACL_CLEAN                         15
#define IDM_SUBMOUNTS_EDIT                    16
#define IDM_REMOVE_SYMLINK                    17
                                           
#define ID_GET_TOKENS                   917
#define ID_DISCARD_TOKENS               918
#define IDD_KLOG_DIALOG                 920
#define IDR_MENU_FILE                   930
#define IDD_VOLUME_INFO                 931
#define IDD_SET_AFS_ACL                 932
#define IDD_DIALOG_TEST                 933
#define IDD_MAKE_MOUNT_POINT            934
#define IDD_CLEAR_ACL                   935
#define IDD_SUBMTINFO                   936
#define IDD_ADD_ACL                     937
#define IDD_PARTITION_INFO              938
#define IDD_COPY_ACL                    940
#define IDD_MENU_TEST_DLG               941
#define IDD_WHICH_CELL                  942
#define IDD_WHERE_IS                    943
#define IDD_RESULTS                     945
#define IDD_MOUNT_POINTS                946
#define IDD_DOWN_SERVERS                947
#define IDD_SHOWSERVERS                 948
#define IDD_SERVERSTATUS                949
#define IDD_AUTHENTICATION              950
#define IDD_UNLOG_DIALOG                954
#define IDD_ADD_SUBMOUNT                955
#define IDD_SYMBOLICLINK_ADD            956
#define ID_REMOVE_SYMLINK               957
#define ID_SYMBOLICLINK_ADD             958
#define ID_SYMBOLICLINK_REMOVE          959
#define IDC_LIST                        1001
#define IDC_PASSWORD                    1002
#define IDC_OFFLINE_MSG                 1003
#define IDC_MOTD                        1004
#define IDC_NEW_QUOTA                   1005
#define IDC_QUOTA_SPIN                  1006
#define IDC_DIR_NAME                    1007
#define IDC_CELL_NAME                   1008
#define ID_VOLUME_INFO                  1009
#define IDC_NEGATIVE_ENTRIES            1010
#define ID_SET_ACL                      1011
#define IDC_NORMAL_RIGHTS               1012
#define ID_MAKE_MOUNT_POINT             1013
#define IDC_LOCALCELL                   1014
#define IDC_DIALOG                      1015
#define IDC_READ                        1016
#define IDC_SPECIFIEDCELL               1017
#define IDC_WRITE                       1018
#define IDC_VOLUME                      1019
#define IDC_ALL_CELLS                   1020
#define IDC_CB_A                        1021
#define IDC_CELL                        1022
#define IDC_ADD                         1023
#define IDC_RW                          1024
#define IDC_DONTPROBESERVERS            1025
#define IDC_REMOVE                      1026
#define IDC_FAST                        1027
#define IDC_NORMAL                      1028
#define IDC_LOOKUP                      1029
#define IDC_NEGATIVE                    1030
#define IDC_NAME                        1031
#define IDC_DIR                         1032
#define IDC_LOCK                        1033
#define IDC_LOOKUP2                     1034
#define IDC_INSERT                      1035
#define IDC_CHANGE                      1036
#define IDC_CLEAR                       1037
#define IDC_LOCK2                       1038
#define IDC_DELETE                      1039
#define IDC_ADMINISTER                  1040
#define IDC_CB_B                        1041
#define IDC_NEGATIVE_ENTRY              1042
#define IDC_COPY                        1043
#define IDC_ADD_NEGATIVE_ENTRY          1044
#define IDC_CB_C                        1045
#define IDC_PARTITION_INFO              1046
#define IDC_CB_F                        1047
#define IDC_CB_E                        1048
#define IDC_TOTAL_SIZE                  1049
#define IDC_CB_D                        1050
#define IDC_BLOCKS_FREE                 1051
#define IDC_FROM_DIR                    1052
#define IDC_CB_G                        1053
#define IDC_PERCENT_USED                1054
#define IDC_TO_DIR                      1055
#define IDC_CB_H                        1056
#define IDC_BUTTON1                     1057
#define IDC_BROWSE                      1058
#define IDC_ACL_CLEAR                   1059
#define IDC_ACL_LIST_SET_COPY           1060
#define IDC_SHOWSTATUS                  1061
#define IDC_VOL_PART_PROPERTIES         1062
#define IDC_COLUMN_3                    1063
#define IDC_VOL_PART_UPDATE             1064
#define IDC_COLUMN_4                    1065
#define IDC_MOUNT_POINT_SHOW            1066
#define IDC_MOUNT_POINT_REMOVE          1067
#define IDC_MOUNT_POINT_MAKE            1068
#define IDC_FLUSH_FILEDIR               1069
#define IDC_FLUSH_VOLUME                1070
#define IDC_SHOW_FILESERVER             1071
#define IDC_SHOW_CELL                   1072
#define IDC_ADD_NORMAL_ENTRY            1073
#define IDC_RESULTS_LABEL               1074
#define IDC_CLEAN                       1075
#define IDC_TOKEN_LIST                  1076
#define IDC_REGULAR                     1077
#define IDC_STATIC_READ                 1078
#define IDC_STATIC_WRITE                1079
#define IDC_STATIC_EXECUTE              1080
#define IDC_STATIC_CONTROL              1081
#define IDC_STATIC_INSERT               1082
#define IDC_STATIC_DELETE               1083
#define IDC_STATIC_CONTROL2             1084
#define IDC_SHARE_NAME                  1085
#define IDC_PATH_NAME                   1086


// Next default values for new objects
// 
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        163
#define _APS_NEXT_COMMAND_VALUE         32829
#define _APS_NEXT_CONTROL_VALUE         1087
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
