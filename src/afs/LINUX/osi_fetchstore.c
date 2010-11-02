/*
 * Copyright (c) 2009 Simon Wilkinson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Linux specific store operations
 *
 * The idea of these operations is to reduce the number of copies
 * that data incurs when passing from the disk cache through to the
 * RX layer, and vice versa.
 *
 * In kernels which support it, we use the splice() operation - in
 * older kernels, the filesystem's sendpage() operation is used directly.
 * Either way, this means that we can get direct access to the page contents,
 * rather than getting a copy.
 */

#include <afsconfig.h>
#include "afs/param.h"

#include <linux/fs.h>
#if defined(HAVE_LINUX_SPLICE_DIRECT_TO_ACTOR)
# include <linux/splice.h>
#else
# include <linux/pipe_fs_i.h>
#endif

#include "afs/sysincludes.h"
#include "afsincludes.h"

struct rockops {
    void *rock;
    struct storeOps *ops;
};

#if defined(HAVE_LINUX_SPLICE_DIRECT_TO_ACTOR)
static int
afs_linux_splice_actor(struct pipe_inode_info *pipe,
		       struct pipe_buffer *buf,
		       struct splice_desc *sd)
{
    struct rockops *rockops = (struct rockops *) (sd->u.data);
    size_t size;
    int code;
    afs_uint32 byteswritten = 0;

    code = buf->ops->confirm(pipe, buf);
    if (code)
	return code;

    size = sd->len;

    /* Eventually, this could be rx_WritePage or better rockops->ops->writepage */
    code = (rockops->ops->write)(rockops->rock, kmap(buf->page), size,
				 &byteswritten);
    if (byteswritten != size)
	size = -33; /* Can't get a proper rx error out from here */

    kunmap(buf->page);

    return size;
}

static int
afs_linux_ds_actor(struct pipe_inode_info *pipe, struct splice_desc *sd)
{
    return __splice_from_pipe(pipe, sd, afs_linux_splice_actor);
}

/* This is a store proc which uses splice to reduce the number
 * of page copies. */
afs_int32
afs_linux_storeproc(struct vcache *avc, struct storeOps *ops, void *rock,
		    struct dcache *tdc, int *shouldwake, afs_size_t *bytesXferred)
{
    struct file *cacheFp;
    struct rockops rockops = {
	.rock = rock,
	.ops = ops
    };
    struct splice_desc sd = {
	.len	= 0,
	.total_len = tdc->f.chunkBytes,
	.pos	= 0,
	.u.data = &rockops
    };
    int code;

    /* Open the file, splice its contents */
    AFS_GUNLOCK();
    cacheFp = afs_linux_raw_open(&tdc->f.inode);
    code = splice_direct_to_actor(cacheFp, &sd, afs_linux_ds_actor);
    filp_close(cacheFp, NULL);
    AFS_GLOCK();

    /* If we're being called from a backing request, then wake up that
     * request once the file server says its happy. Potentially, we should
     * do this each time we rx_Write, but that would mean acquiring the
     * GLOCK in the middle of our actor */
    if (shouldwake && *shouldwake && ((*ops->status)(rock) == 0)) {
	*shouldwake = 0;
	afs_wakeup(avc);
    }

    if (code > 0) {
	*bytesXferred+=code;
        return 0;
    }

    return code;
}

# else

static int
afs_linux_read_actor(read_descriptor_t *desc, struct page *page,
		     unsigned long offset, unsigned long size)
{
    struct rockops *rockops = (struct rockops *) desc->arg.data;
    unsigned long count = desc->count;
    int code;
    afs_uint32 byteswritten = 0;

    if (size > count)
	size = count;

    /* Eventually, this could be rx_WritePage or better rockops->ops->writepage */
    code = (rockops->ops->write)(rockops->rock, kmap(page) + offset, size,
				 &byteswritten);
    kunmap(page);

    if (byteswritten != size) {
        return -33; /* Can't get a proper rx error out from here */
    }

    desc->count = count - size;
    desc->written += size;

    return size;
}

afs_int32
afs_linux_storeproc(struct vcache *avc, struct storeOps *ops, void *rock,
		    struct dcache *tdc, int *shouldwake, afs_size_t *bytesXferred)
{
    struct file *cacheFp;
    int code;
    loff_t offset = 0;
    struct rockops rockops = {
	.rock = rock,
	.ops = ops
    };

    /* Open the file, splice its contents */
    AFS_GUNLOCK();
    cacheFp = afs_linux_raw_open(&tdc->f.inode);
    code = cacheFp->f_op->sendfile(cacheFp, &offset, tdc->f.chunkBytes,
				   afs_linux_read_actor, &rockops);
    filp_close(cacheFp, NULL);
    AFS_GLOCK();

    /* If we're being called from a backing request, then wake up that
     * request once the file server says its happy. Potentially, we should
     * do this each time we rx_Write, but that would mean acquiring the
     * GLOCK in the middle of our actor */
    if (shouldwake && *shouldwake && ((*ops->status)(rock) == 0)) {
	*shouldwake = 0;
	afs_wakeup(avc);
    }

    if (code > 0) {
	*bytesXferred+=code;
        return 0;
    }

    return code;
}

#endif
