# CMUCS AFStools
# Copyright (c) 1996, Carnegie Mellon University
# All rights reserved.
#
# See CMU_copyright.ph for use and distribution information
#
#: * pts.pm - Wrappers around PTS commands (user/group maintenance)
#: * This module provides wrappers around the various PTS commands, giving
#: * them a nice perl-based interface.  Someday, they might talk to the
#: * ptserver directly instead of using 'pts', but not anytime soon.
#:

package OpenAFS::pts;
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT :afs_internal);
use OpenAFS::wrapper;
use Exporter;

$VERSION   = '';
$VERSION   = '1.00';
@ISA       = qw(Exporter);
@EXPORT    = qw(&AFS_pts_createuser    &AFS_pts_listmax
		&AFS_pts_creategroup   &AFS_pts_setmax
		&AFS_pts_delete        &AFS_pts_add
		&AFS_pts_rename        &AFS_pts_remove
		&AFS_pts_examine       &AFS_pts_members
		&AFS_pts_chown         &AFS_pts_listown
		&AFS_pts_setf);


#: AFS_pts_createuser($user, [$id], [$cell])
#: Create a PTS user with $user as its name.
#: If specified, use $id as the PTS id; otherwise, AFS picks one.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return the PTS id of the newly-created user.
#:
$AFS_Help{pts_createuser} = '$user, [$id], [$cell] => $uid';
sub AFS_pts_createuser {
  my($user, $id, $cell) = @_;
  my(@args, $uid);

  @args = ('createuser', '-name', $user);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  push(@args, '-id', $id) if ($id);
  &wrapper('pts', \@args, [[ '^User .* has id (\d+)', \$uid ]]);
  $uid;
}


#: AFS_pts_creategroup($group, [$id], [$owner], [$cell])
#: Create a PTS group with $group as its name.
#: If specified, use $id as the PTS id; otherwise, AFS picks one.
#: If specified, use $owner as the owner, instead of the current user.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return the PTS id of the newly-created group.
#:
$AFS_Help{pts_creategroup} = '$group, [$id], [$owner], [$cell] => $gid';
sub AFS_pts_creategroup {
  my($group, $id, $owner, $cell) = @_;
  my(@args, $uid);

  @args = ('creategroup', '-name', $group);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  push(@args, '-id', $id) if ($id);
  push(@args, '-owner', $owner) if ($owner);
  &wrapper('pts', \@args, [[ '^group .* has id (\-\d+)', \$uid ]]);
  $uid;
}


#: AFS_pts_delete(\@objs, [$cell])
#: Attempt to destroy PTS objects listed in @objs.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return 1.
#: If multiple objects are specified and only some are destroyed, some
#: operations may be left untried.
#:
$AFS_Help{pts_delete} = '\@objs, [$cell] => Success?';
sub AFS_pts_delete {
  my($objs, $cell) = @_;
  my(@args);

  @args = ('delete', '-nameorid', @$objs);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('pts', \@args);
  1;
}


#: AFS_pts_rename($old, $new, [$cell])
#: Rename the PTS object $old to have the name $new.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{pts_rename} = '$old, $new, [$cell] => Success?';
sub AFS_pts_rename {
  my($old, $new, $cell) = @_;
  my(@args);

  @args = ('rename', '-oldname', $old, '-newname', $new);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('pts', \@args);
  1;
}


#: AFS_pts_examine($obj, [$cell])
#: Examine the PTS object $obj, and return information about it.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return an associative array with some or all of the following:
#: - name         Name of this object
#: - id           ID of this object
#: - owner        Name or ID of owner
#: - creator      Name or ID of creator
#: - mem_count    Number of members (group) or memberships (user)
#: - flags        Privacy/access flags (as a string)
#: - group_quota  Remaining group quota
#:
$AFS_Help{pts_examine} = '$obj, [$cell] => %info';
sub AFS_pts_examine {
  my($obj, $cell) = @_;
  my(@args);

  @args = ('examine', '-nameorid', $obj);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('pts', \@args,
	   [[ '^Name\: (.*)\, id\: ([\-0-9]+)\, owner\: (.*)\, creator\: (.*)\,$', #',
	      'name', 'id', 'owner', 'creator' ],
	    [ '^  membership\: (\d+)\, flags\: (.....)\, group quota\: (\d+)\.$',  #',
	      'mem_count', 'flags', 'group_quota' ]
	    ]);
}


#: AFS_pts_chown($obj, $owner, [$cell])
#: Change the owner of the PTS object $obj to be $owner.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{pts_chown} = '$obj, $owner, [$cell] => Success?';
sub AFS_pts_chown {
  my($obj, $owner, $cell) = @_;
  my(@args);

  @args = ('chown', '-name', $obj, '-owner', $owner);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('pts', \@args);
  1;
}


#: AFS_pts_setf($obj, [$access], [$gquota], [$cell])
#: Change the access flags and/or group quota for the PTS object $obj.
#: If specified, $access specifies the new access flags in the standard 'SOMAR'
#: format; individual flags may be specified as '.' to keep the current value.
#: If specified, $gquota specifies the new group quota.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{pts_setf} = '$obj, [$access], [$gquota], [$cell] => Success?';
sub AFS_pts_setf {
  my($obj, $access, $gquota, $cell) = @_;
  my(%result, @args);

  @args = ('setfields', '-nameorid', $obj);
  push(@args, '-groupquota', $gquota) if ($gquota ne '');
  if ($access) {
    my(@old, @new, $i);
    # Ensure access is 5 characters
    if (length($access) < 5) {
      $access .= ('.' x (5 - length($access)));
    } elsif (length($access) > 5) {
      substr($access, 5) = '';
    }

    %result = &AFS_pts_examine($obj, $cell);

    @old = split(//, $result{'flags'});
    @new = split(//, $access);
    foreach $i (0 .. 4) {
      $new[$i] = $old[$i] if ($new[$i] eq '.');
    }
    $access = join('', @new);
    if ($access ne $result{'flags'}) {
      push(@args, '-access', $access);
    }
  }
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('pts', \@args);
  1;
}


#: AFS_pts_listmax([$cell])
#: Fetch the maximum assigned group and user ID.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, returns (maxuid, maxgid)
#:
$AFS_Help{pts_listmax} = '[$cell] => ($maxuid, $maxgid)';
sub AFS_pts_listmax {
  my($cell) = @_;
  my(@args, $uid, $gid);

  @args = ('listmax');
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('pts', \@args,
	   [[ '^Max user id is (\d+) and max group id is (\-\d+).',
	      \$uid, \$gid ]]);
  ($uid, $gid);
}


#: AFS_pts_setmax([$maxuser], [$maxgroup], [$cell])
#: Set the maximum assigned group and/or user ID.
#: If specified, $maxuser is the new maximum user ID
#: If specified, $maxgroup is the new maximum group ID
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{pts_setmax} = '[$maxuser], [$maxgroup], [$cell] => Success?';
sub AFS_pts_setmax {
  my($maxuser, $maxgroup, $cell) = @_;
  my(@args);

  @args = ('setmax');
  push(@args, '-group', $maxgroup) if ($maxgroup);
  push(@args, '-user',  $maxuser)  if ($maxuser);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('pts', \@args);
  1;
}

#: AFS_pts_add(\@users, \@groups, [$cell])
#: Add users specified in @users to groups specified in @groups.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return 1.
#: If multiple users and/or groups are specified and only some memberships
#: are added, some operations may be left untried.
#:
$AFS_Help{pts_add} = '\@users, \@groups, [$cell] => Success?';
sub AFS_pts_add {
  my($users, $groups, $cell) = @_;
  my(@args);

  @args = ('adduser', '-user', @$users, '-group', @$groups);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('pts', \@args);
  1;
}


#: AFS_pts_remove(\@users, \@groups, [$cell])
#: Remove users specified in @users from groups specified in @groups.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return 1.
#: If multiple users and/or groups are specified and only some memberships
#: are removed, some operations may be left untried.
#:
$AFS_Help{pts_remove} = '\@users, \@groups, [$cell] => Success?';
sub AFS_pts_remove {
  my($users, $groups, $cell) = @_;
  my(@args);

  @args = ('removeuser', '-user', @$users, '-group', @$groups);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('pts', \@args);
  1;
}


#: AFS_pts_members($obj, [$cell])
#: If $obj specifies a group, retrieve a list of its members.
#: If $obj specifies a user, retrieve a list of groups to which it belongs.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return the resulting list.
#:
$AFS_Help{pts_members} = '$obj, [$cell] => @members';
sub AFS_pts_members {
  my($obj, $cell) = @_;
  my(@args, @grouplist);

  @args = ('membership', '-nameorid', $obj);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('pts', \@args, [[ '^  (.*)', \@grouplist ]]);
  @grouplist;
}  


#: AFS_pts_listown($owner, [$cell])
#: Retrieve a list of PTS groups owned by the PTS object $obj.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return the resulting list.
#:
$AFS_Help{pts_listown} = '$owner, [$cell] => @owned';
sub AFS_pts_listown {
  my($owner, $cell) = @_;
  my(@args, @grouplist);

  @args = ('listowned', '-nameorid', $owner);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('pts', \@args, [[ '^  (.*)', \@grouplist ]]);
  @grouplist;
}  


1;
