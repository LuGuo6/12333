/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * cpuinfo_kirin9020.c - 伪造 /proc/cpuinfo 的 KPM 模块
 *
 * ============================================================
 * 行为逻辑 (基于 IDA 逆向分析):
 * ============================================================
 *
 * 本模块通过 hook ARM64 的三个系统调用, 拦截进程对 /proc/cpuinfo
 * 的读取, 返回伪造的 CPU 信息, 使系统看起来像是搭载了
 * HiSilicon Kirin 9020 处理器.
 *
 * 三个 hook 点:
 *   syscall 56 (openat)  → before_openat  + after_openat
 *   syscall 63 (read)    → before_read
 *   syscall 57 (close)   → before_close
 *
 * 数据流:
 *   1. openat("/proc/cpuinfo") → before_openat 检测文件名
 *      → 匹配则标记 a1->ret=0, 设 a1->__flag=1
 *   2. 在 after_openat 中, 将 {task, fd, offset=0} 存入 tracked_fds[]
 *   3. read(fd, buf, n) → before_read 查找 tracked_fds
 *      → 用 compat_copy_to_user 将 fake_cpuinfo 内容拷贝到用户空间
 *      → 更新 offset, 设置返回值
 *   4. close(fd) → before_close 清除 tracked_fds 中的条目
 *
 * 任务识别:
 *   通过读取 ARM64 SP_EL0 寄存器获取当前 task_struct 指针,
 *   兼容两种内核配置:
 *     - sp_el0_is_current: SP_EL0 直接存储 task_struct
 *     - sp_el0_is_thread_info: SP_EL0 存储 thread_info, 需加偏移
 *
 * ============================================================
 * 移植说明:
 *   原始代码使用 hook_syscalln (KernelPatch 高层 API),
 *   本实现使用 hook_wrap (底层 API), 直接 hook 内核函数
 *   __arm64_sys_openat / __arm64_sys_read / __arm64_sys_close
 * ============================================================
 */

#include "cpuinfo_kirin9020.h"
#include "../kpm_utils.h"

#ifndef CPUINFO_VERSION
#define CPUINFO_VERSION "1.0.0"
#endif

// ============================================================
// 全局状态
// ============================================================

static struct cpuinfo_state g_state;

// 伪造的 cpuinfo 内容
static const char fake_cpuinfo[] = FAKE_CPUINFO_CONTENT;
#define FAKE_CPUINFO_SIZE (sizeof(fake_cpuinfo) - 1)

// ============================================================
// 内核函数指针
// ============================================================

// 用于获取 current task
static struct task_struct *(*get_current_fn)(void) = NULL;

// ============================================================
// 工具函数
// ============================================================

// 获取当前 task_struct 指针
// ARM64 上通过 SP_EL0 寄存器获取, 兼容多种内核配置
static inline struct task_struct *get_current_task(void) {
  if (get_current_fn) {
    return get_current_fn();
  }
  // 回退: 直接读 SP_EL0 (许多 ARM64 内核的 current 实现)
  unsigned long sp_el0;
  asm volatile("mrs %0, sp_el0" : "=r"(sp_el0));
  return (struct task_struct *)sp_el0;
}

// 在追踪表中查找条目, 返回索引, 未找到返回 -1
static int find_tracked_fd(struct task_struct *task, int fd) {
  for (int i = 0; i < MAX_TRACKED_FDS; i++) {
    if (g_state.fds[i].task == task && g_state.fds[i].fd == fd) {
      return i;
    }
  }
  return -1;
}

// 添加追踪条目, 优先复用空槽位
static int add_tracked_fd(struct task_struct *task, int fd) {
  int free_slot = -1;

  for (int i = 0; i < MAX_TRACKED_FDS; i++) {
    // 检查是否已存在
    if (g_state.fds[i].task == task && g_state.fds[i].fd == fd) {
      g_state.fds[i].offset = 0;
      return i;
    }
    // 记录第一个空槽位
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

// 检查文件名是否匹配 /proc/cpuinfo
// 支持完整路径 "/proc/cpuinfo" 和短名 "cpuinfo"
static bool is_cpuinfo_path(const char *filename, int len) {
  if (len <= 0) return false;

  // 从末尾向前匹配, 判断是否以 "/proc/cpuinfo" 结尾
  if (len >= TARGET_FILE_FULL_LEN) {
    const char *suffix = filename + len - TARGET_FILE_FULL_LEN;
    if (strncmp(suffix, TARGET_FILE_FULL, TARGET_FILE_FULL_LEN) == 0) {
      return true;
    }
  }

  // 匹配 "cpuinfo" (不含路径)
  if (len >= TARGET_FILE_NAME_LEN) {
    const char *suffix = filename + len - TARGET_FILE_NAME_LEN;
    if (strncmp(suffix, TARGET_FILE_NAME, TARGET_FILE_NAME_LEN) == 0) {
      return true;
    }
  }

  return false;
}

// ============================================================
// Hook 回调: openat 系统调用
// ============================================================

// __arm64_sys_openat 的参数:
//   int dfd, const char __user *filename, int flags, umode_t mode
struct openat_args {
  int dfd;
  const char __user *filename;
  int flags;
  umode_t mode;
};

// openat 的 before 回调
// 检测是否正在打开 /proc/cpuinfo, 如果是则标记此调用
static void before_openat(const struct hook_wrap_params *params) {
  struct openat_args *args = (struct openat_args *)params->args;

  if (!g_state.enabled) return;

  // 从用户空间读取文件名 (最多 128 字节)
  char filename[128] = {0};
  long ret = strncpy_from_user(filename, args->filename, sizeof(filename) - 1);
  if (ret <= 0) return;

  filename[127] = '\0';

  // 检测是否为 /proc/cpuinfo
  if (is_cpuinfo_path(filename, (int)ret)) {
    // 标记此 openat 调用需要追踪
    // 使用 args->flags 的保留位来传递标记 (hack做法)
    // 实际做法: 设置一个标志位, 在 after 回调中检查
    // 这里我们直接设置返回值, 让 openat 返回成功
    // 但真正的追踪在 after 回调中完成
    // 设置一个魔数标记到参数中
    args->dfd = 0xCPUNFO;  // 魔数标记, 在 after 回调中识别
    pr_info("cpuinfo: intercepted openat(/proc/cpuinfo)\n");
  }
}

// openat 的 after 回调
// 在 openat 成功返回后, 将 fd 加入追踪表
static void after_openat(const struct hook_wrap_params *params) {
  struct openat_args *args = (struct openat_args *)params->args;

  if (!g_state.enabled) return;

  // 检查是否是之前标记的 /proc/cpuinfo 打开操作
  if (args->dfd != 0xCPUNFO) return;
  args->dfd = 0;  // 恢复

  // 获取 openat 的返回值 (即 fd)
  long fd = args->ret;
  if (fd < 0) return;  // 打开失败

  struct task_struct *task = get_current_task();
  if (!task) return;

  int idx = add_tracked_fd(task, (int)fd);
  if (idx >= 0) {
    pr_info("cpuinfo: tracked fd=%ld for task=%px\n", fd, task);
  }
}

// ============================================================
// Hook 回调: read 系统调用
// ============================================================

// __arm64_sys_read 的参数:
//   unsigned int fd, char __user *buf, size_t count
struct read_args {
  unsigned int fd;
  char __user *buf;
  size_t count;
};

// read 的 before 回调
// 如果 fd 是被追踪的 /proc/cpuinfo, 用伪造数据替代真实读取
static void before_read(const struct hook_wrap_params *params) {
  struct read_args *args = (struct read_args *)params->args;

  if (!g_state.enabled) return;

  struct task_struct *task = get_current_task();
  if (!task) return;

  // 查找是否是追踪的 fd
  int idx = find_tracked_fd(task, (int)args->fd);
  if (idx < 0) return;  // 不是追踪的 fd, 放行

  struct tracked_fd *entry = &g_state.fds[idx];
  unsigned long offset = entry->offset;

  // 检查是否已读到文件末尾
  if (offset >= FAKE_CPUINFO_SIZE) {
    // EOF: 返回 0
    args->ret = 0;
    params->skip_origin = true;
    return;
  }

  // 计算本次可读取的字节数
  size_t remaining = FAKE_CPUINFO_SIZE - offset;
  size_t to_copy = (args->count < remaining) ? args->count : remaining;

  if (to_copy == 0) {
    args->ret = 0;
    params->skip_origin = true;
    return;
  }

  // 将伪造数据拷贝到用户空间
  long copied = copy_to_user(args->buf, fake_cpuinfo + offset, to_copy);
  if (copied != 0) {
    // 拷贝失败
    args->ret = -EFAULT;
    params->skip_origin = true;
    return;
  }

  // 更新偏移量
  entry->offset = offset + to_copy;

  // 设置返回值 = 实际读取的字节数
  args->ret = (long)to_copy;
  params->skip_origin = true;

  pr_info("cpuinfo: read fd=%u offset=%lu count=%zu\n",
          args->fd, offset, to_copy);
}

// ============================================================
// Hook 回调: close 系统调用
// ============================================================

// __arm64_sys_close 的参数:
//   unsigned int fd
struct close_args {
  unsigned int fd;
};

// close 的 before 回调
// 清理追踪表中的条目
static void before_close(const struct hook_wrap_params *params) {
  struct close_args *args = (struct close_args *)params->args;

  if (!g_state.enabled) return;

  struct task_struct *task = get_current_task();
  if (!task) return;

  int idx = find_tracked_fd(task, (int)args->fd);
  if (idx >= 0) {
    remove_tracked_fd(task, (int)args->fd);
    pr_info("cpuinfo: untracked fd=%u\n", args->fd);
  }
}

// ============================================================
// 内核函数指针 (用于 hook_wrap)
// ============================================================

static void *sys_openat_ptr = NULL;
static void *sys_read_ptr = NULL;
static void *sys_close_ptr = NULL;

// ============================================================
// KPM 生命周期函数
// ============================================================

// 模块初始化
long inline_hook_init(const char *args, const char *event, void *__user reserved) {
  pr_info("cpuinfo_kirin9020: version %s initializing...\n", CPUINFO_VERSION);

  // 初始化全局状态
  memset(&g_state, 0, sizeof(g_state));
  g_state.enabled = 1;

  // 查找内核函数
  // ARM64 系统调用入口: __arm64_sys_openat, __arm64_sys_read, __arm64_sys_close
  lookup_name(sys_openat_ptr);
  lookup_name(sys_read_ptr);
  lookup_name(sys_close_ptr);

  // 尝试获取 get_current 函数
  lookup_name_continue(get_current_fn);

  // 安装 hook
  // openat: 4 个参数 (dfd, filename, flags, mode)
  hook_func(sys_openat_ptr, 4, before_openat, after_openat, NULL);

  // read: 3 个参数 (fd, buf, count)
  hook_func(sys_read_ptr, 3, before_read, NULL, NULL);

  // close: 1 个参数 (fd)
  hook_func(sys_close_ptr, 1, before_close, NULL, NULL);

  pr_info("cpuinfo_kirin9020: init complete! fake CPU: HiSilicon Kirin 9020\n");
  return 0;
}

// 运行时控制
// 用法: kpatch $SUPERKEY kpm ctl0 cpuinfo_kirin9020 "status"
// 用法: kpatch $SUPERKEY kpm ctl0 cpuinfo_kirin9020 "enable"
// 用法: kpatch $SUPERKEY kpm ctl0 cpuinfo_kirin9020 "disable"
long inline_hook_control0(const char *args, char *__user out_msg, int out_msg_len) {
  if (!args) return -1;

  if (strcmp(args, "status") == 0) {
    const char *msg = g_state.enabled ? "enabled" : "disabled";
    if (out_msg && out_msg_len > 0) {
      long len = strlen(msg);
      if (len > out_msg_len) len = out_msg_len;
      if (copy_to_user(out_msg, msg, len) == 0) {
        // 成功
      }
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
long inline_hook_cleanup(const char *args, const char *event, void *__user reserved) {
  pr_info("cpuinfo_kirin9020: cleaning up...\n");

  // 解除所有 hook
  unhook_func(sys_openat_ptr);
  unhook_func(sys_read_ptr);
  unhook_func(sys_close_ptr);

  // 清空追踪表
  memset(&g_state, 0, sizeof(g_state));

  pr_info("cpuinfo_kirin9020: cleanup complete!\n");
  return 0;
}