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
KPM_DESCRIPTION("Auto-run script at /data/adb/autorun");

static long autorun_init(const char *args, const char *event, void *__user reserved)
{
    char *argv[] = { "/system/bin/sh", AUTORUN_SCRIPT_PATH, NULL };
    char *envp[] = { "HOME=/", "PATH=/sbin:/system/sbin:/system/bin:/system/xbin", NULL };
    int ret;

    int (*call_umh)(const char *, char **, char **, int) = NULL;
    call_umh = (typeof(call_umh))kallsyms_lookup_name("call_usermodehelper");
    if (!call_umh) {
        pr_err("autorun: call_usermodehelper not found\n");
        return -1;
    }

    pr_info("autorun: executing %s\n", AUTORUN_SCRIPT_PATH);

    ret = call_umh(argv[0], argv, envp, 1);
    if (ret) {
        pr_err("autorun: call_usermodehelper failed, ret=%d\n", ret);
    } else {
        pr_info("autorun: script executed successfully\n");
    }

    return 0;
}

static long autorun_exit(void *__user reserved)
{
    pr_info("autorun: exit\n");
    return 0;
}

static long autorun_ctl0(const char *args, char *__user out_msg, int out_msg_len)
{
    return 0;
}

KPM_INIT(autorun_init);
KPM_EXIT(autorun_exit);
KPM_CTL0(autorun_ctl0);