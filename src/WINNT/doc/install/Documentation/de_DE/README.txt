Copyright 2000, International Business Machines Corporation and others.
All Rights Reserved.

This software has been released under the terms of the IBM Public
License.  For details, see the LICENSE file in the top-level source
directory or online at http://www.openafs.org/dl/license10.html

All Rights Reserved
***************************************************************

IBM AFS for Windows, version 3.6

***************************************************************
The README.txt file includes AFS for Windows product notes, which
can possibly identify specific limitations and restrictions
associated with this release of AFS for Windows.



AFS Partitions No Longer Need to Reside On Empty NTFS Volumes

On Windows NT machines, any NTFS volume can be designated as an AFS
partition.  Previously, an NTFS volume containing any data other than
the Windows Recycler could not be designated as an AFS partition. 



Encryption Not Supported in Simplified Chinese Version of Windows 98

The Simplified Chinese version of Microsoft Windows 98 does not support
encryption, which is needed to transmit AFS passwords from AFS Light to
the AFS Light Gateway.  In order for AFS Light users to obtain AFS tokens
when using the Simplified Chinese version of Microsoft Windows 98,
encryption in AFS must be disabled.
To disable encryption in AFS, add the following line to your Windows
autoexec.bat file:
set AFS_RPC_ENCRYPT=OFF
Note that disabling encryption introduces a potential security risk
because AFS passwords are transmitted to the AFS Light Gateway in an
unencrypted form when tokens are obtained.



Windows NT with Service Pack 6 Is Now Supported

The Client, Server, and Control Center components of AFS for Windows can
be installed on Microsoft Windows NT 4.0 with Service Pack 4, Service Pack 5,
or Service Pack 6.



AFS for Windows Supplemental Documentation

The Supplemental Documentation component of AFS for Windows is only available
online if the AFS Supplemental Documentation option was chosen when AFS for Windows
was installed on your system.  (AFS Supplemental Documentation is not an option when 
installing AFS Light.) Note that documentation is also available directly
from the AFS for Windows CD-ROM, in the CD:\Documentation directory, where CD
is the letter of your CD-ROM drive.


Refer to the AFS for Windows Release Notes for additional product information.
