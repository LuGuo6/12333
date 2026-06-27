/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * utils.h - 模块专用工具函数
 * 包含 ARM64 指令解析、动态偏移量计算等
 */
#ifndef _MY_KPM_UTILS_H
#define _MY_KPM_UTILS_H

#include "../kpm_utils.h"
#include "my_kpm.h"

// ============================================================
// 动态偏移量计算
// 原理: 通过分析内核函数的 ARM64 机器码, 自动提取结构体字段偏移量
// 这样就不需要为每个内核版本硬编码偏移量
// ============================================================

// 伪代码: 计算偏移量的通用流程
//
// int calculate_offsets(void) {
//   // 步骤1: 查找需要分析的内核函数
//   lookup_name(func_ptr);
//
//   // 步骤2: 遍历函数的每一条 ARM64 指令
//   uint32_t *code = (uint32_t *)func_ptr;
//   for (int i = 0; i < MAX_INST_COUNT; i++) {
//     uint32_t inst = code[i];
//
//     // 步骤3: 匹配指令模式, 提取偏移量
//     // 例如: 匹配 ADD x0, sp, #imm 提取栈偏移
//     if (inst_is_add_imm(inst)) {
//       int rd = inst_get_add_imm_rd(inst);
//       int rn = inst_get_add_imm_rn(inst);
//       long imm = inst_get_add_imm_imm(inst);
//       // 如果 rn 是 sp 且 rd 是目标寄存器, imm 就是偏移量
//     }
//
//     // 步骤4: 匹配 LDR/STR 指令, 提取结构体偏移
//     // 例如: LDR x0, [x19, #0x3A0] → 提取 #0x3A0
//     if (inst_is_ldr_imm_uint(inst)) {
//       int rt = inst_get_ldr_imm_uint_rt(inst);
//       int rn = inst_get_ldr_imm_uint_rn(inst);
//       long imm = inst_get_ldr_imm_uint_imm(inst);
//       // 结合上下文判断这是哪个字段的偏移
//     }
//   }
// }
//
// 关键函数:
//   inst_is_XXX(inst)     - 判断指令类型
//   inst_get_XXX_rd(inst) - 提取目标寄存器
//   inst_get_XXX_rn(inst) - 提取源寄存器
//   inst_get_XXX_imm(inst) - 提取立即数(即偏移量)

// ============================================================
// 实际偏移量计算函数
// ============================================================

// 通过分析 __task_cred 函数获取 task_struct->cred 偏移量
// 伪代码:
//   __task_cred 的机器码中会有类似:
//     LDR x0, [x0, #0x3A0]   ← 这个 #0x3A0 就是 cred 字段的偏移
//   所以遍历该函数找到第一条 LDR 指令即可
static inline int calculate_task_cred_offset(void *__task_cred) {
  if (!__task_cred) return -1;

  uint32_t *code = (uint32_t *)__task_cred;
  for (int i = 0; i < 20; i++) {
    uint32_t inst = code[i];
    // 匹配 LDR x0, [x0, #imm] 或类似指令
    if (inst_is_ldr_imm_uint(inst)) {
      int rt = inst_get_ldr_imm_uint_rt(inst);
      int rn = inst_get_ldr_imm_uint_rn(inst);
      long imm = inst_get_ldr_imm_uint_imm(inst);
      if (rt == 0 && rn == 0 && imm > 0) {
        return (int)imm;
      }
    }
    // 遇到 ret 指令则停止
    if (inst_is_ret(inst)) break;
  }
  return -1;
}

// 通过分析函数获取 cred 结构体内部偏移量
// 原理同上, 分析 kuid_t 相关函数中的 LDR 指令
static inline int calculate_cred_uid_offset(void *from_kuid) {
  if (!from_kuid) return -1;

  uint32_t *code = (uint32_t *)from_kuid;
  for (int i = 0; i < 15; i++) {
    uint32_t inst = code[i];
    // 寻找 LDR x0, [x0, #imm]
    if (inst_is_ldr_imm_uint(inst)) {
      int rt = inst_get_ldr_imm_uint_rt(inst);
      int rn = inst_get_ldr_imm_uint_rn(inst);
      long imm = inst_get_ldr_imm_uint_imm(inst);
      if (rt == 0 && rn == 0 && imm > 0) {
        return (int)imm;
      }
    }
    if (inst_is_ret(inst)) break;
  }
  return -1;
}

// 通过分析 get_task_pid 函数获取 task_struct->pid 偏移量
static inline int calculate_task_pid_offset(void *get_task_pid) {
  if (!get_task_pid) return -1;

  uint32_t *code = (uint32_t *)get_task_pid;
  for (int i = 0; i < 20; i++) {
    uint32_t inst = code[i];
    // 匹配 LDR w0, [x0, #imm] (32位加载)
    if (inst_is_ldr_imm_uint(inst)) {
      int sf = inst_get_ldr_imm_uint_sf(inst);
      int rt = inst_get_ldr_imm_uint_rt(inst);
      int rn = inst_get_ldr_imm_uint_rn(inst);
      long imm = inst_get_ldr_imm_uint_imm(inst);
      if (sf == 0 && rt == 0 && rn == 0 && imm > 0) {
        return (int)imm;
      }
    }
    if (inst_is_ret(inst)) break;
  }
  return -1;
}

#endif /* _MY_KPM_UTILS_H */