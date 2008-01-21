#!/usr/bin/env perl
use strict;
use warnings;
use OpenAFS::Dirpath;
use OpenAFS::Auth;
use Getopt::Long;

# options
my $debug = 0;
my $cellname = 'testcell';
my $admin = 'admin';
my $kerberos_type = 'MIT';
my $kerberos_realm = 'TESTCELL';
my $kerberos_keytab = "$openafsdirpath->{'afsconfdir'}/krb5.keytab";

GetOptions (
       "debug!" => \$debug,
       "cellname=s" => \$cellname, 
       "admin=s" => \$admin,
       "kerberos-type=s" => \$kerberos_type,
       "kerberos-realm=s" => \$kerberos_realm,
       "kerberos-keytab=s" => \$kerberos_keytab,
       );

my $auth = OpenAFS::Auth::create(
      'debug'=>$debug,
      'type'=>$kerberos_type, 
      'cell'=>$cellname,
      'realm'=>$kerberos_realm,
      'keytab'=>$kerberos_keytab,
      );

# Run as the administrator.
$auth->authorize($admin);
