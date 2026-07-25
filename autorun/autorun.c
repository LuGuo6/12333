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

#define UMH_WAIT_PROC 1

struct subprocess_info;
struct cred;


// 1. 将等待模式改为 UMH_WAIT_PROC
static long autorun_init(const char *args, const char *event, void *__user reserved)
{
    int ret;

    // 2. 扩展环境变量 PATH
    char *envp[] = { 
        "HOME=/", 
        "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/product/bin:/vendor/bin", 
        NULL 
    };

    // 3. 用 sh 执行脚本（保留这种方式，更可靠）
    char *sh_argv[] = { "/system/bin/sh", AUTORUN_SCRIPT_PATH, NULL };
    ret = call_usermodehelper(sh_argv[0], sh_argv, envp, UMH_WAIT_PROC);
    if (ret == 0) {
        printk(KERN_INFO "autorun: script executed successfully.\n");
    } else {
        printk(KERN_ERR "autorun: script execution failed, ret=%d\n", ret);
    }
    
    return 0;
}

static long autorun_exit(void *__user reserved)
{
    printk("\0016autorun: exit\n");
    return 0;
}

static long autorun_ctl0(const char *args, char *__user out_msg, int out_msg_len)
{
    return 0;
}

KPM_INIT(autorun_init);
KPM_EXIT(autorun_exit);
KPM_CTL0(autorun_ctl0);