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

/*
 * Background page copying
 *
 * In the Linux CM, we pull cached files in from disk by reading them into
 * a page backed by the disk file, then copying them into the relevant AFS
 * page. This is a syncronous operation, requiring us to wait until the
 * disk read is completed before the page copy can be performed. When we're
 * doing readahead with readpages(), it means that the readpages() call must
 * block until the readahead is complete, which somewhat defeats the point.
 *
 * This file implements a background queuing system for performing these
 * page copies. For each collection of pages requiring copying, a new
 * task is created by calling afs_pagecopy_init_task(). Every time
 * readpage() on the backing cache returns a page which is still locked,
 * afs_pagecopy_queue_page() can be called to queue up a background copy
 * of this page.  queue_page() ensures that the new page is connected to
 * the current task structure, and that that task is on a locally implemented
 * work queue.
 *
 * The work queue is handled by a dedicated kernel thread (created by
 * afs_init_pagecopy() and destroyed with afs_shutdown_pagecopy() ). This
 * thread iterates on the queue, moving all pages that are unlocked to a
 * different list, and placing tasks with unlocked pages onto the kernel
 * work queue. Once it has run through all of the unlocked pages, it will
 * identify a still-locked page to sleep upon, and wait until that page is
 * unlocked.
 *
 * The final act of copying the pages is performed by a per-task job in the
 * kernel work queue (this allows us to use multiple processors on SMP systems)
 */

#include <afsconfig.h>
#include "afs/param.h"

#include <linux/pagemap.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

static DECLARE_WAIT_QUEUE_HEAD (afs_pagecopy_wq);
static spinlock_t afs_pagecopy_lock;
static struct list_head afs_pagecopy_tasks;
static struct task_struct * afs_pagecopy_thread_id;

struct afs_pagecopy_page {
    struct page *afspage;
    struct page *cachepage;
    struct list_head tasklink;
};

struct afs_pagecopy_task {
    struct work_struct work;
    struct list_head checkpages;
    struct list_head copypages;
    atomic_t refcnt;
    spinlock_t lock;
    struct list_head joblink;
};

#if defined(INIT_WORK_HAS_DATA)
static void afs_pagecopy_worker(void *rock);
#else
static void afs_pagecopy_worker(struct work_struct *work);
#endif

struct afs_pagecopy_task *
afs_pagecopy_init_task(void) {
    struct afs_pagecopy_task *task;

    task = kzalloc(sizeof(struct afs_pagecopy_task), GFP_NOFS);
    INIT_LIST_HEAD(&task->checkpages);
    INIT_LIST_HEAD(&task->copypages);
    INIT_LIST_HEAD(&task->joblink);
#if defined(INIT_WORK_HAS_DATA)
    INIT_WORK(&task->work, afs_pagecopy_worker, &task->work);
#else
    INIT_WORK(&task->work, afs_pagecopy_worker);
#endif
    spin_lock_init(&task->lock);
    atomic_inc(&task->refcnt);

    return task;
}

void afs_pagecopy_queue_page(struct afs_pagecopy_task *task,
			     struct page *cachepage,
			     struct page *afspage)
{
    struct afs_pagecopy_page *page;

    page = kzalloc(sizeof(struct afs_pagecopy_page), GFP_NOFS);
    INIT_LIST_HEAD(&page->tasklink);

    page_cache_get(cachepage);
    page->cachepage = cachepage;
    page_cache_get(afspage);
    page->afspage = afspage;

    spin_lock(&task->lock);
    list_add_tail(&page->tasklink, &task->checkpages);
    spin_lock(&afs_pagecopy_lock);
    if (list_empty(&task->joblink)) {
	atomic_inc(&task->refcnt);
	list_add_tail(&task->joblink, &afs_pagecopy_tasks);
    }
    spin_unlock(&afs_pagecopy_lock);
    spin_unlock(&task->lock);

    wake_up_interruptible(&afs_pagecopy_wq);
}

void afs_pagecopy_put_task(struct afs_pagecopy_task *task)
{
    if (!atomic_dec_and_test(&task->refcnt))
	return;

    kfree(task);
}

static struct page * afs_pagecopy_checkworkload(void) {
    struct page *sleeppage = NULL;
    struct afs_pagecopy_task *task, *tmp_task;
    struct afs_pagecopy_page *page, *tmp_page;

    spin_lock(&afs_pagecopy_lock);
    list_for_each_entry_safe(task, tmp_task, &afs_pagecopy_tasks, joblink) {
	spin_unlock(&afs_pagecopy_lock);

	spin_lock(&task->lock);
	list_for_each_entry_safe(page, tmp_page, &task->checkpages, tasklink) {
	   if (!PageLocked(page->cachepage)) {
		list_move_tail(&page->tasklink, &task->copypages);
		atomic_inc(&task->refcnt);
		if (!schedule_work(&task->work))
		   atomic_dec(&task->refcnt);
	   } else if (!sleeppage) {
		page_cache_get(page->cachepage);
		sleeppage = page->cachepage;
	   }
	}
	/* If the task structure has no more pages to check, remove it
	 * from our workload queue */
	if (list_empty(&task->checkpages)) {
	    spin_lock(&afs_pagecopy_lock);
	    spin_unlock(&task->lock);
	    list_del_init(&task->joblink);
	    spin_unlock(&afs_pagecopy_lock);
	    afs_pagecopy_put_task(task);
	} else {
	    spin_unlock(&task->lock);
	}
	spin_lock(&afs_pagecopy_lock);
    }
    spin_unlock(&afs_pagecopy_lock);

    return sleeppage;
}

#if defined(INIT_WORK_HAS_DATA)
static void afs_pagecopy_worker(void *work)
#else
static void afs_pagecopy_worker(struct work_struct *work)
#endif
{
    struct afs_pagecopy_task *task =
	container_of(work, struct afs_pagecopy_task, work);
    struct afs_pagecopy_page *page;

    spin_lock(&task->lock);
    while (!list_empty(&task->copypages)) {
	page = list_entry(task->copypages.next, struct afs_pagecopy_page,
			  tasklink);
	list_del(&page->tasklink);
	spin_unlock(&task->lock);

	if (PageUptodate(page->cachepage)) {
	    copy_highpage(page->afspage, page->cachepage);
	    flush_dcache_page(page->afspage);
	    ClearPageError(page->afspage);
	    SetPageUptodate(page->afspage);
	}
	unlock_page(page->afspage);
	page_cache_release(page->cachepage);
	page_cache_release(page->afspage);
	kfree(page);

	spin_lock(&task->lock);
    }
    spin_unlock(&task->lock);

    afs_pagecopy_put_task(task);
}

static int afs_pagecopy_thread(void *unused) {
    struct page *sleeppage;

    while (!kthread_should_stop()) {
	for (;;) {
	    sleeppage = afs_pagecopy_checkworkload();
	    if (sleeppage) {
		wait_on_page_locked(sleeppage);
		page_cache_release(sleeppage);
	    } else {
		break;
	    }
        }
	wait_event_interruptible(afs_pagecopy_wq,
		   !list_empty(&afs_pagecopy_tasks) || kthread_should_stop());
    }

    return 0;
}

void afs_init_pagecopy(void) {
   spin_lock_init(&afs_pagecopy_lock);
   INIT_LIST_HEAD(&afs_pagecopy_tasks);

   afs_pagecopy_thread_id = kthread_run(afs_pagecopy_thread, NULL,
				        "afs_pagecopy");
}

void afs_shutdown_pagecopy(void) {
   if (afs_pagecopy_thread_id)
	kthread_stop(afs_pagecopy_thread_id);
}

