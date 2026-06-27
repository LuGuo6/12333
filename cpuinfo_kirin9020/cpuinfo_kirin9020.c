/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <compiler.h>
#include <hook.h>
#include <kpmodule.h>
#include <kputils.h>
#include <syscall.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/errno.h>
#include <uapi/asm-generic/unistd.h>
#include <asm/current.h>

#include "cpuinfo_kirin9020.h"

#ifndef CPUINFO_VERSION
#define CPUINFO_VERSION "1.0.0"
#endif

KPM_NAME("cpuinfo_kirin9030");
KPM_VERSION(CPUINFO_VERSION);
KPM_LICENSE("GPL v2");
KPM_AUTHOR("路过");
KPM_DESCRIPTION("cpuinfo_kirin9030_By_PassBy");

static int module_enabled;

struct tracked_fd {
    void *task;
    int fd;
    unsigned long offset;
};

static struct tracked_fd tracked_fds[MAX_TRACKED_FDS];

static const char fake_cpuinfo[] = FAKE_CPUINFO_CONTENT;

static void before_openat(hook_fargs4_t *args, void *udata)
{
    char buf[128];
    const char *filename;
    long ret;

    filename = (const char *)syscall_argn(args, 1);
    args->local.data0 = 0;
    buf[0] = 0;

    ret = compat_strncpy_from_user(buf, filename, 128);
    if (ret <= 0)
        return;

    buf[127] = 0;
    if (!buf[0])
        return;

    if (strncmp(buf, "/proc/cpuinfo", 13) == 0 || strncmp(buf, "cpuinfo", 7) == 0) {
        if (module_enabled) {
            args->local.data0 = 1;
        }
    }
}

static void after_openat(hook_fargs4_t *args, void *udata)
{
    struct task_struct *task;
    long fd;
    int i;

    if (!args->local.data0)
        return;

    fd = (long)args->ret;
    if (fd < 0)
        return;

    task = get_current();

    for (i = 0; i < MAX_TRACKED_FDS; i++) {
        if (tracked_fds[i].task == task && tracked_fds[i].fd == (int)fd) {
            tracked_fds[i].offset = 0;
            return;
        }
    }

    for (i = 0; i < MAX_TRACKED_FDS; i++) {
        if (!tracked_fds[i].task) {
            tracked_fds[i].task = task;
            tracked_fds[i].fd = (int)fd;
            tracked_fds[i].offset = 0;
            return;
        }
    }
}

static void before_read(hook_fargs3_t *args, void *udata)
{
    struct task_struct *task;
    unsigned int fd;
    unsigned long offset;
    unsigned long count;
    unsigned long remaining;
    unsigned long to_copy;
    char __user *buf;
    long copied;
    int i;

    fd = (unsigned int)syscall_argn(args, 0);
    task = get_current();

    for (i = 0; i < MAX_TRACKED_FDS; i++) {
        if (tracked_fds[i].task == task && tracked_fds[i].fd == (int)fd)
            break;
    }
    if (i == MAX_TRACKED_FDS)
        return;

    if (!module_enabled)
        return;

    offset = tracked_fds[i].offset;
    if (offset >= FAKE_CPUINFO_SIZE) {
        args->skip_origin = true;
        args->ret = 0;
        return;
    }

    remaining = FAKE_CPUINFO_SIZE - offset;
    count = (unsigned long)syscall_argn(args, 2);
    to_copy = count < remaining ? count : remaining;

    if (!to_copy) {
        args->skip_origin = true;
        args->ret = 0;
        return;
    }

    buf = (char __user *)syscall_argn(args, 1);
    copied = compat_copy_to_user(buf, fake_cpuinfo + offset, to_copy);
    if (copied > 0) {
        tracked_fds[i].offset = offset + to_copy;
        args->skip_origin = true;
        args->ret = (long)to_copy;
    }
}

static void before_close(hook_fargs1_t *args, void *udata)
{
    struct task_struct *task;
    unsigned int fd;
    int i;

    fd = (unsigned int)syscall_argn(args, 0);
    task = get_current();

    for (i = 0; i < MAX_TRACKED_FDS; i++) {
        if (tracked_fds[i].task == task && tracked_fds[i].fd == (int)fd) {
            tracked_fds[i].task = 0;
            tracked_fds[i].fd = 0;
            tracked_fds[i].offset = 0;
            return;
        }
    }
}

static long cpuinfo_init(const char *args, const char *event, void *__user reserved)
{
    unsigned int err;

    printk("\0016cpuinfo Kirin9030 init, event=%s args=%s\n", event, args);

    module_enabled = 1;

    err = hook_syscalln(__NR_openat, 4, before_openat, after_openat, 0);
    if (err)
        printk("\0013cpuinfo hook openat error: %d\n", err);

    err = hook_syscalln(__NR_read, 3, before_read, 0, 0);
    if (err)
        printk("\0013cpuinfo hook read error: %d\n", err);

    err = hook_syscalln(__NR_close, 1, before_close, 0, 0);
    if (err)
        printk("\0013cpuinfo hook close error: %d\n", err);

    return 0;
}

static long cpuinfo_exit(void *__user reserved)
{
    unhook_syscalln(__NR_openat, before_openat, after_openat);
    unhook_syscalln(__NR_read, before_read, 0);
    unhook_syscalln(__NR_close, before_close, 0);

    printk("\0016cpuinfo Kirin9030 exit\n");

    return 0;
}

static long cpuinfo_ctl0(const char *args, char *__user out_msg, int out_msg_len)
{
    int n;

    if (!out_msg || out_msg_len <= 0)
        return 0;

    n = out_msg_len;
    if (n > 8)
        n = 8;

    compat_copy_to_user(out_msg, "enabled", n);

    return 0;
}

KPM_INIT(cpuinfo_init);
KPM_CTL0(cpuinfo_ctl0);
KPM_EXIT(cpuinfo_exit);