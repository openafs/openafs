# Copyright 2000, International Business Machines Corporation and others.
# All Rights Reserved.
# 
# This software has been released under the terms of the IBM Public
# License.  For details, see the LICENSE file in the top-level source
# directory or online at http://www.openafs.org/dl/license10.html

# boundary conditions:
# verify locking occurs on n+1 failures, where n is the permitted number.
# verify that n in {0,1,253,254} work
# verify that n == 255 or greater fails
# verify that n < 0 fails
# locktime < 0 should fail
# locktime == 0 should succeed (infinite)
# verify locktime of 0 (infinite) produces correct output from kas exam
# locktime <= 36*60 should work quietly
# locktime > 2160 should give error message
# test locktimes in hh:mm also, 00:00 00:01 01:00 01:01 35:59 36:00 36:01

# fail to authenticate once
# verify that klog does not report locked
# verify that exam reports not locked
# fail to authenticate many times - 1.
# verify that klog does not report locked
# verify that exam reports locked (unless -attempts is 0)
# fail to authenticate once more.
# verify that klog does report locked  (unless -attempts is 0)
# verify that exam reports locked  (unless -attempts is 0)

# note: attempts don't reset totally, only one server's attempts counter
# gets reset.
# set attempts to 200                    (check that the attempts reset)
# fail several times, then succeed
# check that you can retry 200 times before id gets locked

# check that kas unlock works (implicit)
# check that the correct # of attempts remain after some # of failures? (implicit)

# the following things are not yet tested.  That is, this test procedure
# does not sleep for 36 hours to see if the id will automatically unlock
# or not.
#
# verify locktimes of 1 min to 36*60-1 work
# test locktimes of 36*60, 36*60+1, 36*60+14, 36*60+15
# verify locktime of 0 (infinite) remains locked for longer than 36*60+30
source testlocking.utils.tcl

set usr lyletest0
create_user $usr

# test the error cases 
foreach i { -1 255 256 } { 
   spawn $kas setfields $usr -attempts $i -admin_us $admin \
         -password_for $adminpass -cell $cell
   expect_limits_msg $i
   wait_for $spawn_id
}

# I expect these cases to succeed
foreach i { 0 1 253 254 } { 
   spawn $kas setfields $usr -attempts $i  -admin_us $admin \
          -password_for $adminpass -cell $cell
   expect_nothing $i
   wait_for $spawn_id
   verify_attempts $usr $i
   verify_lock $usr $i
}

verify_lock_time $usr 0

# test the lock time error cases 
set i -1 
spawn $kas setfields $usr -attempts 1 -locktime $i -admin_us $admin \
         -password_for $adminpass -cell $cell
expect_timelimits_msg $i
wait_for $spawn_id

foreach i { 2161 2162 2175 36:01 36:02 36:14 36:15  } { 
   spawn $kas setfields $usr -attempts 1 -locktime $i -admin_us $admin \
         -password_for $adminpass -cell $cell
   expect_timelimits_msg $i
   expect timeout {print "\nFAILED: timeout\n"} \
	  eof {print "\nFAILED: eof\n"} \
          "{*Continuing with lock time of exactly 36 hours.*}" {print "\nSUCCESS\n" } 
   wait_for $spawn_id
}
 
foreach i { 0 1 2 3 4 5 6 7 8 9 10 55 60 61 2159 2160 00:00 } { 
   spawn $kas setfields $usr -attempts 1 -locktime $i -admin_us $admin \
         -password_for $adminpass -cell $cell
   expect_nothing $i
   wait_for $spawn_id
   verify_lock_time $usr $i
}

# having a hard time with these guys, so I do them individually....
   set i 00:01 
   spawn $kas setfields $usr -attempts 1 -locktime $i -admin_us $admin \
         -password_for $adminpass -cell $cell
   expect_nothing $i
   wait_for $spawn_id
   verify_lock_time $usr 1

   set i 01:00 
   spawn $kas setfields $usr -attempts 1 -locktime $i -admin_us $admin \
         -password_for $adminpass -cell $cell
   expect_nothing $i
   wait_for $spawn_id
   verify_lock_time $usr 60

   set i 01:01 
   spawn $kas setfields $usr -attempts 1 -locktime $i -admin_us $admin \
         -password_for $adminpass -cell $cell
   expect_nothing $i
   wait_for $spawn_id
   verify_lock_time $usr 61

   set i 35:59 
   spawn $kas setfields $usr -attempts 1 -locktime $i -admin_us $admin \
         -password_for $adminpass -cell $cell
   expect_nothing $i
   wait_for $spawn_id
   verify_lock_time $usr 2159

   set i 36:00
   spawn $kas setfields $usr -attempts 1 -locktime $i -admin_us $admin \
         -password_for $adminpass -cell $cell
   expect_nothing $i
   wait_for $spawn_id
   verify_lock_time $usr 2160
