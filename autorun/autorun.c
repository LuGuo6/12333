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

typedef int (*call_usermodehelper_t)(const char *path, char **argv, char **envp, int wait);

static long autorun_init(const char *args, const char *event, void *__user reserved)
{
    call_usermodehelper_t call_umh;
    int ret;
    
    printk(KERN_INFO "autorun: init, event=%s\n", event);
    
    call_umh = (call_usermodehelper_t)kallsyms_lookup_name("call_usermodehelper");
    if (!call_umh) {
        printk(KERN_ERR "autorun: call_usermodehelper not found\n");
        return -1;
    }
    
    char *argv[] = { "/system/bin/sh", AUTORUN_SCRIPT_PATH, NULL };
    char *envp[] = { "PATH=/sbin:/system/sbin:/system/bin:/system/xbin", NULL };
    
    ret = call_umh(argv[0], argv, envp, UMH_WAIT_PROC);
    printk(KERN_INFO "autorun: execution result=%d\n", ret);
    
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