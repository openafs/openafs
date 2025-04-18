#!/usr/bin/perl
    eval 'exec /usr/bin/perl -S $0 ${1+"$@"}'
	if $running_under_some_shell;

@sections = ('1', '3', '5', '8');

$TOP_SRCDIR = shift @ARGV;
$TOP_OBJDIR = shift @ARGV;
$srcdir = sprintf "%s/../doc/xml/AdminRef", $TOP_SRCDIR;
$poddir = sprintf "%s/doc/xml/AdminRef", $TOP_OBJDIR;

open(ENTITIES, ">entities.dtd") || die;

foreach $section (@sections) {
    printf "generating section %s...\n", $section;
    my @entities = ();

    mkdir(sprintf "sect%d", $section);
    opendir($DIR, sprintf "%s/pod%d", $poddir, $section) || die;
    while ($podfile = readdir($DIR)) {
	next unless $podfile =~ /\.pod$/;

	($xmlfile = $podfile) =~ s/\.pod$/.xml/;
	($entity = $xmlfile) =~ s/\.xml$//;

	printf "%s/pod2refentry < %s > %s\n", $srcdir, $podfile, $xmlfile;

	my $rc = system(sprintf "%s/pod2refentry --section=%d < %s/pod%d/%s > sect%d/%s",
	    $srcdir, $section, $poddir, $section, $podfile, $section, $xmlfile);
	if ($rc != 0) {
	    die "Failed to generate sect${section}/${xmlfile}: $rc\n";
	}

	printf ENTITIES "<!ENTITY %s%s SYSTEM \"sect%d/%s\">\n",
	    $entity, $section, $section, $xmlfile;

	push(@entities, $entity);
    }
    closedir($DIR);

    open(SECT, sprintf ">sect%d.xml", $section) || die;
    foreach $entity (sort(@entities)) {
	printf SECT "&%s%s;\n", $entity, $section;
    }

    close(SECT);
}

close(ENTITIES);
