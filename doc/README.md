# OpenAFS Documentation

## process

Documentation related to the OpenAFS development process and the Contributor
Guide. Includes the code of conduct, guides for submitting changes, and
reporting bugs.

## man-pages

pod sources for man pages (converted from original IBM html source).

## xml

xml sources for manuals (converted from original IBM html source).

**Note:** The `doc/xml/AdminRef` uses `doc/xml/AdminRef/pod2refentry` to
convert the pod man pages to xml for printing.  pod goes directly to html just
fine.

## pdf

Historical Andrew File System (AFS-3) architecture, protocol, and API
documentation as PDF files.

Text versions of these files are available in `doc/protocol`.

## protocol

Text versions of the historical Andrew File System (AFS-3) architecture, protocol
and API documentation files.  These files use Doxygen markup and have a `.h`
extension, but they are not C header files and are not used in the current
build process to generate documentation.

This directory also contains a copy of RFC 5864 **DNS SRV Resource Records for AFS**
in plain text.

## txt

Technical notes, Windows notes, and examples.

## doxygen

Configuration files for the doxygen tool to generate documentation from
the annotated sources. See the `dox` Makefile target in the top level
Makefile.
