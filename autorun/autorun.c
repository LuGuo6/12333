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
KPM_DESCRIPTION("Auto-run script at /product/bin/Autorun");

#define GFP_KERNEL 0xCC0
#define UMH_WAIT_PROC 1

struct subprocess_info;
struct cred;

static long autorun_init(const char *args, const char *event, void *__user reserved)
{
    char *argv[] = { AUTORUN_SCRIPT_PATH, NULL };
    char *envp[] = { "HOME=/", "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/product/bin", NULL };
    int ret;

    struct subprocess_info *(*setup)(const char *, char **, char **,
        unsigned, void *, void *, void *);
    int (*exec)(struct subprocess_info *, int);

    setup = (typeof(setup))kallsyms_lookup_name("call_usermodehelper_setup");
    exec = (typeof(exec))kallsyms_lookup_name("call_usermodehelper_exec");

    if (!setup || !exec) {
        pr_err("autorun: lookup call_usermodehelper_setup/exec failed\n");
        return -1;
    }

    pr_info("autorun: executing %s\n", AUTORUN_SCRIPT_PATH);

    struct subprocess_info *info = setup(AUTORUN_SCRIPT_PATH, argv, envp,
        GFP_KERNEL, NULL, NULL, NULL);
    if (!info) {
        pr_err("autorun: call_usermodehelper_setup returned NULL\n");
        return -1;
    }

    ret = exec(info, UMH_WAIT_PROC);
    if (ret) {
        pr_err("autorun: call_usermodehelper_exec failed, ret=%d\n", ret);
    } else {
        pr_info("autorun: executed successfully\n");
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