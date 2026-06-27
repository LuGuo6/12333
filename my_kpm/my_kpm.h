/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * my_kpm.h - 模块内部头文件
 * 声明模块私有的结构体、偏移量、函数原型
 */
#ifndef _MY_KPM_H
#define _MY_KPM_H

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/version.h>

// ============================================================
// 你需要声明的内核结构体偏移量
// ============================================================

// 示例: task_struct 关键字段偏移量
struct task_struct_offset {
  int cred_offset;       // task_struct->cred
  int pid_offset;        // task_struct->pid
  int tgid_offset;       // task_struct->tgid
  int comm_offset;       // task_struct->comm
  int jobctl_offset;     // task_struct->jobctl
  int signal_offset;     // task_struct->signal
};

// 示例: cred 结构体偏移量
struct cred_offset {
  int uid_offset;
  int gid_offset;
  int euid_offset;
  int egid_offset;
  int suid_offset;
  int sgid_offset;
};

// 全局偏移量变量(在 .c 中定义)
extern struct task_struct_offset task_struct_offset;
extern struct cred_offset cred_offset;

// ============================================================
// 模块参数
// ============================================================

// 可通过 KPM 控制接口修改的参数
#define MY_KPM_PARAM_DEFAULT 0

// ============================================================
// 函数声明
// ============================================================

// 计算偏移量(通常在 init 中调用)
int calculate_offsets(void);

// 获取进程名
static inline char *task_comm(struct task_struct *task) {
  return (char *)((uintptr_t)task + task_struct_offset.comm_offset);
}

// 获取进程 PID
static inline pid_t task_pid(struct task_struct *task) {
  return *(pid_t *)((uintptr_t)task + task_struct_offset.pid_offset);
}

// 获取进程 TGID
static inline pid_t task_tgid(struct task_struct *task) {
  return *(pid_t *)((uintptr_t)task + task_struct_offset.tgid_offset);
}

#endif /* _MY_KPM_H */