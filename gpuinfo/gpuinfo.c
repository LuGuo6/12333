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

#include "gpuinfo.h"

#ifndef GPUINFO_VERSION
#define GPUINFO_VERSION "1.0.0"
#endif

KPM_NAME("gpuinfo");
KPM_VERSION(GPUINFO_VERSION);
KPM_LICENSE("GPL v2");
KPM_AUTHOR("");
KPM_DESCRIPTION("GPU & kernel version faker for Adreno & Mali");

static int module_enabled;

// ============================================================
// 路径匹配表 — 在此定义所有要伪造的 GPU sysfs 路径
// ============================================================

// Adreno 750 (骁龙 8 Gen3)
static const struct gpu_path_entry adreno750_paths[] = {
    { "/sys/class/kgsl/kgsl-3d0/gpu_model",
      sizeof("/sys/class/kgsl/kgsl-3d0/gpu_model") - 1,
      ADRENO_750_MODEL, ADRENO_750_MODEL_SIZE },
    { "/sys/class/kgsl/kgsl-3d0/gpu_id",
      sizeof("/sys/class/kgsl/kgsl-3d0/gpu_id") - 1,
      ADRENO_750_ID, ADRENO_750_ID_SIZE },
    { "/sys/class/kgsl/kgsl-3d0/max_gpuclk",
      sizeof("/sys/class/kgsl/kgsl-3d0/max_gpuclk") - 1,
      ADRENO_750_MAX_GPUCLK, ADRENO_750_MAX_GPUCLK_SIZE },
    { "/sys/class/kgsl/kgsl-3d0/freq_table_mhz",
      sizeof("/sys/class/kgsl/kgsl-3d0/freq_table_mhz") - 1,
      ADRENO_750_FREQ_TABLE, ADRENO_750_FREQ_TABLE_SIZE },
    { NULL, 0, NULL, 0 }
};

// Mali-G720 (天玑 9300)
static const struct gpu_path_entry mali_g720_paths[] = {
    { "/sys/class/misc/mali0/device/gpuinfo",
      sizeof("/sys/class/misc/mali0/device/gpuinfo") - 1,
      MALI_G720_INFO, MALI_G720_INFO_SIZE },
    { "/sys/class/misc/mali0/device/gpu_model",
      sizeof("/sys/class/misc/mali0/device/gpu_model") - 1,
      MALI_G720_MODEL, MALI_G720_MODEL_SIZE },
    { "/sys/class/misc/mali/device/gpuinfo",
      sizeof("/sys/class/misc/mali/device/gpuinfo") - 1,
      MALI_G720_INFO, MALI_G720_INFO_SIZE },
    { "/sys/class/misc/mali/device/gpu_model",
      sizeof("/sys/class/misc/mali/device/gpu_model") - 1,
      MALI_G720_MODEL, MALI_G720_MODEL_SIZE },
    { NULL, 0, NULL, 0 }
};

// /proc/version (内核版本)
static const struct gpu_path_entry proc_paths[] = {
    { "/proc/version",
      sizeof("/proc/version") - 1,
      FAKE_PROC_VERSION, FAKE_PROC_VERSION_SIZE },
    { NULL, 0, NULL, 0 }
};

// 所有路径表（模块加载时按顺序尝试匹配）
static const struct gpu_path_entry *all_gpu_paths[] = {
    adreno750_paths,
    mali_g720_paths,
    proc_paths,
    NULL
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

// 在路径表中查找匹配，返回对应的 fake_data 和 fake_size
// 匹配失败返回 NULL
static const struct gpu_path_entry *find_gpu_path(const char *buf, int buf_len)
{
    const struct gpu_path_entry *entry;
    int t;

    for (t = 0; all_gpu_paths[t] != NULL; t++) {
        for (entry = all_gpu_paths[t]; entry->path != NULL; entry++) {
            if (buf_len >= entry->path_len &&
                strncmp(buf, entry->path, entry->path_len) == 0) {
                return entry;
            }
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
    const struct gpu_path_entry *entry;

    filename = (const char *)syscall_argn(args, 1);
    args->local.data0 = 0;
    buf[0] = 0;

    ret = compat_strncpy_from_user(buf, filename, MAX_PATH_LEN);
    if (ret <= 0)
        return;

    buf[MAX_PATH_LEN - 1] = 0;
    if (!buf[0])
        return;

    entry = find_gpu_path(buf, (int)ret);
    if (entry && module_enabled) {
        args->local.data0 = 1;
        // 将 fake_data 指针暂存到 local.data1（after_openat 中使用）
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

    // 如果已有记录，更新 fake_data
    for (i = 0; i < MAX_TRACKED_FDS; i++) {
        if (tracked_fds[i].task == task && tracked_fds[i].fd == (int)fd) {
            tracked_fds[i].offset = 0;
            tracked_fds[i].fake_data = (const char *)args->local.data1;
            tracked_fds[i].fake_size = (int)args->local.data2;
            return;
        }
    }

    // 新建记录
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
// uname 系统调用 hook — 伪造内核版本
// ============================================================

// struct utsname 布局 (ARM64, 每个字段 65 字节):
// [0]   sysname    "Linux"
// [65]  nodename   hostname
// [130] release    "6.1.75-android14-11-gc52c7e3e6"  ← 修改这个
// [195] version    "#1 SMP PREEMPT ..."
// [260] machine    "aarch64"
// [325] domainname ""
#define UTSNAME_RELEASE_OFFSET  130

static void after_uname(hook_fargs1_t *args, void *udata)
{
    char __user *buf;

    if (!module_enabled)
        return;

    // 原始 uname 已执行完毕，覆盖 release 字段
    buf = (char __user *)syscall_argn(args, 0);
    compat_copy_to_user(buf + UTSNAME_RELEASE_OFFSET,
                        FAKE_KERNEL_VERSION, FAKE_KERNEL_VERSION_LEN);
}

// ============================================================
// KPM 生命周期函数
// ============================================================

static long gpuinfo_init(const char *args, const char *event, void *__user reserved)
{
    unsigned int err;

    printk("\0016gpuinfo init, event=%s args=%s\n", event, args);

    module_enabled = 1;

    err = hook_syscalln(__NR_openat, 4, before_openat, after_openat, 0);
    if (err)
        printk("\0013gpuinfo hook openat error: %d\n", err);

    err = hook_syscalln(__NR_read, 3, before_read, 0, 0);
    if (err)
        printk("\0013gpuinfo hook read error: %d\n", err);

    err = hook_syscalln(__NR_close, 1, before_close, 0, 0);
    if (err)
        printk("\0013gpuinfo hook close error: %d\n", err);

    err = hook_syscalln(__NR_uname, 1, 0, after_uname, 0);
    if (err)
        printk("\0013gpuinfo hook uname error: %d\n", err);

    printk("\0016gpuinfo init complete\n");

    return 0;
}

static long gpuinfo_exit(void *__user reserved)
{
    unhook_syscalln(__NR_openat, before_openat, after_openat);
    unhook_syscalln(__NR_read, before_read, 0);
    unhook_syscalln(__NR_close, before_close, 0);
    unhook_syscalln(__NR_uname, 0, after_uname);

    printk("\0016gpuinfo exit\n");

    return 0;
}

static long gpuinfo_ctl0(const char *args, char *__user out_msg, int out_msg_len)
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

KPM_INIT(gpuinfo_init);
KPM_CTL0(gpuinfo_ctl0);
KPM_EXIT(gpuinfo_exit);
