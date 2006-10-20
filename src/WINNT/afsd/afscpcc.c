/*
 * Copyright 2005,2006 Secure Endpoints Inc.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the MIT License.  
 */

#include <windows.h>
#include <afskfw.h>

int main(int argc, char *argv[])
{
    if ( argc != 2 )
        return 1;

    KFW_initialize();

    return KFW_AFS_copy_file_cache_to_default_cache(argv[1]);
}


