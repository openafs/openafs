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

set kas "kas"
set klog "klog"
set multiklog "multiklog"
set admin "admin"
set adminpass "adminpassword"
set cell "cellname1"
set usr "guest"
set admininfo "-admin_us admin -password_for adminpass"
set cellinfo "-cell cellname2"
set timeout 600
set initialpasswd "InitialPassword"
trap SIG_IGN { SIGCHLD }

proc wait_for { x } {
	select $x
	expect timeout { } \
	       eof { } 
}

proc verify_attempts { usr x } {
   global kas klog admin adminpass cell timeout timeout

   spawn $kas exam $usr -admin_us $admin -password_fo $adminpass -cell $cell
   print "Expecting $x attempts.\n"

   expect timeout {print "\nFAILED: timeout: presume user is not locked\n"} \
      eof {print "\nFAILED: eof: presume user is not locked\n"} \
      "{* An unlimited number*}" { if $x then { print "\nFAILED: expected only $x attempts, got: $expect_match" } else { print "\nSUCCESS\n" }  } \
      "{*consecutive unsuccessful authentications are permitted*}" \
      { if $x then { print "SUCCESS\n"; print \
      "!!! in this case, should check to be sure that the int matches $x\n" } \
      else { print "\nFAILED: expected unlimited, got: $expect_match"} } 

   wait_for $spawn_id
}

proc FailToAuthenticate { usr reps expectlockmsg } {
	global multiklog cell 
 
   set locked 0
   set failed 0
   spawn $multiklog $usr randomtrashnotpassword -cell $cell -rep $reps
      
   expect timeout {print "\nFAILED: timeout\n"; set failed 1 } 	\
         "{*Unable*locked*(KALOCKED)*}" { set locked 1 } 		\
         "{*password was incorrect*}" { }			\
         eof {print "\nFAILED: eof but expected KALOCKED\n"; set failed 1 } 

   wait_for $spawn_id

   if { ! $failed } {					\
   if { $expectlockmsg } then {				\
      if { $locked } then { 				\
	 print "\nSUCCESS\n" }				\
      else { print "\nFAILED: klog didn't report KALOCKED\n" } \
      }							\
   else {					\
      if { $locked } then { 				\
          print "\nFAILED: (locked too soon) $expect_match\n" } \
      else { print "\nSUCCESS: klog didn't report KALOCKED\n" } \
      }							\
   }
}

proc SucceedToAuthenticate { usr reps expectlockmsg } {
	global multiklog cell initialpasswd
 
   set locked 0
   set failed 1
   spawn $multiklog $usr $initialpasswd -cell $cell -rep $reps
      
   expect timeout {print "\nSUCCESS: timeout\n"; set failed 0 } 	\
         "{*Unable*is locked*}" { set locked 1 } 		\
         "{*password was incorrect*}" { }			\
         eof {print "\nSUCCESS: eof\n"; set failed 0}
   wait_for $spawn_id

   if { $expectlockmsg } then { if {$locked} then {			\
      print "\nSUCCESS: klog reported KALOCKED\n" }	\
      else { print "\nFAILED: expected KALOCKED but got $expect_match\n" } } \
   else { if { $failed } {					\
          print "\nFAILED: $expect_match" } }
}

proc ExamKasLocked { usr expectlockmsg } {
   global kas admin adminpass cell 	   

   spawn $kas exam $usr -admin_us $admin -password_fo $adminpass -cell $cell
   if { $expectlockmsg } { print "Expecting to find user locked..." }

   expect timeout { if { $expectlockmsg } {print "\nFAILED: timeout\n"} else  { print "\nSUCCESS: timeout\n" }  } \
      eof { if { $expectlockmsg } {print "\nFAILED: eof\n"} else  { print "\nSUCCESS: eof\n" }  } \
      "{*User is not locked*}" { if { $expectlockmsg } then { print "\nFAILED: expected $usr to be locked\n"} else { print "\nSUCCESS: kas didn't report LOCKED\n" }  } \
      "{*User is locked*}" { if { ! $expectlockmsg } then { print "\nFAILED: $usr should not be locked\n"} else { print "\nSUCCESS: kas reported LOCKED\n"} } 

   wait_for $spawn_id
}

# klog should not return KALOCKED after j failures.  That is, the ID becomes
# locked after the j'th failure, but that's not the reason for the denial.
# On the next attempt, klog returns KALOCKED regardless of whether the password
# was correct or not.  So if a user is restricted to -attempts 1, he gets one
# chance, and if he blows it, his ID gets locked.
proc verify_lock { usr j } {
   global kas klog admin adminpass cell multiklog

   set failed 0

   if { $j == 0 } then { set x 258 } else { set x $j }

   spawn $kas unloc $usr -admin_user $admin -password_fo $adminpass -cell $cell
   expect_nothing 2
   wait_for $spawn_id
   ExamKasLocked $usr 0

# fail once if $x is sufficiently large, and make sure that neither klog
# nor kas report that the id is locked.
   if { $x > 2 } { \
	set x [expr $x-1]
	FailToAuthenticate $usr 1 0
	ExamKasLocked $usr 0
	}
#
# after this multiklog, exam should report that the ID is locked (only if $j>0)
# but klog should not have reported KALOCKED.
#
   set expectlockmsg 0
   FailToAuthenticate $usr $x $expectlockmsg
   ExamKasLocked $usr $j
   
#   
# after this klog, kas exam should report that the user is locked.  klog
# should also return KALOCKED. 
#
   if { $j } then {							\
        FailToAuthenticate $usr 1 1
	ExamKasLocked $usr 1
	}
}

# If a user fails several times, but then successfully authenticates,
# the attempts counter on one server should get reset.  Check to be 
# sure that happens.
proc verify_auto_reset { usr } {
   global kas klog admin adminpass cell multiklog initialpasswd

   spawn $kas unloc $usr -admin_user $admin -password_fo $adminpass -cell $cell
   expect_nothing 2
   wait_for $spawn_id

# set attempts to be 103
   spawn $kas setfields $usr -attempts 103  -admin_us $admin \
          -password_for $adminpass -cell $cell
   expect_nothing 0
   wait_for $spawn_id

   FailToAuthenticate $usr 101 0
   ExamKasLocked $usr 0

# this should make some more attempts possible.  Expected value is 101/#servers, but 
# multiklog may not distribute attempts randomly.
   SucceedToAuthenticate $usr 1 0

# ordinarily, this would fail (101+14 > 103) but the success above should have set
# one server back.
   FailToAuthenticate $usr 14 0
   ExamKasLocked $usr 0

# If there is one server, it should take ~89 attempts to lock this ID.
# If there are seven, it should take only ~15...
   FailToAuthenticate $usr 90 1
   ExamKasLocked $usr 1
   SucceedToAuthenticate $usr 1 1
   ExamKasLocked $usr 1
}

proc verify_lock_time {usr x} {
   global kas klog admin adminpass cell timeout timeout

   spawn $kas exam $usr -admin_us $admin -password_fo $adminpass -cell $cell
   print "Expecting locktime == $x.\n"
   case "$x" in { 00:00 } { set x 0 } 

   expect "{* The lock time for this user is not limited.*}" \
      { if {$x} then { print "\nFAILED: expected limited locktime, got: $expect_match" } else { print "\nSUCCESS\n" }  } \
      "{* The lock time for this user is * minutes*}" \
      { if {$x} then { print "SUCCESS\n"; print \
      "!!! in this case, should check to be sure that the interval matches $x\n" } \
      else {print "\nFAILED: expected unlimited locktime, got: $expect_match"}}\
	timeout {print "\nFAILED: timeout checking locktime\n"} \
      	eof {print "\nFAILED: eof checking locktime\n"} 

   wait_for $spawn_id
}

proc expect_timelimits_msg {x} {
   expect timeout {print "\nFAILED: timeout\n"} \
	  eof {print "\nFAILED: eof\n"} \
          "{*Lockout times must be either minutes or hh:mm.\r\nLockout times must be less than 36 hours.*}" {print "\nSUCCESS\n" } 
}

proc expect_limits_msg {x} {
   expect timeout {print "\nFAILED: timeout\n"} \
	  eof {print "\nFAILED: eof\n"} \
          "{*Failure limit must be in*}" {print "\nSUCCESS\n" } \
          "*" {print "\nFAILED: $expect_match"}
}

proc expect_nothing { x } {
   expect {*Note:\ Operation\ is\ performed\ on\ cell\ *\n} { print "\n" } \
	  timeout { print "\n" } \
	  eof     { print "\n" } \
          "*" { print "\nFAILED: expected nothing, got: $expect_match" }
}

proc create_user { usr } {
   global kas klog admin adminpass cell initialpasswd timeout

   spawn $kas delete $usr -admin_us $admin -password_fo $adminpass -cell $cell

   expect  timeout { print "\nSUCCESS: delete didn't complain\n" }   \
	   eof     { print "\nSUCCESS: delete didn't complain\n" }   \
           "{Deleting user $usr already*}" { print "\nSUCCESS: user gone\n" } \
	   "*"     { print "\nFAILURE: $expect_match\n" }

   wait_for $spawn_id

   spawn $kas create $usr $initialpasswd -admin_us $admin -password_fo $adminpass -cell $cell 
   expect  timeout { print "\nSUCCESS: create didn't complain\n" }   \
	   eof     { print "\nSUCCESS: create didn't complain\n" }   \
	   "*"     { print "\nFAILURE: $expect_match\n" }

   wait_for $spawn_id

 verify_attempts $usr 0
}
