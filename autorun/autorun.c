/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <compiler.h>
#include <hook.h>
#include <kpmodule.h>
#include <kputils.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/version.h>
#include <linux/kallsyms.h>
#include <linux/workqueue.h>    // 添加

#include "autorun.h"

#ifndef AUTORUN_VERSION
#define AUTORUN_VERSION "1.0.0"
#endif

KPM_NAME("autorun");
KPM_VERSION(AUTORUN_VERSION);
KPM_LICENSE("GPL v2");
KPM_AUTHOR("");
KPM_DESCRIPTION("Auto-run script at /data/adb/Autorun");

static struct work_struct autorun_work;

static void autorun_work_handler(struct work_struct *work)
{
    int ret;
    int (*call_umh)(const char *, char **, char **, int);
    
    printk(KERN_INFO "autorun: work handler started\n");
    
    // 查找函数
    call_umh = (typeof(call_umh))kallsyms_lookup_name("call_usermodehelper");
    if (!call_umh) {
        printk(KERN_ERR "autorun: call_usermodehelper not found\n");
        return;
    }
    
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
}

static long autorun_init(const char *args, const char *event, void *__user reserved)
{
    printk(KERN_INFO "autorun: init, event=%s\n", event);
    
    // 初始化工作队列并调度
    INIT_WORK(&autorun_work, autorun_work_handler);
    schedule_work(&autorun_work);
    
    printk(KERN_INFO "autorun: work scheduled\n");
    return 0;
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