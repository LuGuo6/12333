/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * cpuinfo_kirin9020.h - 模块头文件
 *
 * 功能: 伪造 /proc/cpuinfo, 将 CPU 信息伪装为 HiSilicon Kirin 9020
 * 原理: Hook openat/read/close 三个系统调用, 拦截对 /proc/cpuinfo 的读写
 */
#ifndef _CPUINFO_KIRIN9020_H
#define _CPUINFO_KIRIN9020_H

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/uaccess.h>

// ============================================================
// 常量定义
// ============================================================

// 最大追踪的文件描述符数量
#define MAX_TRACKED_FDS 64

// 要伪装的目标文件路径
#define TARGET_FILE_FULL "/proc/cpuinfo"
#define TARGET_FILE_NAME "cpuinfo"
#define TARGET_FILE_FULL_LEN 13
#define TARGET_FILE_NAME_LEN 7

// ARM64 系统调用号
#define __NR_openat 56
#define __NR_close  57
#define __NR_read   63

// 伪造的 /proc/cpuinfo 内容
// 显示为 HiSilicon Kirin 9020, 8核 (1+3+4 三丛集架构)
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

// 追踪的 fd 条目
// 每个条目追踪一个被打开的 /proc/cpuinfo 文件描述符
struct tracked_fd {
  void *task;           // 打开该 fd 的 task_struct 指针
  int fd;               // 文件描述符编号
  unsigned long offset; // 当前读取偏移量
};

// 全局状态
struct cpuinfo_state {
  int enabled;                          // 模块是否启用
  struct tracked_fd fds[MAX_TRACKED_FDS]; // fd 追踪表
};

// ============================================================
// 函数声明
// ============================================================

// 获取当前 task_struct 指针
static inline struct task_struct *get_current_task(void);

// 在追踪表中查找条目
static int find_tracked_fd(struct task_struct *task, int fd);

// 添加追踪条目
static int add_tracked_fd(struct task_struct *task, int fd);

// 移除追踪条目
static void remove_tracked_fd(struct task_struct *task, int fd);

#endif /* _CPUINFO_KIRIN9020_H */