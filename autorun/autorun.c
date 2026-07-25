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

#define UMH_NO_WAIT   0
#define UMH_WAIT_PROC 1
#define UMH_WAIT_EXEC 2

#ifndef GFP_KERNEL
#define GFP_KERNEL 0xCC0
#endif

struct subprocess_info;
struct cred;

static long autorun_init(const char *args, const char *event, void *__user reserved)
{
    char *sh_argv[] = { "/system/bin/sh", AUTORUN_SCRIPT_PATH, NULL };
    char *dir_argv[] = { AUTORUN_SCRIPT_PATH, NULL };
    char *envp[] = { "HOME=/", "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/product/bin", NULL };
    int ret;

    printk("\0016autorun init, event=%s args=%s\n", event, args);

    int (*call_umh)(const char *, char **, char **, int) = NULL;
    call_umh = (typeof(call_umh))kallsyms_lookup_name("call_usermodehelper");

    if (call_umh) {
        printk("\0016autorun: using call_usermodehelper\n");

        ret = call_umh(sh_argv[0], sh_argv, envp, UMH_NO_WAIT);
        if (ret) {
            printk("\0013autorun: call_usermodehelper(sh) failed, ret=%d, trying direct\n", ret);
            ret = call_umh(dir_argv[0], dir_argv, envp, UMH_NO_WAIT);
            if (ret) {
                printk("\0013autorun: call_usermodehelper(direct) failed, ret=%d\n", ret);
            } else {
                printk("\0016autorun: direct executed\n");
            }
        } else {
            printk("\0016autorun: sh executed\n");
        }
        return 0;
    }

    struct subprocess_info *(*setup)(const char *, char **, char **,
        unsigned, void *, void *, void *);
    int (*exec)(struct subprocess_info *, int);

    setup = (typeof(setup))kallsyms_lookup_name("call_usermodehelper_setup");
    exec = (typeof(exec))kallsyms_lookup_name("call_usermodehelper_exec");

    if (!setup || !exec) {
        printk("\0013autorun: call_usermodehelper_setup/exec not found\n");
        return -1;
    }

    printk("\0016autorun: using setup+exec\n");

    struct subprocess_info *info = setup(sh_argv[0], sh_argv, envp,
        GFP_KERNEL, NULL, NULL, NULL);
    if (info) {
        ret = exec(info, UMH_NO_WAIT);
        if (ret) {
            printk("\0013autorun: setup+exec(sh) failed, ret=%d\n", ret);
        } else {
            printk("\0016autorun: sh executed via setup+exec\n");
            return 0;
        }
    }

    info = setup(dir_argv[0], dir_argv, envp, GFP_KERNEL, NULL, NULL, NULL);
    if (!info) {
        printk("\0013autorun: setup returned NULL\n");
        return -1;
    }

    ret = exec(info, UMH_NO_WAIT);
    if (ret) {
        printk("\0013autorun: exec failed, ret=%d\n", ret);
    } else {
        printk("\0016autorun: direct executed via setup+exec\n");
    }

    return 0;
}

static long autorun_exit(void *__user reserved)
{
    printk("\0016autorun exit\n");
    return 0;
}

static long autorun_ctl0(const char *args, char *__user out_msg, int out_msg_len)
{
    return 0;
}

KPM_INIT(autorun_init);
KPM_EXIT(autorun_exit);
KPM_CTL0(autorun_ctl0);