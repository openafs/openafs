.TL
Package
.AI
Carnegie Mellon University
.AB
Package is a configuration control system for VICE II workstations
developed at CMU. It provides simple consistent robust software updating
and reconfiguration of workstation local disks based on system description files.
Both the number workstations and configurations is limited only by
the needs of a given site. The control of workstation configurations may be
distributed or fragmented as needed between a number of adminstrators.
.AE
.NH
DESIGN GOALS
.PP
The goal of package was to build a simple robust mechanism on the already
existing VICE II filesystem to maintain a workstations local disk files.
A number of related issues were also addressed in its design.
.PP
Workstations are often located within individual offices that
administrators may not have easy access. Package had to be very robust so
as to minimize the physical access adminstrators would need to workstations.
.PP
The number of both workstations and configurations within ANDREW is increasing
rapidy. Package had to handle massive numbers of workstations of varying 
configuration while sharing as much common configuration information as
possible.
.PP
Support and administrative personnel vary widely in expertise and duties.
Package had to permit the simple division of configuration information
between different persons. For example /vmunix is generally maintained by
system software and is very critical to normal workstation operation,
however /etc/hosts.equiv is more likely to be maintained by a departmental
adminstrator and much less critical.
.PP
Configurations must be easily documented and verifiable. Technicans
lacking software experience must be able to check and verify workstation disk
configurations. Similarly software specialists must be able to collect
configuration information and intelligently plan changes and testing.
.NH
IMPLEMENTATION
.PP
The system was implemented by the construction of two new programs: 
package and mpp, and the use of the standard UNIX utility make.
Please refer to the corresponding manual entries for specific details on
program options.
.PP
The program Package is the actual program that does the updating.
It loads a single configuration file that describes the local disk
completely in memory. Vice directories that are specified as wildcards
are expanded and all vice file modification times are collected. 
If any Vice file that is specified is missing or inconsistently 
specified package aborts.
Package then verifies that local disk partition mount points
agree with configuration specifications.
Finally package updates local disk files based on configuration information.
.PP
The program Mpp is used to construct machine configuration files from
prototype files.
Mpp provides macro preprocessor support. Translating macros in the form
${name} to their
corresponding string values. Macro directives such as "ifdef", "ifndef",
"endif", "undef",
"define", and "include" are supported by mpp.
.PP
The program Make is used to group machine configurations by logical
groups such as
department machines, postoffice machines, public machines, etc... We add the
following lines to the header of all Makefiles to simplify this task.
.nf

	.SUFFIXES: .vax .sun .sun2 .sun3 .ibm032 .proto
	.proto.sun:	; mpp -Dcpu=sun -Dname=$* $*.proto > $@
	.proto.sun2:	; mpp -Dcpu=sun2 -Dname=$* $*.proto > $@
	.proto.sun3:	; mpp -Dcpu=sun3 -Dname=$* $*.proto > $@
	.proto.ibm032:	; mpp -Dcpu=ibm032 -Dname=$* $*.proto > $@
	.proto.vax:	; mpp -Dcpu=vax -Dname=$* $*.proto > $@
.fi
.PP
Note that definition of cpu and name on mpp command line. These are used to
define configuration name and cputype as macros to be used in configuration
specification.
.NH
CONFIGURATION STRATEGIES
.PP
Creating a machine configuration from scratch is somewhat massive. Fortunately
this is rarely required. Generally one master configuration file may be
created that contains all the common information that is constant throughout
all configurations and a number of files that specify option items. The
liberal use of macros is essential for any but the most simple
configurations.
.PP
For example we currently include a file called "cmu.readonly" in most configuration
which contains the lines:
.nf

	%define	common			/cmu/unix/common
	%define	machine			/cmu/unix/${cpu}
.fi
.PP
We may include a file call "disk.ra2" which contains information about
optional two disk microvax configurations which contains the lines.
.nf

	%define	hasdisk
	%define	bigdisk
	DX /lost+found						${treemode}
	DX /tmp/lost+found					${treemode}
	DX /usr/lost+found					${treemode}
	DX /usr/vice/lost+found					${treemode}
	FA /etc/fstab		${machine}/etc/fstab.ra2	${textmode}
	FA /etc/vstab		${machine}/etc/vstab.ra2	${textmode}
.fi
.PP
Following that we include a file called "tree.generic" and a section of this file has
the lines:
.nf

%ifndef	hasdisk
	DX /lost+found					${treemode}
	DX /usr/lost+found				${treemode}
	F  /etc/fstab			${machine}	${textmode}
	F  /etc/vstab			${machine}	${textmode}
%endif	hasdisk
.fi
.PP
Note how unless disk.ra2 is included before the file tree.generic
the default definitions will be used in tree.generic because of the use of
the macro name "hasdisk". Similarly that macro name "bigdisk" may be used
to inform package that the local disk has enough space for /usr/ucb to reside
on local disk and need not be a symbolic link as most configurations have.
For example from file "tree.generic".
.nf

%ifdef bigdisk
	DR			${machine}		${treemode}
%else	bigdisk
	L			${machine}
%endif	bigdisk
.fi
.PP
One very useful trick when dealing with macros in configuration prototype
files is shown in the the following lines:
.nf

%define	${cpu}		xxxxxx
%ifdef	vax
F	/etc/chpt	${machine}
%endif	vax
.fi
In this the macro cpu has the value vax the file /etc/chpt will be added
to the configuration. One should, however be very careful with name conflicts when
using this approach.
It is best to thoughouly examine and understand configuration files
currently in use before changing them or creating new configurations.
The art of developing machine configurations is still very early in its
infancy.
.NH
SUMMARY
.PP
The package system is currently primitive but functional. A much better
configuration generation system should be developed and used as a replacement
for make/mpp.


