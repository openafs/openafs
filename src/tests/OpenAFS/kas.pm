# CMUCS AFStools
# Copyright (c) 1996, Carnegie Mellon University
# All rights reserved.
#
# See CMU_copyright.ph for use and distribution information
#
#: * kas.pm - Wrappers around KAS commands (authentication maintenance)
#: * This module provides wrappers around the various kaserver commands
#: * giving them a nice perl-based interface.  At present, this module
#: * requires a special 'krbkas' which uses existing Kerberos tickets
#: * which the caller must have already required (using 'kaslog').
#:

package OpenAFS::kas;
use OpenAFS::CMU_copyright;
use OpenAFS::util qw(:DEFAULT :afs_internal);
use OpenAFS::wrapper;
use POSIX ();
use Exporter;

$VERSION   = '';
$VERSION   = '1.00';
@ISA       = qw(Exporter);
@EXPORT    = qw(&AFS_kas_create        &AFS_kas_setf
                &AFS_kas_delete        &AFS_kas_setkey
                &AFS_kas_examine       &AFS_kas_setpw
                &AFS_kas_randomkey     &AFS_kas_stringtokey
                &AFS_kas_list);

# Instructions to parse kas error messages
@kas_err_parse = ( [ ' : \[.*\] (.*), wait one second$', '.' ],
                   [ ' : \[.*\] (.*) \(retrying\)$',     '.' ],
                   [ ' : \[.*\] (.*)',                   '-' ]);

# Instructions to parse attributes of an entry
@kas_entry_parse = (
    [ '^User data for (.*) \((.*)\)$',      'princ', 'flags', '.'        ],
    [ '^User data for (.*)',                'princ'                      ],
    [ 'key \((\d+)\) cksum is (\d+),',      'kvno', 'cksum'              ],
    [ 'last cpw: (.*)',                     \&parsestamp, 'stamp_cpw'    ],
    [ 'password will (never) expire',       'stamp_pwexp'                ],
    [ 'password will expire: ([^\.]*)',     \&parsestamp, 'stamp_pwexp'  ],
    [ 'An (unlimited) number of',           'max_badauth'                ],
    [ '(\d+) consecutive unsuccessful',     'max_badauth'                ],
    [ 'for this user is ([\d\.]+) minutes', 'locktime'                   ],
    [ 'for this user is (not limited)',     'locktime'                   ],
    [ 'User is locked (forever)',           'locked'                     ],
    [ 'User is locked until (.*)',          \&parsestamp, 'locked'       ],
    [ 'entry (never) expires',              'stamp_expire'               ],
    [ 'entry expires on ([^\.]*)\.',        \&parsestamp, 'stamp_expire' ],
    [ 'Max ticket lifetime (.*) hours',     'maxlife'                    ],
    [ 'Last mod on (.*) by',                \&parsestamp, 'stamp_update' ],
    [ 'Last mod on .* by (.*)',             'last_writer'                ]);


@Months = ('Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun',
           'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec');
%Months = map(($Months[$_] => $_), 0..11);

# Parse a timestamp
sub parsestamp {
  my($stamp) = @_;
  my($MM, $DD, $YYYY, $hh, $mm, $ss);

  if ($stamp =~ /^\S+ (\S+) (\d+) (\d+):(\d+):(\d+) (\d+)/) {
    ($MM, $DD, $hh, $mm, $ss, $YYYY) = ($1, $2, $3, $4, $5, $6);
    $YYYY -= 1900;
    $MM = $Months{$MM};
    if (defined($MM)) {
      $stamp = POSIX::mktime($ss, $mm, $hh, $DD, $MM, $YYYY);
    }
  }
  $stamp;
}


# Turn an 8-byte key into a string we can give to kas
sub stringize_key {
  my($key) = @_;
  my(@chars) = unpack('CCCCCCCC', $key);

  sprintf("\\%03o" x 8, @chars);
}


# Turn a string into an 8-byte DES key
sub unstringize_key {
  my($string) = @_;
  my($char, $key);

  while ($string ne '') {
    if ($string =~ /^\\(\d\d\d)/) {
      $char = $1;
      $string = $';
      $key .= chr(oct($char));
    } else {
      $key .= substr($string, 0, 1);
      $string =~ s/^.//;
    }
  }
  $key;
}


#: AFS_kas_create($princ, $initpass, [$cell])
#: Create a principal with name $princ, and initial password $initpass
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{kas_create} = '$princ, $initpass, [$cell] => Success?';
sub AFS_kas_create {
  my($print, $initpass, $cell) = @_;
  my(@args, $id);

  @args = ('create', '-name', $princ, '-initial_password', $initpass);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('krbkas', \@args, [ @kas_err_parse ]);
  1;
}


#: AFS_kas_delete($princ, [$cell])
#: Delete the principal $princ.
#: If specified, work in $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{kas_delete} = '$princ, [$cell] => Success?';
sub AFS_kas_delete {
  my($princ, $cell) = @_;
  my(@args);

  @args = ('delete', '-name', $princ);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('krbkas', \@args, [ @kas_err_parse ]);
  1;
}


#: AFS_kas_examine($princ, [$cell])
#: Examine the prinicpal $princ, and return information about it.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return an associative array with some or all of the following:
#: - princ        Name of this principal
#: - kvno         Key version number
#: - cksum        Key checksum
#: - maxlife      Maximum ticket lifetime (in hours)
#: - stamp_expire Time this principal expires, or 'never'
#: - stamp_pwexp  Time this principal's password expires, or 'never'
#: - stamp_cpw    Time this principal's password was last changed
#: - stamp_update Time this princiapl was last modified
#: - last_writer  Administrator who last modified this principal
#: - max_badauth  Maximum number of bad auth attempts, or 'unlimited'
#: - locktime     Penalty time for bad auth (in minutes), or 'not limited'
#: - locked       Set and non-empty if account is locked
#: - expired      Set and non-empty if account is expired
#: - flags        Reference to a list of flags
#:
$AFS_Help{kas_examine} = '$princ, [$cell] => %info';
sub AFS_kas_examine {
  my($vol, $cell) = @_;
  my(%result, @args, $flags);

  @args = ('examine', '-name', $princ);
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  %result = &wrapper('krbkas', \@args, [ @kas_err_parse, @kas_entry_parse ]);

  if ($result{flags}) {
    $result{expired} = 1 if ($result{flags} =~ /expired/);
    $result{flags} = [ split(/\+/, $result{flags}) ];
  }
  %result;
}


#: AFS_kas_list([$cell])
#: Get a list of principals in the kaserver database
#: If specified, work in $cell instead of the default cell.
#: On success, return an associative array whose keys are names of kaserver
#: principals, and each of whose values is an associative array describing
#: the corresponding principal, containing some or all of the same elements
#: that may be returned by AFS_kas_examine
#:
$AFS_Help{kas_list} = '[$cell] => %princs';
sub AFS_kas_list {
  my($cell) = @_;
  my(@args, %finres, %plist);

  @args = ('list', '-long');
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  %finres = &wrapper('krbkas', \@args,
    [ @kas_err_parse,
    [ '^User data for (.*)', sub {
      my(%pinfo) = %OpenAFS::wrapper::result;

      if ($pinfo{name}) {
        $plist{$pinfo{name}} = \%pinfo;
        %OpenAFS::wrapper::result = ();
      }
    }],
      @kas_entry_parse ]);

  if ($finres{name}) {
    $plist{$finres{name}} = \%finres;
  }
  %plist;
}


#: AFS_kas_setf($princ, \%attrs, [$cell])
#: Change the attributes of the principal $princ.
#: If specified, operate in cell $cell instead of the default cell.
#: The associative array %attrs specifies the attributes to change and
#: their new values.  Any of the following attributes may be changed:
#: - flags        Entry flags
#: - expire       Expiration time (mm/dd/yy)
#: - lifetime     Maximum ticket lifetime (seconds)
#: - pwexpires    Maximum password lifetime (days)
#: - reuse        Permit password reuse (yes/no)
#: - attempts     Maximum failed authentication attempts
#: - locktime     Authentication failure penalty (minutes or hh:mm)
#: 
#: On success, return 1.
#:
$AFS_Help{kas_setf} = '$princ, \%attrs, [$cell] => Success?';
sub AFS_kas_setf {
  my($princ, $attrs, $cell) = @_;
  my(%result, @args);

  @args = ('setfields', '-name', $princ);
  push(@args, '-flags',      $$attrs{flags})     if ($$attrs{flags});
  push(@args, '-expiration', $$attrs{expire})    if ($$attrs{expire});
  push(@args, '-lifetime',   $$attrs{lifetime})  if ($$attrs{lifetime});
  push(@args, '-pwexpires',  $$attrs{pwexpires}) if ($$attrs{pwexpires});
  push(@args, '-reuse',      $$attrs{reuse})     if ($$attrs{reuse});
  push(@args, '-attempts',   $$attrs{attempts})  if ($$attrs{attempts});
  push(@args, '-locktime',   $$attrs{locktime})  if ($$attrs{locktime});
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('krbkas', \@args, [ @kas_err_parse ]);
  1;
}


#: AFS_kas_setkey($princ, $key, [$kvno], [$cell])
#: Change the key of principal $princ to the specified value.
#: $key is the 8-byte DES key to use for this principal.
#: If specified, set the key version number to $kvno.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{kas_setkey} = '$princ, $key, [$kvno], [$cell] => Success?';
sub AFS_kas_setkey {
  my($princ, $key, $kvno, $cell) = @_;
  my(@args);

  @args = ('setkey', '-name', $princ, '-new_key', &stringize_key($key));
  push(@args, '-kvno', $kvno) if (defined($kvno));
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('krbkas', \@args, [ @kas_err_parse ]);
  1;
}


#: AFS_kas_setpw($princ, $password, [$kvno], [$cell])
#: Change the key of principal $princ to the specified value.
#: $password is the new password to use.
#: If specified, set the key version number to $kvno.
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return 1.
#:
$AFS_Help{kas_setpw} = '$princ, $password, [$kvno], [$cell] => Success?';
sub AFS_kas_setpw {
  my($princ, $password, $kvno, $cell) = @_;
  my(@args);

  @args = ('setpasswd', '-name', $princ, '-new_password', $password);
  push(@args, '-kvno', $kvno) if (defined($kvno));
  push(@args, '-noauth')           if ($AFS_Parms{'authlvl'} == 0);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('krbkas', \@args, [ @kas_err_parse ]);
  1;
}


#: AFS_kas_stringtokey($string, [$cell])
#: Convert the specified string to a DES key
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return the resulting key
$AFS_Help{kas_stringtokey} = '$string, [$cell] => $key';
sub AFS_kas_stringtokey {
  my($string, $cell) = @_;
  my(@args, $key);

  @args = ('stringtokey', '-string', $string);
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('krbkas', \@args,
    [ @kas_err_parse,
      [ q/^Converting .* in realm .* yields key='(.*)'.$/, \$key ]]);
  &unstringize_key($key);
}


#: AFS_kas_randomkey([$cell])
#: Ask the kaserver to generate a random DES key
#: If specified, operate in cell $cell instead of the default cell.
#: On success, return the resulting key
$AFS_Help{kas_randomkey} = '[$cell] => $key';
sub AFS_kas_randomkey {
  my($cell) = @_;
  my(@args, $key);

  @args = ('getrandomkey');
  push(@args, '-cell', $cell ? $cell : $AFS_Parms{'cell'});
  &wrapper('krbkas', \@args,
    [ @kas_err_parse,
      [ '^Key: (\S+)', \$key ]]);
  &unstringize_key($key);
}

1;
