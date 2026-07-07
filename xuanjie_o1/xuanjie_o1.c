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

#include "xuanjie_o1.h"

#ifndef XUANJIE_VERSION
#define XUANJIE_VERSION "1.0.0"
#endif

KPM_NAME("xuanjie_o1");
KPM_VERSION(XUANJIE_VERSION);
KPM_LICENSE("GPL v2");
KPM_AUTHOR("");
KPM_DESCRIPTION("Xiaomi XuanJie O1 faker (CPU + GPU + Kernel)");

static int module_enabled;

// ============================================================
// 路径匹配表 — CPU / GPU / 内核版本
// ============================================================

static const struct path_entry all_paths[] = {
    // /proc/cpuinfo — CPU 信息
    { "/proc/cpuinfo",
      sizeof("/proc/cpuinfo") - 1,
      FAKE_CPUINFO_CONTENT, FAKE_CPUINFO_SIZE },

    // /proc/version — 内核版本
    { "/proc/version",
      sizeof("/proc/version") - 1,
      FAKE_PROC_VERSION, FAKE_PROC_VERSION_SIZE },

    // ARM Mali GPU — Immortalis-G925
    { "/sys/class/misc/mali0/device/gpuinfo",
      sizeof("/sys/class/misc/mali0/device/gpuinfo") - 1,
      FAKE_GPU_INFO, FAKE_GPU_INFO_SIZE },
    { "/sys/class/misc/mali0/device/gpu_model",
      sizeof("/sys/class/misc/mali0/device/gpu_model") - 1,
      FAKE_GPU_INFO, FAKE_GPU_INFO_SIZE },
    { "/sys/class/misc/mali/device/gpuinfo",
      sizeof("/sys/class/misc/mali/device/gpuinfo") - 1,
      FAKE_GPU_INFO, FAKE_GPU_INFO_SIZE },
    { "/sys/class/misc/mali/device/gpu_model",
      sizeof("/sys/class/misc/mali/device/gpu_model") - 1,
      FAKE_GPU_INFO, FAKE_GPU_INFO_SIZE },

    { NULL, 0, NULL, 0 }
};

// ============================================================
// tracked fd 管理
// ============================================================

struct tracked_fd {
    void *task;
    int fd;
    unsigned long offset;
    const char *fake_data;
    int fake_size;
};

static struct tracked_fd tracked_fds[MAX_TRACKED_FDS];

// ============================================================
// 路径匹配
// ============================================================

static const struct path_entry *find_path(const char *buf, int buf_len)
{
    const struct path_entry *entry;

    for (entry = all_paths; entry->path != NULL; entry++) {
        if (buf_len >= entry->path_len &&
            strncmp(buf, entry->path, entry->path_len) == 0) {
            return entry;
        }
    }
    return NULL;
}

// ============================================================
// Hook 回调函数
// ============================================================

static void before_openat(hook_fargs4_t *args, void *udata)
{
    char buf[MAX_PATH_LEN];
    const char *filename;
    long ret;
    const struct path_entry *entry;

    filename = (const char *)syscall_argn(args, 1);
    args->local.data0 = 0;
    buf[0] = 0;

    ret = compat_strncpy_from_user(buf, filename, MAX_PATH_LEN);
    if (ret <= 0)
        return;

    buf[MAX_PATH_LEN - 1] = 0;
    if (!buf[0])
        return;

    entry = find_path(buf, (int)ret);
    if (entry && module_enabled) {
        args->local.data0 = 1;
        args->local.data1 = (long)entry->fake_data;
        args->local.data2 = entry->fake_size;
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
            tracked_fds[i].fake_data = (const char *)args->local.data1;
            tracked_fds[i].fake_size = (int)args->local.data2;
            return;
        }
    }

    for (i = 0; i < MAX_TRACKED_FDS; i++) {
        if (!tracked_fds[i].task) {
            tracked_fds[i].task = task;
            tracked_fds[i].fd = (int)fd;
            tracked_fds[i].offset = 0;
            tracked_fds[i].fake_data = (const char *)args->local.data1;
            tracked_fds[i].fake_size = (int)args->local.data2;
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
    if (offset >= (unsigned long)tracked_fds[i].fake_size) {
        args->skip_origin = true;
        args->ret = 0;
        return;
    }

    remaining = tracked_fds[i].fake_size - offset;
    count = (unsigned long)syscall_argn(args, 2);
    to_copy = count < remaining ? count : remaining;

    if (!to_copy) {
        args->skip_origin = true;
        args->ret = 0;
        return;
    }

    buf = (char __user *)syscall_argn(args, 1);
    copied = compat_copy_to_user(buf, tracked_fds[i].fake_data + offset, to_copy);
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
            tracked_fds[i].fake_data = NULL;
            tracked_fds[i].fake_size = 0;
            return;
        }
    }
}

// ============================================================
// uname hook — 伪造内核版本
// ============================================================

// struct utsname: [0]sysname [65]nodename [130]release [195]version [260]machine [325]domainname
#define UTSNAME_RELEASE_OFFSET  130

static void after_uname(hook_fargs1_t *args, void *udata)
{
    char __user *buf;

    if (!module_enabled)
        return;

    buf = (char __user *)syscall_argn(args, 0);
    compat_copy_to_user(buf + UTSNAME_RELEASE_OFFSET,
                        FAKE_KERNEL_VERSION, FAKE_KERNEL_VERSION_LEN);
}

// ============================================================
// KPM 生命周期函数
// ============================================================

static long xuanjie_init(const char *args, const char *event, void *__user reserved)
{
    unsigned int err;

    printk("\0016xuanjie_o1 init, event=%s args=%s\n", event, args);

    module_enabled = 1;

    err = hook_syscalln(__NR_openat, 4, before_openat, after_openat, 0);
    if (err)
        printk("\0013xuanjie_o1 hook openat error: %d\n", err);

    err = hook_syscalln(__NR_read, 3, before_read, 0, 0);
    if (err)
        printk("\0013xuanjie_o1 hook read error: %d\n", err);

    err = hook_syscalln(__NR_close, 1, before_close, 0, 0);
    if (err)
        printk("\0013xuanjie_o1 hook close error: %d\n", err);

    err = hook_syscalln(__NR_uname, 1, 0, after_uname, 0);
    if (err)
        printk("\0013xuanjie_o1 hook uname error: %d\n", err);

    printk("\0016xuanjie_o1 init complete\n");

    return 0;
}

static long xuanjie_exit(void *__user reserved)
{
    unhook_syscalln(__NR_openat, before_openat, after_openat);
    unhook_syscalln(__NR_read, before_read, 0);
    unhook_syscalln(__NR_close, before_close, 0);
    unhook_syscalln(__NR_uname, 0, after_uname);

    printk("\0016xuanjie_o1 exit\n");

    return 0;
}

static long xuanjie_ctl0(const char *args, char *__user out_msg, int out_msg_len)
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

KPM_INIT(xuanjie_init);
KPM_CTL0(xuanjie_ctl0);
KPM_EXIT(xuanjie_exit);
