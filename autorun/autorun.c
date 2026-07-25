/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <compiler.h>
#include <hook.h>
#include <kpmodule.h>
#include <kputils.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/version.h>

#include "autorun.h"

#ifndef AUTORUN_VERSION
#define AUTORUN_VERSION "1.0.0"
#endif

KPM_NAME("autorun");
KPM_VERSION(AUTORUN_VERSION);
KPM_LICENSE("GPL v2");
KPM_AUTHOR("");
KPM_DESCRIPTION("Auto-run script at /data/adb/Autorun");

// 定义需要的常量
#define UMH_NO_WAIT    0
#define UMH_WAIT_PROC  1
#define UMH_WAIT_EXEC  2

// 声明函数类型
typedef int (*call_usermodehelper_t)(const char *path, char **argv, char **envp, int wait);

// 全局变量用于缓存函数指针
static call_usermodehelper_t call_umh = NULL;

static long autorun_init(const char *args, const char *event, void *__user reserved)
{
    int ret;
    
    printk(KERN_INFO "autorun: init, event=%s\n", event);
    
    // 只查找一次 call_usermodehelper
    if (!call_umh) {
        call_umh = (call_usermodehelper_t)kallsyms_lookup_name("call_usermodehelper");
        if (!call_umh) {
            printk(KERN_ERR "autorun: call_usermodehelper not found in kernel\n");
            return -1;
        }
        printk(KERN_INFO "autorun: call_usermodehelper found at %p\n", call_umh);
    }
    
    // 尝试直接执行脚本
    char *sh_argv[] = { "/system/bin/sh", AUTORUN_SCRIPT_PATH, NULL };
    char *envp[] = { 
        "HOME=/", 
        "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/product/bin:/vendor/bin", 
        NULL 
    };
    
    // 方法1: 通过 sh 执行
    ret = call_umh(sh_argv[0], sh_argv, envp, UMH_WAIT_PROC);
    if (ret == 0) {
        printk(KERN_INFO "autorun: script executed successfully via sh\n");
        return 0;
    }
    printk(KERN_WARNING "autorun: sh execution failed, ret=%d, trying direct...\n", ret);
    
    // 方法2: 直接执行脚本（需要脚本有 shebang 和执行权限）
    char *dir_argv[] = { AUTORUN_SCRIPT_PATH, NULL };
    ret = call_umh(dir_argv[0], dir_argv, envp, UMH_WAIT_PROC);
    if (ret == 0) {
        printk(KERN_INFO "autorun: script executed successfully directly\n");
        return 0;
    }
    printk(KERN_ERR "autorun: all execution methods failed, ret=%d\n", ret);
    
    return ret;
}

static long autorun_exit(void *__user reserved)
{
    printk(KERN_INFO "autorun: exit\n");
    return 0;
}

static long autorun_ctl0(const char *args, char *__user out_msg, int out_msg_len)
{
    return 0;
}

KPM_INIT(autorun_init);
KPM_EXIT(autorun_exit);
KPM_CTL0(autorun_ctl0);