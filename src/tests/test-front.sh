#!/bin/sh
pwd=`pwd`
cell=$1
./reauth.pl
PERL5LIB=$pwd
export PERL5LIB
WORKDIR=/afs/${cell}/unreplicated
export WORKDIR
./run-tests -all

