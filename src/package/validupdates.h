/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/*------------------------------------------------------------------------
 * validupdates.c
 *
 * Description:
 *	Specification of all valid update modes for package, the AFS
 *	workstation configuration tool.
 *
 *------------------------------------------------------------------------*/

#include <sys/param.h>

struct updatetype
{
    u_short filetype;	/*Type of file*/
    u_short updtflags;	/*A valid update specification for the filetype*/
};

/*
  * These entries record the meaning of the sequence of letters appearing
  * as the first field of a line in the package configuration file.
  *
  * Question: Are FIQ and FIAQ combinations semantically valid?
  */
static struct updatetype validupdates[] =
{
  S_IFBLK,	/* B    */	0,
  S_IFCHR,	/* C    */	0,
  S_IFDIR,	/* D    */	0,
  S_IFDIR,	/* DA   */	U_ABSPATH, 
  S_IFDIR,	/* DR   */	U_RMEXTRA,
  S_IFDIR,	/* DRA  */	U_ABSPATH | U_RMEXTRA,
  S_IFDIR,	/* DX   */	U_LOSTFOUND,
  S_IFDIR,	/* DXA  */	U_ABSPATH | U_LOSTFOUND,
  S_IFREG,	/* F    */	0,
  S_IFREG,	/* FA   */	U_ABSPATH,
  S_IFREG,	/* FI   */	U_NOOVERWRITE,
  S_IFREG,	/* FIA  */	U_ABSPATH | U_NOOVERWRITE,
  S_IFREG,	/* FO   */	U_RENAMEOLD,
  S_IFREG,	/* FOA  */	U_ABSPATH | U_RENAMEOLD,
  S_IFREG,	/* FQ   */	U_REBOOT,
  S_IFREG,	/* FAQ  */	U_ABSPATH | U_REBOOT,
  S_IFREG,	/* FIQ  */	U_NOOVERWRITE | U_REBOOT,
  S_IFREG,	/* FIAQ */	U_ABSPATH | U_NOOVERWRITE | U_REBOOT,
  S_IFREG,	/* FOQ  */	U_RENAMEOLD | U_REBOOT,
  S_IFREG,	/* FOAQ */	U_ABSPATH | U_RENAMEOLD | U_REBOOT,
  S_IFLNK,	/* L    */	0,
  S_IFLNK,	/* LA   */	U_ABSPATH,
  S_IFLNK,	/* LI   */	U_NOOVERWRITE,
  S_IFLNK,	/* LIA  */	U_ABSPATH | U_NOOVERWRITE,
#ifndef AFS_AIX_ENV
  S_IFSOCK,	/* S    */	0,
#endif /* AFS_AIX_ENV */
#ifdef S_IFIFO
  S_IFIFO,	/*P     */	0,
  S_IFIFO,	/*PA    */	U_ABSPATH,
  S_IFIFO,	/*PO    */	U_RENAMEOLD,
  S_IFIFO,	/*PI    */	U_NOOVERWRITE,
  S_IFIFO,	/*PAO   */	U_ABSPATH | U_RENAMEOLD,
  S_IFIFO,	/*PAI   */	U_NOOVERWRITE,
#endif /* S_IFIFO */
  /* W	0, $$what?: undocumented feature */
	0,		0
};
