\begindata{text, 268793616}
\textdsversion{11}
\template{default}
\define{italic
menu:[Font,Italic]
attr:[FontFace Italic Int Set]}
\define{bold
menu:[Font,Bold]
attr:[FontFace Bold Int Set]}
\define{chapter
menu:[Title,Chapter]
attr:[Justification LeftJustified Point 0]
attr:[FontFace Bold Int Set]
attr:[FontSize PreviousFontSize Point 4]}
\define{section
menu:[Title,Section]
attr:[Justification LeftJustified Point 0]
attr:[FontFace Bold Int Set]
attr:[FontSize PreviousFontSize Point 2]}
\define{subsection
menu:[Title,Subsection]
attr:[Justification LeftJustified Point 0]
attr:[FontFace Bold Int Set]}
\define{paragraph
menu:[Title,Paragraph]
attr:[Justification LeftJustified Point 0]
attr:[FontFace Italic Int Set]}
\define{bigger
menu:[Font,Bigger]
attr:[FontSize PreviousFontSize Point 2]}
\define{indent
menu:[Region,Indent]
attr:[LeftMargin LeftMargin Inch 32768]
attr:[RightMargin RightMargin Inch 32768]}
\define{typewriter
menu:[Font,Typewriter]
attr:[FontFace FixedFace Int Set]
attr:[FontFamily AndyType Int 0]}
\define{display
menu:[Region,Display]
attr:[LeftMargin LeftMargin Inch 32768]
attr:[RightMargin RightMargin Inch 32768]
attr:[Justification LeftJustified Point 0]}
\define{example
menu:[Region,Example]
attr:[LeftMargin LeftMargin Inch 32768]
attr:[Justification LeftJustified Point 0]
attr:[FontFace FixedFace Int Set]
attr:[FontFamily AndyType Int 0]}
\define{description
menu:[Region,Description]
attr:[LeftMargin LeftMargin Inch 32768]
attr:[Indent LeftEdge Inch -32768]}
\define{quotation
menu:[Region,Quotation]
attr:[LeftMargin LeftMargin Inch 32768]
attr:[RightMargin RightMargin Inch 32768]
attr:[FontFace Italic Int Set]}
\define{subscript
menu:[Font,Subscript]
attr:[Script PreviousScriptMovement Point 131072]
attr:[FontSize PreviousFontSize Point -2]}
\define{superscript
menu:[Font,Superscript]
attr:[Script PreviousScriptMovement Point -393216]
attr:[FontSize PreviousFontSize Point -2]}
\define{smaller
menu:[Font,Smaller]
attr:[FontSize PreviousFontSize Point -2]}
\define{heading
menu:[Title,Heading]
attr:[LeftMargin LeftMargin Inch -13107]
attr:[Justification LeftJustified Point 0]
attr:[FontFace Bold Int Set]}
\define{majorheading
menu:[Title,MajorHeading]
attr:[Justification Centered Point 0]
attr:[FontSize PreviousFontSize Point 4]}
\define{formatnote
menu:[Region,FormatNote]
attr:[Flags PassThru Int Set]}
\define{subheading
menu:[Title,Subheading]
attr:[Justification LeftJustified Point 0]
attr:[FontFace Bold Int Set]}
\define{center
menu:[Justify,Center]
attr:[Justification Centered Point 0]}
\define{flushleft
menu:[Justify,FlushLeft]
attr:[Justification LeftJustified Point 0]}
\define{flushright
menu:[Justify,FlushRight]
attr:[Justification RightJustified Point 0]}
\define{leftindent
menu:[Region,LeftIndent]
attr:[LeftMargin LeftMargin Inch 32768]}


\majorheading{Testing Script for Unlog}
\center{15\center{ November 1987}}\center{
J. Rosenberg
\italic{\smaller{file: in RCS directory ?, file ?}}}

\smaller{This script tests the basic functioning of the program unlog, which is currently executed out of /usr/andrew/bin/.
}
\bold{1.} Pull up two typescript windows. 

\bold{2.} In window number 1, type the following:

	su

   and respond with your password.  This will create a new shell for the same user, but
   associated with a different PAG.

\bold{3.} Touch a file in the home directory in both windows, to demonstrate that authentication
    for both PAGs is in place.  E.g.,

\example{touch ~/testfile}

\bold{4.} Type

	unlog -p 

    in window 1.  This will kill authentication for that shell's PAG.

\bold{5.} Touch a file in the home directory in both windows.  The touch will succeed in the
    second window, but will fail in window 1.

\bold{6.} (Cleanup) Type

	^D 

    in window 1 to terminate the new shell with the unauthenticated PAG.  By the way, the
    original shell that reappears in that typescript will be able to successfully touch the same
    file, since it also belongs to the first PAG.
\enddata{text,268793616}
