/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * cpuinfo_kirin9020.c - 伪造 /proc/cpuinfo 的 KPM 模块
 *
 * ============================================================
 * 行为逻辑 (基于 IDA 逆向分析):
 * ============================================================
 *
 * 通过 hook_syscalln 拦截 openat(56) / read(63) / close(57) 系统调用,
 * 对 /proc/cpuinfo 的读取返回伪造的 HiSilicon Kirin 9020 信息.
 *
 * 三个 hook 点:
 *   __NR_openat (56)  → before_openat  + after_openat
 *   __NR_read   (63)  → before_read
 *   __NR_close  (57)  → before_close
 *
 * 数据流:
 *   1. openat("/proc/cpuinfo") → before_openat 检测文件名
 *      → 匹配则设置 args->local.data0 = 1 (标记)
 *   2. after_openat → 检查标记, 将 {task, fd, offset=0} 存入 tracked_fds[]
 *   3. read(fd, buf, n) → before_read 查找 tracked_fds
 *      → 用 compat_copy_to_user 将 fake_cpuinfo 拷贝到用户空间
 *      → 更新 offset, 设置 skip_origin = true
 *   4. close(fd) → before_close 清除 tracked_fds 中的条目
 *
 * 关键: 必须使用 syscall_argn() 访问参数, 不能用 args->argN.
 *       因为 CONFIG_HAVE_SYSCALL_WRAPPERS 内核中 arg0 是 pt_regs *.
 * ============================================================
 */

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

// ============================================================
// 模块元信息
// ============================================================

KPM_NAME("cpuinfo_kirin9020");
KPM_VERSION(CPUINFO_VERSION);
KPM_LICENSE("GPL v2");
KPM_AUTHOR("KPM Template");
KPM_DESCRIPTION("Fake /proc/cpuinfo as HiSilicon Kirin 9020");

// ============================================================
// 全局状态
// ============================================================

static struct cpuinfo_state g_state;

// 伪造的 cpuinfo 内容
static const char fake_cpuinfo[] = FAKE_CPUINFO_CONTENT;
#define FAKE_CPUINFO_SIZE (sizeof(fake_cpuinfo) - 1)

// ============================================================
// 工具函数
// ============================================================

// 在追踪表中查找条目
static int find_tracked_fd(struct task_struct *task, int fd) {
  for (int i = 0; i < MAX_TRACKED_FDS; i++) {
    if (g_state.fds[i].task == task && g_state.fds[i].fd == fd) {
      return i;
    }
  }
  return -1;
}

// 添加追踪条目
static int add_tracked_fd(struct task_struct *task, int fd) {
  int free_slot = -1;

  for (int i = 0; i < MAX_TRACKED_FDS; i++) {
    if (g_state.fds[i].task == task && g_state.fds[i].fd == fd) {
      g_state.fds[i].offset = 0;
      return i;
    }
    if (free_slot < 0 && g_state.fds[i].task == NULL) {
      free_slot = i;
    }
  }

  if (free_slot < 0) return -1;

  g_state.fds[free_slot].task = task;
  g_state.fds[free_slot].fd = fd;
  g_state.fds[free_slot].offset = 0;
  return free_slot;
}

// 移除追踪条目
static void remove_tracked_fd(struct task_struct *task, int fd) {
  for (int i = 0; i < MAX_TRACKED_FDS; i++) {
    if (g_state.fds[i].task == task && g_state.fds[i].fd == fd) {
      g_state.fds[i].task = NULL;
      g_state.fds[i].fd = 0;
      g_state.fds[i].offset = 0;
      return;
    }
  }
}

// 检查文件名是否以 /proc/cpuinfo 或 cpuinfo 结尾
static bool is_cpuinfo_path(const char *filename, int len) {
  if (len <= 0) return false;

  if (len >= TARGET_FILE_FULL_LEN) {
    const char *suffix = filename + len - TARGET_FILE_FULL_LEN;
    if (strncmp(suffix, TARGET_FILE_FULL, TARGET_FILE_FULL_LEN) == 0) {
      return true;
    }
  }

  if (len >= TARGET_FILE_NAME_LEN) {
    const char *suffix = filename + len - TARGET_FILE_NAME_LEN;
    if (strncmp(suffix, TARGET_FILE_NAME, TARGET_FILE_NAME_LEN) == 0) {
      return true;
    }
  }

  return false;
}

// ============================================================
// Hook 回调: __NR_openat (56)
// ============================================================

// 回调签名: void callback(hook_fargs4_t *args, void *udata)
// openat 参数: dfd, filename, flags, mode

static void before_openat(hook_fargs4_t *args, void *udata) {
  if (!g_state.enabled) return;

  // 使用 syscall_argn 访问参数 (兼容 CONFIG_HAVE_SYSCALL_WRAPPERS)
  const char __user *filename = (const char __user *)syscall_argn(args, 1);
  if (!filename) return;

  char buf[128] = {0};
  long ret = compat_strncpy_from_user(buf, filename, sizeof(buf) - 1);
  if (ret <= 0) return;

  buf[127] = '\0';

  if (is_cpuinfo_path(buf, (int)ret)) {
    // 用 local.data0 标记, 在 after 回调中识别
    args->local.data0 = 1;
    pr_info("cpuinfo: intercepted openat(/proc/cpuinfo)\n");
  }
}

// openat 的 after 回调: 在原始函数返回后, 将 fd 加入追踪表
static void after_openat(hook_fargs4_t *args, void *udata) {
  if (!g_state.enabled) return;
  if (!args->local.data0) return;
  args->local.data0 = 0;

  // args->ret 是原始 openat 的返回值 (即 fd)
  long fd = (long)args->ret;
  if (fd < 0) return;

  struct task_struct *task = get_current();
  if (!task) return;

  int idx = add_tracked_fd(task, (int)fd);
  if (idx >= 0) {
    pr_info("cpuinfo: tracked fd=%ld for task=%px\n", fd, task);
  }
}

// ============================================================
// Hook 回调: __NR_read (63)
// ============================================================

// read 参数: fd, buf, count

static void before_read(hook_fargs3_t *args, void *udata) {
  if (!g_state.enabled) return;

  unsigned int fd = (unsigned int)syscall_argn(args, 0);
  struct task_struct *task = get_current();
  if (!task) return;

  int idx = find_tracked_fd(task, (int)fd);
  if (idx < 0) return;

  struct tracked_fd *entry = &g_state.fds[idx];
  unsigned long offset = entry->offset;

  // 已读到文件末尾
  if (offset >= FAKE_CPUINFO_SIZE) {
    args->ret = 0;
    args->skip_origin = true;
    return;
  }

  // 计算本次读取量
  size_t count = (size_t)syscall_argn(args, 2);
  size_t remaining = FAKE_CPUINFO_SIZE - offset;
  size_t to_copy = (count < remaining) ? count : remaining;

  if (to_copy == 0) {
    args->ret = 0;
    args->skip_origin = true;
    return;
  }

  // 将伪造数据拷贝到用户空间
  char __user *buf = (char __user *)syscall_argn(args, 1);
  long copied = compat_copy_to_user(buf, fake_cpuinfo + offset, to_copy);
  if (copied != 0) {
    args->ret = -EFAULT;
    args->skip_origin = true;
    return;
  }

  // 更新偏移量
  entry->offset = offset + to_copy;

  // 设置返回值 = 实际读取字节数, 跳过原始函数
  args->ret = (long)to_copy;
  args->skip_origin = true;

  pr_info("cpuinfo: read fd=%u offset=%lu count=%zu\n", fd, offset, to_copy);
}

// ============================================================
// Hook 回调: __NR_close (57)
// ============================================================

// close 参数: fd

static void before_close(hook_fargs1_t *args, void *udata) {
  if (!g_state.enabled) return;

  unsigned int fd = (unsigned int)syscall_argn(args, 0);
  struct task_struct *task = get_current();
  if (!task) return;

  int idx = find_tracked_fd(task, (int)fd);
  if (idx >= 0) {
    remove_tracked_fd(task, (int)fd);
    pr_info("cpuinfo: untracked fd=%u\n", fd);
  }
}

// ============================================================
// KPM 生命周期
// ============================================================

static long inline_hook_init(const char *args, const char *event,
                              void *__user reserved) {
  pr_info("cpuinfo_kirin9020: version %s initializing...\n", CPUINFO_VERSION);

  // 初始化全局状态
  memset(&g_state, 0, sizeof(g_state));
  g_state.enabled = 1;

  // 安装 hook (按系统调用号, 自动选择 inline/fp 策略)
  hook_err_t err;

  err = hook_syscalln(__NR_openat, 4, before_openat, after_openat, NULL);
  if (err) {
    pr_err("cpuinfo: hook openat failed: %d\n", err);
    return -1;
  }
  pr_info("cpuinfo: hook openat success\n");

  err = hook_syscalln(__NR_read, 3, before_read, NULL, NULL);
  if (err) {
    pr_err("cpuinfo: hook read failed: %d\n", err);
    unhook_syscalln(__NR_openat, before_openat, after_openat);
    return -1;
  }
  pr_info("cpuinfo: hook read success\n");

  err = hook_syscalln(__NR_close, 1, before_close, NULL, NULL);
  if (err) {
    pr_err("cpuinfo: hook close failed: %d\n", err);
    unhook_syscalln(__NR_openat, before_openat, after_openat);
    unhook_syscalln(__NR_read, before_read, NULL);
    return -1;
  }
  pr_info("cpuinfo: hook close success\n");

  pr_info("cpuinfo_kirin9020: init complete! fake CPU: HiSilicon Kirin 9020\n");
  return 0;
}

// 运行时控制
static long inline_hook_control0(const char *args, char *__user out_msg,
                                  int out_msg_len) {
  if (!args) return -1;

  if (strcmp(args, "status") == 0) {
    const char *msg = g_state.enabled ? "enabled" : "disabled";
    if (out_msg && out_msg_len > 0) {
      long len = strlen(msg);
      if (len > out_msg_len) len = out_msg_len;
      compat_copy_to_user(out_msg, msg, len);
    }
    pr_info("cpuinfo: module is %s\n", msg);
  } else if (strcmp(args, "enable") == 0) {
    g_state.enabled = 1;
    pr_info("cpuinfo: module enabled\n");
  } else if (strcmp(args, "disable") == 0) {
    g_state.enabled = 0;
    pr_info("cpuinfo: module disabled\n");
  }

  return 0;
}

// 模块清理
static long inline_hook_exit(void *__user reserved) {
  pr_info("cpuinfo_kirin9020: cleaning up...\n");

  unhook_syscalln(__NR_openat, before_openat, after_openat);
  unhook_syscalln(__NR_read, before_read, NULL);
  unhook_syscalln(__NR_close, before_close, NULL);

  memset(&g_state, 0, sizeof(g_state));

  pr_info("cpuinfo_kirin9020: cleanup complete!\n");
  return 0;
}

// 注册生命周期
KPM_INIT(inline_hook_init);
KPM_CTL0(inline_hook_control0);
KPM_EXIT(inline_hook_exit);