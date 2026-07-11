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
KPM_DESCRIPTION("Xiaomi XuanJie O1 faker (CPU + GPU)");

static int module_enabled;

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
// GPU 路径匹配 — 最大范围兼容
//
// 不同设备的 GPU sysfs 路径差异巨大：
//   小米: /sys/kernel/gpu/gpu_model
//   一加/OPPO: /sys/class/kgsl/kgsl-3d0/gpu_model
//   天玑: /sys/class/misc/mali0/device/gpuinfo
//   其他: /sys/devices/platform/.../gpu_model
//
// 策略：路径中只要包含 "gpu" 且是 sysfs/procfs 文件，
// 就返回伪造数据。宁可多拦截，不能漏掉。
// ============================================================

static int is_gpu_path(const char *path, int len)
{
    int i;

    // 必须是 /sys/ 或 /proc/ 下的文件
    if (len < 5)
        return 0;
    if (strncmp(path, "/sys/", 5) != 0 && strncmp(path, "/proc/", 6) != 0)
        return 0;

    // 路径中必须包含 "gpu"（不区分位置）
    for (i = 0; i < len - 2; i++) {
        if (path[i] == 'g' && path[i+1] == 'p' && path[i+2] == 'u')
            return 1;
    }

    return 0;
}

// ============================================================
// CPU 路径精确匹配
// ============================================================

static int is_cpuinfo_path(const char *path, int len)
{
    // 精确匹配 /proc/cpuinfo
    if (len == 14 && strncmp(path, "/proc/cpuinfo", 14) == 0)
        return 1;
    // 也匹配相对路径 "cpuinfo" (部分应用)
    if (len == 7 && strncmp(path, "cpuinfo", 7) == 0)
        return 1;
    return 0;
}

// ============================================================
// 路径分发 — 根据匹配结果返回对应的伪造数据
// ============================================================

static const char *match_fake_data(const char *path, int path_len, int *out_size)
{
    if (is_cpuinfo_path(path, path_len)) {
        *out_size = FAKE_CPUINFO_SIZE;
        return FAKE_CPUINFO_CONTENT;
    }

    if (is_gpu_path(path, path_len)) {
        *out_size = FAKE_GPU_MODEL_SIZE;
        return FAKE_GPU_MODEL;
    }

    *out_size = 0;
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
    const char *fake;
    int fake_size;

    filename = (const char *)syscall_argn(args, 1);
    args->local.data0 = 0;
    buf[0] = 0;

    ret = compat_strncpy_from_user(buf, filename, MAX_PATH_LEN);
    if (ret <= 0)
        return;

    buf[MAX_PATH_LEN - 1] = 0;
    if (!buf[0])
        return;

    fake = match_fake_data(buf, (int)ret, &fake_size);
    // 同时追踪 GPU 设备文件 (/dev/kgsl-3d0, /dev/mali0)
    if (!fake && ret > 5 && strncmp(buf, "/dev/", 5) == 0 &&
        (strstr(buf, "kgsl") || strstr(buf, "mali") || strstr(buf, "gpu"))) {
        fake = "";  // 空字符串标记为 GPU fd
        fake_size = 0;
    }
    if (fake && module_enabled) {
        args->local.data0 = 1;
        args->local.data1 = (long)fake;
        args->local.data2 = fake_size;
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
// ioctl hook — 拦截 GPU 驱动属性查询，伪造 glGetString 结果
//
// glGetString(GL_RENDERER) 最终调用:
//   ioctl(fd, KGSL_IOCTL_DEVICE_GETPROPERTY, &prop)
//
// KGSL ioctl 命令号格式: (type << 8) | nr
// type = 0x09 (KGSL magic number)
// ============================================================

// KGSL ioctl magic number
#define KGSL_IOC_TYPE  0x09

// 检查 ioctl 命令是否是 KGSL GPU 命令
static inline int is_kgsl_ioctl(unsigned int cmd)
{
    return ((cmd >> 8) & 0xFF) == KGSL_IOC_TYPE;
}

// 伪造的 GPU 信息
static const char fake_renderer[] = "Immortalis-G925";
static const char fake_vendor[] = "ARM";

// ioctl before 回调：拦截 GPU 属性查询
static void before_ioctl(hook_fargs3_t *args, void *udata)
{
    unsigned int fd;
    unsigned int cmd;
    struct task_struct *task;
    int i;

    if (!module_enabled)
        return;

    fd = (unsigned int)syscall_argn(args, 0);
    cmd = (unsigned int)syscall_argn(args, 1);

    // 只处理 KGSL ioctl
    if (!is_kgsl_ioctl(cmd))
        return;

    // 检查是否是已追踪的 GPU fd
    task = get_current();
    for (i = 0; i < MAX_TRACKED_FDS; i++) {
        if (tracked_fds[i].task == task && tracked_fds[i].fd == (int)fd)
            break;
    }
    if (i == MAX_TRACKED_FDS)
        return;

    // 让原始 ioctl 执行，我们在 after 中修改返回数据
    args->local.data0 = 1;
}

// ioctl after 回调：修改 GPU 属性返回值
static void after_ioctl(hook_fargs3_t *args, void *udata)
{
    unsigned long arg_ptr;
    char kbuf[256];
    long ret;
    int i;

    if (!args->local.data0 || args->ret)
        return;

    arg_ptr = syscall_argn(args, 2);
    if (!arg_ptr)
        return;

    // 从用户空间读取 ioctl 参数
    ret = compat_strncpy_from_user(kbuf, (const char __user *)arg_ptr, 256);
    if (ret <= 0)
        return;

    kbuf[255] = 0;

    // 搜索返回数据中的 GPU 字符串并替换
    for (i = 0; i < 256 - 15; i++) {
        // 替换 "Adreno" 开头的字符串为 "Immortalis-G925"
        if (kbuf[i] == 'A' && kbuf[i+1] == 'd' && kbuf[i+2] == 'r' &&
            kbuf[i+3] == 'e' && kbuf[i+4] == 'n' && kbuf[i+5] == 'o') {
            memset(kbuf + i, 0, 20);
            memcpy(kbuf + i, fake_renderer, sizeof(fake_renderer) - 1);
            break;
        }
    }

    // 写回用户空间
    compat_copy_to_user((void __user *)arg_ptr, kbuf, 256);
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

    err = hook_syscalln(__NR_ioctl, 3, before_ioctl, after_ioctl, 0);
    if (err)
        printk("\0013xuanjie_o1 hook ioctl error: %d\n", err);

    printk("\0016xuanjie_o1 init complete\n");

    return 0;
}

static long xuanjie_exit(void *__user reserved)
{
    unhook_syscalln(__NR_openat, before_openat, after_openat);
    unhook_syscalln(__NR_read, before_read, 0);
    unhook_syscalln(__NR_close, before_close, 0);
    unhook_syscalln(__NR_ioctl, before_ioctl, after_ioctl);

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
