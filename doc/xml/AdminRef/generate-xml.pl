#!/usr/bin/perl
    eval 'exec /usr/bin/perl -S $0 ${1+"$@"}'
	if $running_under_some_shell;

@sections = ('1', '5', '8');

$TOP_SRCDIR = shift @ARGV;
$doc_man_pages = sprintf "%s/../doc/man-pages", $TOP_SRCDIR;

open(ENTITIES, ">entities.dtd") || die;

foreach $section (@sections) {
    printf "generating section %s...\n", $section;

    mkdir(sprintf "sect%d", $section);
    opendir($DIR, sprintf "%s/pod%d", $doc_man_pages, $section) || die;
    open(SECT, sprintf ">sect%d.xml", $section) || die;
    while ($podfile = readdir($DIR)) {
	next unless $podfile =~ /\.pod$/;

	($xmlfile = $podfile) =~ s/\.pod$/.xml/;
	($entity = $xmlfile) =~ s/\.xml$//;

	printf "pod2refentry < %s > %s\n", $podfile, $xmlfile;

	system(sprintf "./pod2refentry --section=%d < %s/pod%d/%s > sect%d/%s",
	    $section, $doc_man_pages, $section, $podfile, $section, $xmlfile);

	printf ENTITIES "<!ENTITY %s%s SYSTEM \"sect%d/%s\">\n",
	    $entity, $section, $section, $xmlfile;
	printf SECT "&%s%s;\n", $entity, $section;
    }
    closedir($DIR);
    close(SECT);
}

close(ENTITIES);
