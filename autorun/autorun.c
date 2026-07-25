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

static int do_exec(char **argv, char **envp, int wait)
{
    int ret;

    int (*call_umh)(const char *, char **, char **, int) = NULL;
    call_umh = (typeof(call_umh))kallsyms_lookup_name("call_usermodehelper");

    if (call_umh) {
        ret = call_umh(argv[0], argv, envp, wait);
        printk("\0016autorun: call_usermodehelper ret=%d\n", ret);
        return ret;
    }

    struct subprocess_info *(*setup)(const char *, char **, char **,
        unsigned, void *, void *, void *);
    int (*exec)(struct subprocess_info *, int);

    setup = (typeof(setup))kallsyms_lookup_name("call_usermodehelper_setup");
    exec = (typeof(exec))kallsyms_lookup_name("call_usermodehelper_exec");

    if (!setup || !exec) {
        printk("\0013autorun: setup=%px exec=%px\n", setup, exec);
        return -38;
    }

    struct subprocess_info *info = setup(argv[0], argv, envp,
        GFP_KERNEL, NULL, NULL, NULL);
    if (!info) {
        printk("\0013autorun: call_usermodehelper_setup returned NULL\n");
        return -12;
    }

    ret = exec(info, wait);
    printk("\0016autorun: call_usermodehelper_exec ret=%d\n", ret);
    return ret;
}

static long autorun_init(const char *args, const char *event, void *__user reserved)
{
    int ret;

    printk("\0016autorun: init, event=%s\n", event);

    char *envp[] = { "HOME=/", "PATH=/sbin:/system/sbin:/system/bin:/system/xbin:/product/bin", NULL };

    char *runcon_argv[] = {
        "/system/bin/runcon", "u:r:shell:s0",
        "/system/bin/sh", AUTORUN_SCRIPT_PATH,
        NULL
    };

    char *sh_setcon_argv[] = {
        "/system/bin/sh", "-c",
        "echo u:r:shell:s0 > /proc/self/attr/exec 2>/dev/null; exec sh /data/adb/Autorun",
        NULL
    };

    ret = do_exec(runcon_argv, envp, UMH_WAIT_PROC);
    printk("\0016autorun: runcon exit=%d\n", ret);

    if (ret != 0) {
        printk("\0013autorun: runcon failed, trying /proc/self/attr/exec\n");
        ret = do_exec(sh_setcon_argv, envp, UMH_WAIT_PROC);
        printk("\0016autorun: sh_setcon exit=%d\n", ret);
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