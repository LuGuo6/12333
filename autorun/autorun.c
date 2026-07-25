/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <compiler.h>
#include <hook.h>
#include <kpmodule.h>
#include <kputils.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/version.h>
#include <linux/kallsyms.h>    // 添加

#include "autorun.h"

#ifndef AUTORUN_VERSION
#define AUTORUN_VERSION "1.0.0"
#endif

KPM_NAME("autorun");
KPM_VERSION(AUTORUN_VERSION);
KPM_LICENSE("GPL v2");
KPM_AUTHOR("");
KPM_DESCRIPTION("Auto-run script at /data/adb/Autorun");

// 定义常量
#define UMH_NO_WAIT    0
#define UMH_WAIT_PROC  1
#define UMH_WAIT_EXEC  2

typedef int (*call_usermodehelper_t)(const char *path, char **argv, char **envp, int wait);

static long autorun_init(const char *args, const char *event, void *__user reserved)
{
    int ret;
    call_usermodehelper_t call_umh;
    
    printk(KERN_INFO "autorun: init, event=%s\n", event);
    
    // 动态查找 call_usermodehelper
    call_umh = (call_usermodehelper_t)kallsyms_lookup_name("call_usermodehelper");
    if (!call_umh) {
        printk(KERN_ERR "autorun: call_usermodehelper not found\n");
        return -ENOENT;
    }
    printk(KERN_INFO "autorun: call_usermodehelper found at %p\n", call_umh);
    
    // 检查脚本文件是否存在
    struct file *fp;
    mm_segment_t old_fs;
    
    old_fs = get_fs();
    set_fs(KERNEL_DS);
    fp = filp_open(AUTORUN_SCRIPT_PATH, O_RDONLY, 0);
    if (IS_ERR(fp)) {
        printk(KERN_ERR "autorun: file %s not found\n", AUTORUN_SCRIPT_PATH);
        set_fs(old_fs);
        return -ENOENT;
    }
    filp_close(fp, NULL);
    set_fs(old_fs);
    
    // 执行脚本
    char *sh_argv[] = { "/system/bin/sh", AUTORUN_SCRIPT_PATH, NULL };
    char *envp[] = { 
        "HOME=/", 
        "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/product/bin:/vendor/bin", 
        NULL 
    };
    
    ret = call_umh(sh_argv[0], sh_argv, envp, UMH_WAIT_PROC);
    if (ret == 0) {
        printk(KERN_INFO "autorun: script executed successfully\n");
    } else {
        printk(KERN_ERR "autorun: script execution failed, ret=%d\n", ret);
    }
    
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