#!/usr/bin/env perl
use Term::ReadLine;
use strict;
use OpenAFS::ConfigUtils;
use OpenAFS::Dirpath;
use OpenAFS::OS;
use OpenAFS::Auth;
use Getopt::Long;
use vars qw($admin $server $cellname $cachesize $part
          $requirements_met  $shutdown_needed $csdb);

&OpenAFS::Auth::authadmin();
