#!/usr/bin/perl
#
# Make VNODE structure from INODE structure
#
# Created By:	Derek Atkins <warlord@MIT.EDU>
#

$linux_header_dir="/usr/src/linux";
$outdir="./src/afs/LINUX";

$sepline="/* LINUX VNODE INCLUDED BELOW -- DO NOT MODIFY */\n";

# makeVfs (fs.h, vfs.hin, vfs.out)
sub makeVfs {
    my ($in,$base,$out) = @_;
    my ($seplinefound);

    open (IN, "$in") || die "Cannot open $in for reading";
    open (BASE, "$base" ) || die "Cannot open base file $base";
    open (OUT, ">$out") || die "Cannot open tempfile $out";

    while (<BASE>) {
	print OUT;
	if ($_ eq $sepline) {
	    $seplinefound = 1;
	    last;
	}
    }

    print OUT $sepline if !$seplinefound;

    my ($state) = 0;
    while (<IN>) {

	# Look for 'struct inode' definition
	if ($state == 0) {
	    next unless m/^struct\s+inode\s*\{/;
	    $state++;
	    s/inode/vnode/;
	    # Fallthrough
	}

	# Look for 'union {' -- print otherwise
	if ($state == 1) {
	    if (m/^\s*union\s*\{/) {
		$state++;
		print OUT "#ifdef notdef\n";
	    }
	    print OUT;
	    next;
	}

	# Look for the end of the union -- ignore otherwise
	if ($state == 2) {
	    print OUT;
	    next unless (m/^\s+\}\s*u;/);
	    $state++;
	    print OUT "#endif /* notdef */\n";
	    next;
	}

	# Look for end brace -- print until we find it
	if ($state == 3) {
	    print OUT;
	    if (m/^\s*\};/) { $state++ }
	}
    }

    while (<BASE>) { print OUT; }

    close (IN);
    close (BASE);
    close (OUT);
}

sub usage {
    print "usage: $0 [-i linux_header_dir] [-o output_dir] [-h]\n";
    exit 1;
}

sub testArg {
    my ($arg) = @_;
    return $arg if ($arg && $arg ne "");
    usage;
}

while ($_ = shift @ARGV) {
    if (m/^-i/) { $linux_header_dir = testArg(shift @ARGV); next; }
    if (m/^-o/) { $outdir = testArg(shift @ARGV); next; }
    usage;
}

$linux_fs_h="$linux_header_dir/include/linux/fs.h";
$vfs_h="$outdir/osi_vfs.h";
$vfs_hin="$outdir/osi_vfs.hin";

makeVfs ($linux_fs_h, $vfs_hin, "$vfs_h.new");

system ("cmp", "-s", $vfs_h, "$vfs_h.new");
$exit_value = $? >> 8;
$signal_num = $? & 127;
$core_dump  = $? & 128;

if ($exit_value == 0 || $signal_num > 0) {
    unlink "$vfs_h.new";
    print "nothing to do... $vfs_h not changed.\n"
} else {
    unlink "$vfs_h";
    rename ("$vfs_h.new", $vfs_h);
    print "wrote $vfs_h\n";
}

exit 0;
