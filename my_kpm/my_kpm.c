/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * my_kpm.c - KPM 模块主文件
 *
 * ============================================================
 * KPM 模块开发指南
 * ============================================================
 *
 * 一个 KPM 模块必须实现以下 3 个函数(由 KernelPatch 框架调用):
 *
 *   long inline_hook_init(const char *args, const char *event, void *__user reserved)
 *      → 模块初始化入口, 在此处:
 *        1. 查找内核函数符号(kallsyms_lookup_name)
 *        2. 计算结构体偏移量
 *        3. 安装 hook
 *        4. 返回 0 表示成功, 负数表示失败
 *
 *   long inline_hook_control0(const char *args, char *__user out_msg, int out_msg_len)
 *      → 运行时控制接口, 用户态可通过 kpatch 命令调用:
 *        kpatch $SUPERKEY kpm ctl0 <模块名> <参数>
 *      → 返回 0 表示成功
 *
 *   long inline_hook_cleanup(const char *args, const char *event, void *__user reserved)
 *      → 模块卸载入口, 在此处解除所有 hook
 *      → 返回 0 表示成功
 *
 * ============================================================
 * 本示例模块功能:
 *   监控 do_send_sig_info, 当内核向进程发送 SIGKILL 时,
 *   记录日志并可以阻止对特定进程的 SIGKILL
 * ============================================================
 */

#include "my_kpm.h"
#include "utils.h"

// 模块版本号
#ifndef MYKPM_VERSION
#define MYKPM_VERSION "1.0.0"
#endif

// ============================================================
// 全局变量
// ============================================================

// 内核结构体偏移量(运行时动态计算)
struct task_struct_offset task_struct_offset;
struct cred_offset cred_offset;

// hook 相关的内核函数指针
static void *do_send_sig_info_ptr = NULL;

// 模块参数(可通过 control0 修改)
static int target_pid = 0;          // 要保护的进程 PID, 0 = 禁用
static bool enable_log = true;      // 是否打印日志

// ============================================================
// 偏移量计算
// ============================================================

// 计算所有需要的内核结构体偏移量
// 返回 0 成功, 负数失败
static int calculate_offsets(void) {
  // 查找用于计算偏移量的内核函数
  void *__task_cred = NULL;
  void *from_kuid = NULL;
  void *get_task_pid = NULL;

  lookup_name(__task_cred);
  lookup_name(from_kuid);
  lookup_name(get_task_pid);

  // 计算 task_struct 偏移量
  task_struct_offset.cred_offset = calculate_task_cred_offset(__task_cred);
  if (task_struct_offset.cred_offset < 0) {
    pr_err("my_kpm: failed to calculate cred_offset\n");
    return -30;
  }
  pr_info("my_kpm: task_struct->cred offset = 0x%x\n", task_struct_offset.cred_offset);

  task_struct_offset.pid_offset = calculate_task_pid_offset(get_task_pid);
  if (task_struct_offset.pid_offset < 0) {
    pr_err("my_kpm: failed to calculate pid_offset\n");
    return -31;
  }
  pr_info("my_kpm: task_struct->pid offset = 0x%x\n", task_struct_offset.pid_offset);

  // 计算 cred 偏移量
  cred_offset.uid_offset = calculate_cred_uid_offset(from_kuid);
  if (cred_offset.uid_offset < 0) {
    pr_err("my_kpm: failed to calculate uid_offset\n");
    return -32;
  }
  pr_info("my_kpm: cred->uid offset = 0x%x\n", cred_offset.uid_offset);

  // 对于高版本内核, uid/gid/euid/egid/suid/sgid 是连续的
  // 每个字段占 4 字节(kuid_t/kgid_t)
  cred_offset.gid_offset  = cred_offset.uid_offset + 4;
  cred_offset.euid_offset = cred_offset.uid_offset + 8;
  cred_offset.egid_offset = cred_offset.uid_offset + 12;
  cred_offset.suid_offset = cred_offset.uid_offset + 16;
  cred_offset.sgid_offset = cred_offset.uid_offset + 20;

  // task_struct 中的其他字段可以根据已知偏移量推算
  // 实际项目中需要通过分析对应函数来计算
  task_struct_offset.tgid_offset   = task_struct_offset.pid_offset + 4;
  task_struct_offset.comm_offset   = task_struct_offset.pid_offset + 8;
  task_struct_offset.jobctl_offset = task_struct_offset.cred_offset - 8;
  task_struct_offset.signal_offset = task_struct_offset.cred_offset - 16;

  return 0;
}

// ============================================================
// Hook 回调函数
// ============================================================

// 参数结构体(由 hook_wrap 框架传入)
// 实际参数列表取决于被 hook 函数的签名
// do_send_sig_info(int sig, struct kernel_siginfo *info, struct task_struct *p, enum pid_type type)
struct do_send_sig_info_args {
  int sig;                           // 信号编号
  struct kernel_siginfo *info;       // 信号信息
  struct task_struct *p;             // 目标进程
  enum pid_type type;                // PID 类型
};

// before 回调: 在 do_send_sig_info 执行前被调用
// 可以:
//   1. 读取参数 (args->sig, args->p 等)
//   2. 修改参数
//   3. 设置 args->ret = 错误码 来阻止原始函数执行
//   4. 设置 args->skip_origin = true 跳过原始函数
static void do_send_sig_info_before(const struct hook_wrap_params *params) {
  struct do_send_sig_info_args *args = (struct do_send_sig_info_args *)params->args;
  struct task_struct *current_task = get_current();

  // 只处理 SIGKILL (信号 9)
  if (args->sig != 9) return;

  // 获取发送者和目标进程信息
  pid_t sender_pid   = task_pid(current_task);
  pid_t target_pid_v = args->p ? task_pid(args->p) : 0;

  uid_t sender_uid = task_uid(current_task);

  // 打印日志(仅在 enable_log 为 true 时)
  if (enable_log) {
    pr_info("my_kpm: SIGKILL from PID=%d UID=%u to PID=%d\n",
            sender_pid, sender_uid, target_pid_v);
  }

  // 如果设置了 target_pid, 阻止对该进程的 SIGKILL
  if (target_pid != 0 && target_pid_v == target_pid) {
    pr_warn("my_kpm: blocked SIGKILL to protected PID=%d\n", target_pid_v);
    args->ret = -1;              // 返回错误码
    params->skip_origin = true;  // 跳过原始函数
  }
}

// after 回调: 在 do_send_sig_info 执行后被调用
// 可以:
//   1. 读取返回值 (args->ret)
//   2. 修改返回值
//   3. 读取参数(此时原始函数已执行)
static void do_send_sig_info_after(const struct hook_wrap_params *params) {
  // 本示例中暂不使用 after 回调
  (void)params;
}

// ============================================================
// KPM 生命周期函数
// ============================================================

// 模块初始化
// 当 KPM 被加载时, KernelPatch 框架调用此函数
long inline_hook_init(const char *args, const char *event, void *__user reserved) {
  pr_info("my_kpm: version %s initializing...\n", MYKPM_VERSION);

  // 步骤1: 计算偏移量
  int ret = calculate_offsets();
  if (ret < 0) {
    pr_err("my_kpm: calculate_offsets failed: %d\n", ret);
    return ret;
  }

  // 步骤2: 查找要 hook 的内核函数
  lookup_name(do_send_sig_info_ptr);

  // 步骤3: 安装 hook
  //   参数1: 内核函数指针
  //   参数2: 参数个数(4个参数: sig, info, p, type)
  //   参数3: before 回调
  //   参数4: after 回调
  //   参数5: 用户数据(可为 NULL)
  hook_func(do_send_sig_info_ptr, 4, do_send_sig_info_before, do_send_sig_info_after, NULL);

  pr_info("my_kpm: init complete!\n");
  return 0;
}

// 运行时控制
// 用户态命令: kpatch $SUPERKEY kpm ctl0 my_kpm "set_pid=1234"
// 用户态命令: kpatch $SUPERKEY kpm ctl0 my_kpm "log=off"
// 用户态命令: kpatch $SUPERKEY kpm ctl0 my_kpm "status"
long inline_hook_control0(const char *args, char *__user out_msg, int out_msg_len) {
  if (!args) return -1;

  pr_info("my_kpm: control0 args=%s\n", args);

  // 解析 set_pid=<pid> 参数
  if (strncmp(args, "set_pid=", 8) == 0) {
    int new_pid;
    if (sscanf(args + 8, "%d", &new_pid) == 1) {
      target_pid = new_pid;
      pr_info("my_kpm: target_pid set to %d\n", target_pid);
    }
  }
  // 解析 log=on|off 参数
  else if (strncmp(args, "log=", 4) == 0) {
    if (strstr(args + 4, "on")) {
      enable_log = true;
    } else if (strstr(args + 4, "off")) {
      enable_log = false;
    }
    pr_info("my_kpm: logging %s\n", enable_log ? "enabled" : "disabled");
  }
  // 查询状态
  else if (strcmp(args, "status") == 0) {
    pr_info("my_kpm: target_pid=%d, log=%s\n",
            target_pid, enable_log ? "on" : "off");
  }

  return 0;
}

// 模块清理
// 当 KPM 被卸载时, KernelPatch 框架调用此函数
long inline_hook_cleanup(const char *args, const char *event, void *__user reserved) {
  pr_info("my_kpm: cleaning up...\n");

  // 解除所有 hook
  unhook_func(do_send_sig_info_ptr);

  pr_info("my_kpm: cleanup complete!\n");
  return 0;
}