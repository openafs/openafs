# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# Quickly tests a subset of the authentication attempts limits changes to the
# kaserver.

# invent a bunch of temporary users (delete, then create them)
# setfields their attempts and locktimes:  Note -- I don't verify that 
# kas exam reports the correct thing as doing the conversion from text
# to numeric and the math and the conversion in expect would be more likely
# to be wrong than the C code in admin_tools.c and kaprocs.c and kaauxdb.c

# fail to authenticate once
# verify that klog does not report locked
# verify that exam reports not locked
# fail to authenticate many times - 1.
# verify that klog does not report locked
# verify that exam reports locked (unless -attempts is 0)
# fail to authenticate once more.
# verify that klog does report locked  (unless -attempts is 0)
# verify that exam reports locked  (unless -attempts is 0)

# check that kas unlock works (implicit)
# check that the correct # of attempts remain after some # of failures? (implicit)
source testlocking.utils.tcl

for { set i 0 } { $i<10 } { set i [expr $i+1] } { 
   set usr lyletest$i
   create_user $usr
}

verify_auto_reset $usr

for { set i 9 } { $i>=0 } { set i [expr $i-1] } { 
   set usr lyletest$i

   spawn $kas setfields $usr -attempts $i -locktime [expr $i*10] -admin_user $admin -password_for_admin $adminpass -cell $cell

   expect_nothing 0
   wait_for $spawn_id
   verify_attempts $usr $i
   verify_lock $usr $i
}



