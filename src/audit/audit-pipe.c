/*
 * Copyright (c) 2019, Sine Nomine Associates
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>

#if defined(AFS_PTHREAD_ENV) && !defined(AFS_NT40_ENV)

#include <afs/afsutil.h>
#include "audit-api.h"
#include <rx/rx.h>

#include <unistd.h>
#include <sys/file.h>

#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <rx/rx_atomic.h>

#include <afs/afs_lock.h>

/* Default, min and max size in K of a single buffer */
#define BUFSIZE_DEFAULT (4 * 1024) /* 4M */
#define BUFSIZE_MIN (1) /* 1K */
#define BUFSIZE_MAX (8 * 1024 * 1024) /* 8G */

struct pipe_buffer {
    char *data;
    ssize_t data_len;

    long int n_msgs; /* how many audit messages are in 'data'? */

    unsigned long gap_msgid; /* id to use for the _Dropped pseudo-event */

    unsigned long gap_count; /* how many messages we dropped while this buffer
			      * was full */
    time_t gap_first; /* time of first dropped message */
    unsigned long gap_firstid; /* id of first dropped message */

    time_t gap_last; /* time of last dropped message */
    unsigned long gap_lastid; /* id of last dropped message */
};

struct pipe_stats {
    rx_atomic_t msgs_total;
    rx_atomic_t msgs_queued;
    rx_atomic_t msgs_dropped;
    rx_atomic_t msgs_truncated;
    rx_atomic_t msgs_error;
    rx_atomic_t msgs_inqueue;
    rx_atomic_t msgs_writing;
    rx_atomic_t msgs_dequeued;
    rx_atomic_t msgs_written;
    rx_atomic_t drops_written;
    rx_atomic_t write_errors;
    rx_atomic_t msg_max;
    rx_atomic_t writer_waiting;
};

struct pipe_ctx {
    char *filename; /* the path to our named pipe */
    ssize_t bufsize; /* how much buffer space to allocate for messages */
    struct pipe_stats stats;

    int id; /* arbitrary id to distinguish between different instances of the
	     * pipe audit interface */

    int thread_running; /* is pipe_thread() running? */

    char drop_buf[OSI_AUDIT_MAXMSG]; /* Scratch space for formatting our "drop"
				      * pseudo-events */

    struct pipe_buffer buf_writing; /* buffer of messages the background
				     * thread is writing to the pipe */

    opr_mutex_t lock; /* protects everything in struct pipe_ctx below here. */
    opr_cv_t cv; /* signalled whenever something is added to msg_queued, or
		  * 'shutdown' is set */

    struct pipe_buffer buf_queued; /* buffer of audit messages that are queued
				    * up */

    unsigned long msg_counter; /* counter for giving ids to the messages we
				* write; a message's id along with its timestamp
				* can be used to uniquely identify a message */

    int shutdown; /* shutdown requested. the background thread should stop
		   * running as soon as possible */
};

static int
set_option(void *rock, char *opt, char *val)
{
    struct pipe_ctx *ctx = rock;

    if (opt == NULL) {
	return 0;
    }

    opr_Assert(ctx != NULL);
    opr_Assert(!ctx->thread_running);

    if (strcmp("buf", opt) == 0) {
	afs_int32 bsize;

	if (val == NULL || *val == '\0') {
	    fprintf(stderr, "audit-pipe: Missing value for 'buf'.\n");
	    goto error;
	}
	if (util_GetHumanInt32(val, &bsize) != 0) {
	    fprintf(stderr, "audit-pipe: Invalid value for 'buf': %s.\n", val);
	    goto error;
	}

	if (bsize < BUFSIZE_MIN) {
	    fprintf(stderr, "audit-pipe: Value given for 'buf' is too small "
		    "(%dK). Minimum size is %dK bytes.\n",
		    bsize, BUFSIZE_MIN);
	    goto error;
	}
	if (bsize > BUFSIZE_MAX) {
	    fprintf(stderr, "audit-pipe: Value given for 'buf' is too large "
		    "(%dK). Maximum size is %dK bytes.\n",
		    bsize, BUFSIZE_MAX);
	    goto error;
	}

	ctx->bufsize = bsize * 1024LL;

	opr_Assert(BUFSIZE_MIN * 1024LL <= ctx->bufsize);
	opr_Assert(ctx->bufsize <= BUFSIZE_MAX * 1024LL);

    } else {
	fprintf(stderr, "audit-pipe: Unknown option '%s'\n", opt);
	goto error;
    }

    return 0;

 error:
    return EINVAL;
}

/*
 * Free the buffers within the context, destroy the lock and conditional
 * variable, then free the context itself.
 */
static void
free_ctx(struct pipe_ctx **a_ctx)
{
    struct pipe_ctx *ctx = *a_ctx;
    if (ctx == NULL) {
	return;
    }

    free(ctx->buf_queued.data);
    free(ctx->buf_writing.data);

    free(ctx->filename);

    opr_mutex_destroy(&ctx->lock);
    opr_cv_destroy(&ctx->cv);

    free(ctx);

    *a_ctx = NULL;
}

/*
 * Set up the context for an interface
 *   Initialze the lock and conditional variable
 */
static void *
create_interface(void)
{
    struct pipe_ctx *ctx;

    ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
	goto enomem;
    }

    ctx->bufsize = BUFSIZE_DEFAULT * 1024;

    opr_mutex_init(&ctx->lock);
    opr_cv_init(&ctx->cv);

    return ctx;

 enomem:
    fprintf(stderr, "audit-pipe: Error allocating memory\n");
    return NULL;
}

/*
 * Shutdown the interface.
 *  Note: can be called if open_file() or open_interface() fails.
 */
static void
close_interface(void **rock)
{
    struct pipe_ctx *ctx = *rock;

    if (ctx == NULL) {
	return;
    }

    if (ctx->thread_running) {
	/*
	 * If our background thread is running, just indicate that it should
	 * shutdown; the background thread will handle freeing the ctx.
	 */
	opr_mutex_enter(&ctx->lock);
	ctx->shutdown = 1;
	opr_cv_signal(&ctx->cv);
	opr_mutex_exit(&ctx->lock);
    } else {
	/* If the background thread is not running, free the ctx ourselves. */
	free_ctx(&ctx);
    }
    *rock = NULL;
}

static int
pipe_create(struct pipe_ctx *ctx, int log_errors)
{
    int code;
    struct stat statbuf;
    char *filename = ctx->filename;

    memset(&statbuf, 0, sizeof(statbuf));

    code = stat(filename, &statbuf);
    if (code != 0) {
	if (errno != ENOENT) {
	    if (log_errors) {
		fprintf(stderr, "audit-pipe: cannot stat %s (errno %d)\n",
			filename, errno);
	    }
	    goto error;
	}

	/* If the file just doesn't exist, create the fifo ourselves. */
	code = mkfifo(filename, 0600);
	if (code < 0) {
	    if (log_errors) {
		fprintf(stderr, "audit-pipe: cannot create named pipe %s "
			"(errno %d)\n", filename, errno);
	    }
	    goto error;
	}

    } else if (!S_ISFIFO(statbuf.st_mode)) {
	/* File exists, but it's not a fifo. */
	if (log_errors) {
	    fprintf(stderr, "audit-pipe: cannot open %s; not a named pipe\n",
		    filename);
	}
	goto error;
    }
    return 0;

 error:
    return -1;
}

/*
 * Save the filename for the consumer thread. Note that we cannot actually
 * open() the pipe here, because doing so may block, waiting for a reader on
 * the pipe.
 *
 * Obtain buffers, and create the consumer thread.
 */
static int
open_file(void *rock, const char *fileName)
{
    /*
     * instance_counter is not protected by any locks; this function is never
     * called in different threads.
     */
    static int instance_counter = 0;

    int code;
    struct pipe_ctx *ctx = rock;

    if (fileName == NULL) {
	fprintf(stderr, "audit-pipe: missing auditlog name\n");
	return -1;
    }

    ctx->filename = strdup(fileName);
    if (ctx->filename == NULL) {
	goto enomem;
    }

    opr_Assert(ctx->bufsize > 0);

    /* Set up the buffers */
    ctx->buf_queued.data = calloc(1, ctx->bufsize);
    if (ctx->buf_queued.data == NULL) {
	goto enomem;
    }

    ctx->buf_writing.data = calloc(1, ctx->bufsize);
    if (ctx->buf_writing.data == NULL) {
	goto enomem;
    }

    code = pipe_create(ctx, 1);
    if (code != 0) {
	goto error;
    }

    ctx->id = ++instance_counter;
    return 0;

 enomem:
    fprintf(stderr, "audit-pipe: Error allocating memory\n");

 error:
    return -1;
}

static FILE *
pipe_open(struct pipe_ctx *ctx)
{
    int first;
    FILE *fh = NULL;

    const char *filename = ctx->filename;

    rx_atomic_set(&ctx->stats.writer_waiting, 1);

    /* Retry forever, but only log errors on the first try. */
    for (first = 1; ; first = 0) {
	int fd;
	int code;
	int log_errors;
	struct stat statbuf;
	int open_flags = O_WRONLY;

#ifdef O_CLOEXEC
	open_flags |= O_CLOEXEC;
#endif

	if (first) {
	    log_errors = 1;
	} else {
	    log_errors = 0;
	    sleep(1);
	}

	code = pipe_create(ctx, log_errors);
	if (code != 0) {
	    continue;
	}

	fd = open(filename, open_flags);
	if (fd < 0) {
	    if (log_errors) {
		fprintf(stderr, "audit-pipe: cannot open named pipe %s "
			"(errno %d)\n", filename, errno);
	    }
	    continue;
	}
	code = fstat(fd, &statbuf);
	if (code < 0) {
	    close(fd);
	    if (log_errors) {
		fprintf(stderr, "audit-pipe: cannot fstat named pipe %s "
			"(errno %d)\n", filename, errno);
	    }
	    continue;
	}
	if (!S_ISFIFO(statbuf.st_mode)) {
	    close(fd);
	    if (log_errors) {
		fprintf(stderr, "audit-pipe: cannot open %s; not a named pipe\n",
			filename);
	    }
	    continue;
	}
	fh = fdopen(fd, "w");
	if (fh == NULL) {
	    close(fd);
	    if (log_errors) {
		fprintf(stderr, "audit-pipe: cannot fdopen named pipe %s "
			"(errno %d)\n", filename, errno);
	    }
	    continue;
	}

	/* Success */
	rx_atomic_set(&ctx->stats.writer_waiting, 0);

	break;
    }

    return fh;
}

static int
write_gap(struct pipe_ctx *ctx, FILE *fh, struct pipe_buffer *buf)
{
    char tbuffer[32];
    int len;
    int code;
    time_t currenttime;
    struct tm tm;

    currenttime = time(0);
    if (strftime(tbuffer, sizeof(tbuffer), "%a %b %d %H:%M:%S %Y ",
		 localtime_r(&currenttime, &tm)) == 0) {
	tbuffer[0] = '\0';
    }
    len = snprintf(ctx->drop_buf, sizeof(ctx->drop_buf),
		   "[%lu] %.24s EVENT AFS_Aud_Pipe_Dropped COUNT %lu FIRST %lu "
		   "FIRSTID %lu LAST %lu LASTID %lu\n",
		   buf->gap_msgid,
		   tbuffer,
		   buf->gap_count,
		   (unsigned long)buf->gap_first,
		   buf->gap_firstid,
		   (unsigned long)buf->gap_last,
		   buf->gap_lastid);

    if (len < 0 || len >= sizeof(ctx->drop_buf)) {
	/*
	 * Either there was an error during formatting, or we truncated the
	 * message. In either case, just don't write anything.
	 */
	rx_atomic_inc(&ctx->stats.write_errors);
	return 0;
    }

    code = fwrite(ctx->drop_buf, len, 1, fh);
    if (code != 1) {
	return -1;
    }
    return 0;
}

static int
write_messages_once(struct pipe_ctx *ctx, FILE *fh, struct pipe_buffer *buf)
{
    int code;
    int wrote_drop = 0;

    /* Write out our actual audit message data */
    code = fwrite(buf->data, buf->data_len, 1, fh);
    if (code != 1) {
	return -1;
    }

    if (buf->gap_count != 0) {
	/* Write out our _Dropped pseudo-event */
	code = write_gap(ctx, fh, buf);
	if (code != 0) {
	    return code;
	}
	wrote_drop = 1;
    }

    code = fflush(fh);
    if (code != 0) {
	return -1;
    }

    /*
     * If we reached here, everything was written successfully, so update our
     * stats.
     */
    if (wrote_drop) {
	rx_atomic_inc(&ctx->stats.drops_written);
    }
    rx_atomic_add(&ctx->stats.msgs_written, buf->n_msgs);

    return 0;
}

static void
write_messages(struct pipe_ctx *ctx, FILE **a_fh, struct pipe_buffer *buf)
{
    FILE *fh = *a_fh;

    for (;;) {
	int code;

	code = write_messages_once(ctx, fh, buf);
	if (code == 0) {
	    /*
	     * Messages were written successfully; there's nothing else we need
	     * to do.
	     */
	    *a_fh = fh;
	    return;
	}

	/*
	 * We got some error while trying to write to the pipe. Reopen it and
	 * retry.
	 */
	rx_atomic_inc(&ctx->stats.write_errors);
	fclose(fh);
	fh = pipe_open(ctx);
    }
}

static_inline void
buf_swap(struct pipe_buffer *buf1, struct pipe_buffer *buf2)
{
    struct pipe_buffer tmp = *buf1;
    *buf1 = *buf2;
    *buf2 = tmp;
}

static_inline void
buf_clear(struct pipe_buffer *buf)
{
    char *data = buf->data;

    /* Clear the message payload, just in case */
    memset(data, 0, buf->data_len);

    /* Zero out the entire buf struct, except for 'data'. */
    memset(buf, 0, sizeof(*buf));
    buf->data = data;
}

static void *
pipe_thread(void *rock)
{
    struct pipe_ctx *ctx = rock;
    FILE *pipe_fh;
    struct pipe_buffer *buf;
    char name[32];

    snprintf(name, sizeof(name), "AuditPipe:%d", ctx->id);
    name[sizeof(name) - 1] = '\0';
    opr_threadname_set(name);

    pipe_fh = pipe_open(ctx);

    opr_mutex_enter(&ctx->lock);

    for (;;) {
	/* Wait for buf_queued to be not empty. */
	while (!ctx->shutdown && ctx->buf_queued.n_msgs == 0) {
	    opr_cv_wait(&ctx->cv, &ctx->lock);
	}

	if (ctx->shutdown) {
	    goto done;
	}

	/*
	 * Move buf_queued to our 'local' copy (buf_writing), so we can access
	 * it outside of the lock.
	 */
	buf_swap(&ctx->buf_queued, &ctx->buf_writing);
	buf = &ctx->buf_writing;

	if (buf->gap_count > 0) {
	    /* Allocate a msgid for the _Dropped message we'll write. */
	    buf->gap_msgid = ctx->msg_counter++;
	}

	rx_atomic_set(&ctx->stats.msgs_writing, buf->n_msgs);
	rx_atomic_add(&ctx->stats.msgs_dequeued, buf->n_msgs);
	rx_atomic_set(&ctx->stats.msgs_inqueue, 0);

	opr_mutex_exit(&ctx->lock);

	write_messages(ctx, &pipe_fh, buf);
	buf_clear(buf);

	rx_atomic_set(&ctx->stats.msgs_writing, 0);

	opr_mutex_enter(&ctx->lock);
    }

 done:
    opr_mutex_exit(&ctx->lock);
    if (pipe_fh != NULL) {
	fclose(pipe_fh);
	pipe_fh = NULL;
    }
    free_ctx(&ctx);
    return NULL;
}

/*
 * Set up thread and start consumer thread
 */
static void
open_interface(void *rock)
{
    pthread_t tid;
    struct pipe_ctx *ctx = rock;

    /*
     * Note that we cannot race with close_interface() here (the other user of
     * thread_running); our caller guarantees that calls to open_interface()
     * and close_interface() are serialized.
     */
    opr_Assert(!ctx->thread_running);
    ctx->thread_running = 1;

    opr_Verify(pthread_create(&tid, NULL, pipe_thread, ctx) == 0);
    opr_Verify(pthread_detach(tid) == 0);
}

static int
buf_appendmsg(struct pipe_ctx *ctx, struct pipe_buffer *buf, const char *data,
	      int data_len, unsigned long msgid)
{
    ssize_t remaining;
    ssize_t printed;

    remaining = ctx->bufsize - buf->data_len;
    if (remaining < data_len + 5) {
	/*
	 * We know we will use at least data_len bytes, plus the bytes for
	 * "[%lu]\n" (at least 5 bytes for, e.g., "[0]\n"). So if we don't have
	 * enough space for that, we can return an ENOSPC error without needing
	 * to go through snprintf.
	 */
	return ENOSPC;
    }

    printed = snprintf(&buf->data[buf->data_len], remaining,
		       "[%lu] %.*s\n", msgid, data_len, data);
    if (printed >= remaining) {
	/*
	 * Keep our buffer terminated; we shouldn't _need_ to explicitly
	 * terminate the buffer, since we always know how many bytes of data we
	 * have in there. But do it anyway just in case, to make sure nothing
	 * goes running off the end.
	 */
	buf->data[ctx->bufsize - 1] = '\0';
	return ENOSPC;
    }

    if (printed < 0) {
	return EINVAL;
    }

    buf->data_len += printed;
    buf->n_msgs++;

    opr_Assert(buf->data_len <= ctx->bufsize);

    opr_cv_signal(&ctx->cv);

    return 0;
}

static int
pipe_enqueue(struct pipe_ctx *ctx, const char *data, int data_len)
{
    int code;
    unsigned long msgid;
    struct pipe_buffer *buf;

    opr_mutex_enter(&ctx->lock);

    if (data_len > rx_atomic_read(&ctx->stats.msg_max)) {
	rx_atomic_set(&ctx->stats.msg_max, data_len);
    }

    buf = &ctx->buf_queued;
    msgid = ctx->msg_counter++;

    code = buf_appendmsg(ctx, buf, data, data_len, msgid);
    if (code == ENOSPC) {
	/*
	 * buf_queued doesn't have enough space to store our message; we must
	 * drop the message instead.
	 * Keep track of the first and last messages dropped as well as the
	 * timestamps.
	 */
	time_t now = time(NULL);

	if (buf->gap_count < ULONG_MAX) {
	    buf->gap_count++;
	}
	if (buf->gap_first == 0) {
	    buf->gap_first = now;
	    buf->gap_firstid = msgid;
	    buf->gap_msgid = 0;
	}
	buf->gap_last = now;
	buf->gap_lastid = msgid;

	rx_atomic_inc(&ctx->stats.msgs_dropped);
	code = 0;

    } else if (code != 0) {
	/*
	 * We encountered some other kind of error (like an internal formatting
	 * error). Pretend we never saw this message by deallocating our msgid.
	 */
	ctx->msg_counter--;
	goto done;

    } else {
	/* We queued the message successfully */
	rx_atomic_inc(&ctx->stats.msgs_inqueue);
	rx_atomic_inc(&ctx->stats.msgs_queued);
    }

 done:
    opr_mutex_exit(&ctx->lock);
    return code;
}

/*
 * Send the message. Add the composed message to the buffer
 * for the consumer thread to write to the pipe.
 */
static void
send_msg(void *rock, const char *message, int msglen, int truncated)
{
    struct pipe_ctx *ctx = rock;
    int code;

    rx_atomic_inc(&ctx->stats.msgs_total);

    code = pipe_enqueue(ctx, message, msglen);
    if (code != 0) {
	rx_atomic_inc(&ctx->stats.msgs_error);
    } else if (truncated) {
	rx_atomic_inc(&ctx->stats.msgs_truncated);
    }
}

/*
 * Print statics
 *
 */
static void
print_interface_stats(void *rock, FILE *out)
{
    struct pipe_ctx *ctx = rock;

    struct pipe_stats stats = ctx->stats;

    fprintf(out, "audit statistics for pipe:%s:buf=%ldK:\n",
	    ctx->filename, ctx->bufsize / 1024L);

    if (rx_atomic_read(&stats.writer_waiting)) {
	fprintf(out, "    waiting for reader to connect to named pipe\n");
    } else {
	fprintf(out, "    connected and writing to named pipe\n");
    }

    fprintf(out, "    %d messages total, %d queued, %d dropped, %d truncated, %d errors\n",
	    rx_atomic_read(&stats.msgs_total),
	    rx_atomic_read(&stats.msgs_queued),
	    rx_atomic_read(&stats.msgs_dropped),
	    rx_atomic_read(&stats.msgs_truncated),
	    rx_atomic_read(&stats.msgs_error));

    fprintf(out, "    %d in queue, %d being written\n",
	    rx_atomic_read(&stats.msgs_inqueue),
	    rx_atomic_read(&stats.msgs_writing));

    fprintf(out, "    %d dequeued, %d written to pipe\n",
	    rx_atomic_read(&stats.msgs_dequeued),
	    rx_atomic_read(&stats.msgs_written));

    fprintf(out, "    %d drop events written, %d write errors\n",
	    rx_atomic_read(&stats.drops_written),
	    rx_atomic_read(&stats.write_errors));

    fprintf(out, "    %d bytes max per message\n",
	    rx_atomic_read(&stats.msg_max));
}

const struct osi_audit_ops audit_pipe_ops = {
    &send_msg,
    &open_file,
    &print_interface_stats,
    &create_interface,
    &close_interface,
    &set_option,
    &open_interface,
};

#endif /* AFS_PTHREAD_ENV && !AFS_NT40_ENV */
