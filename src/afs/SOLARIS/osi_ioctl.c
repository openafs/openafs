/*
 * Copyright 2010, Sine Nomine Associates and others.
 * All Rights Reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

#include <afsconfig.h>
#include "afs/param.h"

/* ioctl-based emulation for the AFS syscall, for Solaris 11 and onwards. */

#ifdef AFS_SUN511_ENV

#include "afs/sysincludes.h"	/* Standard vendor system headers */
#include "afsincludes.h"	/* Afs-based standard headers */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#define DEVAFS_MINOR 0

static int
devafs_open(dev_t *devp, int flag, int otyp, cred_t *cred_p)
{
    if (getminor(*devp) != DEVAFS_MINOR) {
	return EINVAL;
    }
    if (otyp != OTYP_CHR) {
	return EINVAL;
    }
    return 0;
}

static int
devafs_close(dev_t dev, int flag, int otyp, cred_t *cred_p)
{
    if (getminor(dev) != DEVAFS_MINOR) {
	return EINVAL;
    }
    if (otyp != OTYP_CHR) {
	return EINVAL;
    }
    return 0;
}

static int
devafs_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *cred_p,
             int *rval_p)
{
    int error;
    struct afssysa ua;
    rval_t rv;

    if (getminor(dev) != DEVAFS_MINOR) {
	return EINVAL;
    }

    switch (cmd) {
    case VIOC_SYSCALL: {
	struct afssysargs sargs;
	error = ddi_copyin((const void *)arg, &sargs, sizeof(sargs), mode);
	if (error) {
	    return EFAULT;
	}
	ua.syscall = sargs.syscall;
	ua.parm1 = sargs.param1;
	ua.parm2 = sargs.param2;
	ua.parm3 = sargs.param3;
	ua.parm4 = sargs.param4;
	ua.parm5 = sargs.param5;
	ua.parm6 = sargs.param6;

	break;
    }
    case VIOC_SYSCALL32: {
	struct afssysargs32 sargs32;
	error = ddi_copyin((const void *)arg, &sargs32, sizeof(sargs32), mode);
	if (error) {
	    return EFAULT;
	}
	ua.syscall = sargs32.syscall;
	ua.parm1 = sargs32.param1;
	ua.parm2 = sargs32.param2;
	ua.parm3 = sargs32.param3;
	ua.parm4 = sargs32.param4;
	ua.parm5 = sargs32.param5;
	ua.parm6 = sargs32.param6;

	break;
    }
    default:
	return EINVAL;
    }

    rv.r_val1 = 0;
    error = Afs_syscall(&ua, &rv);

    if (!error) {
	error = rv.r_val1;
    }

    return error;
}

static struct cb_ops devafs_cb_ops = {
    /* .cb_open =     */ devafs_open,
    /* .cb_close =    */ devafs_close,
    /* .cb_strategy = */ nodev,
    /* .cb_print =    */ nodev,
    /* .cb_dump =     */ nodev,
    /* .cb_read =     */ nodev,
    /* .cb_write =    */ nodev,
    /* .cb_ioctl =    */ devafs_ioctl,
    /* .cb_devmap =   */ nodev,
    /* .cb_mmap =     */ nodev,
    /* .cb_segmap =   */ nodev,
    /* .cb_chpoll =   */ nochpoll,
    /* .cb_prop_op =  */ ddi_prop_op,
    /* .cb_str =      */ NULL,
    /* .cb_flag =     */ D_NEW | D_MP | D_64BIT,
    /* .cb_rev =      */ CB_REV,
    /* .cb_aread =    */ nodev,
    /* .cb_awrite =   */ nodev,
};

static dev_info_t *devafs_dip = NULL;

static int
devafs_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **resultp)
{
    switch (cmd) {
    case DDI_INFO_DEVT2DEVINFO:
	*resultp = devafs_dip;
	return DDI_SUCCESS;
    case DDI_INFO_DEVT2INSTANCE:
	*resultp = 0; /* we only have one instance */
	return DDI_SUCCESS;
    default:
	return DDI_FAILURE;
    }
}

static int
devafs_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
    int error;
    switch (cmd) {
    case DDI_ATTACH:
	osi_Assert(devafs_dip == NULL);

	error = ddi_create_minor_node(dip, "afs", S_IFCHR, DEVAFS_MINOR,
	                              DDI_PSEUDO, 0);
	if (error != DDI_SUCCESS) {
	    return DDI_FAILURE;
	}

	devafs_dip = dip;

	ddi_report_dev(dip);

	return DDI_SUCCESS;
    case DDI_RESUME:
	return DDI_FAILURE;
    default:
	return DDI_FAILURE;
    }
}

static int
devafs_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
    switch (cmd) {
    case DDI_DETACH:
	osi_Assert(devafs_dip != NULL);

	devafs_dip = NULL;

	ddi_prop_remove_all(dip);
	ddi_remove_minor_node(dip, NULL);

	return DDI_SUCCESS;
    case DDI_SUSPEND:
	return DDI_FAILURE;
    default:
	return DDI_FAILURE;
    }
}

static struct dev_ops afs_devops = {
    /* .devo_rev =      */ DEVO_REV,
    /* .devo_refcnt =   */ 0,
    /* .devo_getinfo =  */ devafs_getinfo,
    /* .devo_identify = */ nulldev,
    /* .devo_probe =    */ nulldev,
    /* .devo_attach =   */ devafs_attach,
    /* .devo_detach =   */ devafs_detach,
    /* .devo_reset =    */ nodev,
    /* .devo_cb_ops =   */ &devafs_cb_ops,
    /* .devo_bus_ops =  */ NULL,
    /* .devo_power =    */ NULL,
    /* .devo_quiesce =  */ ddi_quiesce_not_needed,
};

extern struct mod_ops mod_driverops;

struct modldrv afs_modldrv = {
    &mod_driverops,
    "/dev/afs driver",
    &afs_devops,
};

#endif /* AFS_SUN511_ENV */
