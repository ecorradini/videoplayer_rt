/*
 * Libdl
 *  (C) Dario Faggioli <raistlin@linux.it>, 2009, 2010
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License (COPYING file) for more details.
 *
 */

#ifndef __DL_SYSCALLS__
#define __DL_SYSCALLS__

#include <linux/kernel.h>
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include <linux/types.h>

#define SCHED_DEADLINE	6

/* XXX use the proper syscall numbers */

/* __NR_sched_setattr number */
#ifndef __NR_sched_setattr
#ifdef __x86_64__
#define __NR_sched_setattr		314
#endif

#ifdef __i386__
#define __NR_sched_setattr		351
#endif

#ifdef __arm__
#define __NR_sched_setattr		380
#endif

#ifdef __aarch64__
#define __NR_sched_setattr		274
#endif
#endif

/* __NR_sched_getattr number */
#ifndef __NR_sched_getattr
#ifdef __x86_64__
#define __NR_sched_getattr		315
#endif

#ifdef __i386__
#define __NR_sched_getattr		352
#endif

#ifdef __arm__
#define __NR_sched_getattr		381
#endif

#ifdef __aarch64__
#define __NR_sched_getattr		275
#endif
#endif

struct sched_attr {
	__u32 size;
	
	__u32 sched_policy;
	__u64 sched_flags;
	
	/* SCHED_NORMAL, SCHED_BATCH */
	__s32 sched_nice;
	
	/* SCHED_FIFO, SCHED_RR */
	__u32 sched_priority;
	
	/* SCHED_DEADLINE */
	__u64 sched_runtime;
	__u64 sched_deadline;
	__u64 sched_period;
};

int sched_setattr(pid_t pid,
		      const struct sched_attr *attr,
		      unsigned int flags);

int sched_getattr(pid_t pid,
		      struct sched_attr *attr,
		      unsigned int size,
		      unsigned int flags);

#endif /* __DL_SYSCALLS__ */

