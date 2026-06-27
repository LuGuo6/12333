/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * cpuinfo_kirin9020.h - 模块头文件
 *
 * 功能: 伪造 /proc/cpuinfo, 将 CPU 信息伪装为 HiSilicon Kirin 9020
 * 原理: Hook __arm64_sys_openat / __arm64_sys_read / __arm64_sys_close
 *       拦截对 /proc/cpuinfo 的读写, 返回伪造内容
 */
#ifndef _CPUINFO_KIRIN9020_H
#define _CPUINFO_KIRIN9020_H

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/uaccess.h>
#include <linux/printk.h>
#include <linux/errno.h>

// ============================================================
// 常量
// ============================================================

#define MAX_TRACKED_FDS      64
#define TARGET_FILE_FULL     "/proc/cpuinfo"
#define TARGET_FILE_NAME     "cpuinfo"
#define TARGET_FILE_FULL_LEN 13
#define TARGET_FILE_NAME_LEN 7

// ============================================================
// 伪造的 /proc/cpuinfo 内容
// ============================================================

#define FAKE_CPUINFO_CONTENT \
  "Processor\t: AArch64 Processor rev 0 (aarch64)\n" \
  "Features\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture\t: 8\n" \
  "CPU variant\t: 0x1\n" \
  "CPU part\t: 0xd0c\n" \
  "CPU revision\t: 0\n" \
  "\n" \
  "processor\t: 0\n" \
  "BogoMIPS\t: 26.00\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture\t: 8\n" \
  "CPU variant\t: 0x1\n" \
  "CPU part\t: 0xd0c\n" \
  "CPU revision\t: 0\n" \
  "\n" \
  "processor\t: 1\n" \
  "BogoMIPS\t: 26.00\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture\t: 8\n" \
  "CPU variant\t: 0x2\n" \
  "CPU part\t: 0xd0a\n" \
  "CPU revision\t: 0\n" \
  "\n" \
  "processor\t: 2\n" \
  "BogoMIPS\t: 26.00\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture\t: 8\n" \
  "CPU variant\t: 0x2\n" \
  "CPU part\t: 0xd0a\n" \
  "CPU revision\t: 0\n" \
  "\n" \
  "processor\t: 3\n" \
  "BogoMIPS\t: 26.00\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture\t: 8\n" \
  "CPU variant\t: 0x2\n" \
  "CPU part\t: 0xd0a\n" \
  "CPU revision\t: 0\n" \
  "\n" \
  "processor\t: 4\n" \
  "BogoMIPS\t: 26.00\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture\t: 8\n" \
  "CPU variant\t: 0x3\n" \
  "CPU part\t: 0xd0b\n" \
  "CPU revision\t: 0\n" \
  "\n" \
  "processor\t: 5\n" \
  "BogoMIPS\t: 26.00\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture\t: 8\n" \
  "CPU variant\t: 0x3\n" \
  "CPU part\t: 0xd0b\n" \
  "CPU revision\t: 0\n" \
  "\n" \
  "processor\t: 6\n" \
  "BogoMIPS\t: 26.00\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture\t: 8\n" \
  "CPU variant\t: 0x3\n" \
  "CPU part\t: 0xd0b\n" \
  "CPU revision\t: 0\n" \
  "\n" \
  "processor\t: 7\n" \
  "BogoMIPS\t: 26.00\n" \
  "CPU implementer\t: 0x48\n" \
  "CPU architecture\t: 8\n" \
  "CPU variant\t: 0x3\n" \
  "CPU part\t: 0xd0b\n" \
  "CPU revision\t: 0\n" \
  "\n" \
  "Hardware\t: HiSilicon Kirin 9020\n"

// ============================================================
// 数据结构
// ============================================================

struct tracked_fd {
  void *task;
  int fd;
  unsigned long offset;
};

struct cpuinfo_state {
  int enabled;
  struct tracked_fd fds[MAX_TRACKED_FDS];
};

// ============================================================
// lookup_name 宏 (通过 kallsyms 查找内核函数)
// ============================================================

#define lookup_name(func)                                  \
  func = 0;                                                \
  func = (typeof(func))kallsyms_lookup_name(#func);        \
  pr_info("kernel function %s addr: %px\n", #func, func);  \
  if (!func) {                                             \
    return -21;                                            \
  }

// ============================================================
// hook_func / unhook_func 宏 (与参考项目一致)
// ============================================================

#define hook_func(func, argv, before, after, udata)                       \
  if (!func) {                                                            \
    return -22;                                                           \
  }                                                                       \
  hook_err_t hook_err_##func = hook_wrap(func, argv, before, after, udata); \
  if (hook_err_##func) {                                                  \
    pr_err("hook %s error: %d\n", #func, hook_err_##func);                \
    return -23;                                                           \
  } else {                                                                \
    pr_info("hook %s success\n", #func);                                  \
  }

#define unhook_func(func)           \
  if (func && !is_bad_address(func)) \
    unhook(func);

#endif /* _CPUINFO_KIRIN9020_H */