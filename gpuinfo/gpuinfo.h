/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef _GPUINFO_H
#define _GPUINFO_H

#define MAX_TRACKED_FDS      64

// ============================================================
// 路径最大长度
// ============================================================
#define MAX_PATH_LEN         128

// ============================================================
// Qualcomm Adreno (骁龙平台)
// 路径前缀: /sys/class/kgsl/kgsl-3d0/
// ============================================================

// Adreno 750 (骁龙 8 Gen3)
#define ADRENO_750_MODEL         "Qualcomm Adreno (TM) 750"
#define ADRENO_750_MODEL_SIZE    28

#define ADRENO_750_ID            "0x07500000"
#define ADRENO_750_ID_SIZE       11

#define ADRENO_750_MAX_GPUCLK    "903000000"
#define ADRENO_750_MAX_GPUCLK_SIZE 10

#define ADRENO_750_FREQ_TABLE    "276 414 514 580 670 710 770 810 903"
#define ADRENO_750_FREQ_TABLE_SIZE 39

// Adreno 740 (骁龙 8 Gen2)
#define ADRENO_740_MODEL         "Qualcomm Adreno (TM) 740"
#define ADRENO_740_MODEL_SIZE    28

#define ADRENO_740_ID            "0x06e00000"
#define ADRENO_740_ID_SIZE       11

// ============================================================
// ARM Mali (天玑平台 / 三星 Exynos)
// 路径前缀: /sys/class/misc/mali0/device/ 或 /sys/class/misc/mali/device/
// ============================================================

// Mali-G720 (天玑 9300)
#define MALI_G720_INFO           "Mali-G720 MC12"
#define MALI_G720_INFO_SIZE      15

#define MALI_G720_MODEL          "Mali-G720"
#define MALI_G720_MODEL_SIZE     10

// Mali-G715 (天玑 9200)
#define MALI_G715_INFO           "Mali-G715 MC10"
#define MALI_G715_INFO_SIZE      15

#define MALI_G715_MODEL          "Mali-G715"
#define MALI_G715_MODEL_SIZE     10

// ============================================================
// 内核版本伪造（匹配骁龙 8 Gen3 / Adreno 750 的典型内核）
// ============================================================

// uname -r 返回的版本字符串
#define FAKE_KERNEL_VERSION        "6.1.75-android14-11-gc52c7e3e6"
#define FAKE_KERNEL_VERSION_LEN    35

// /proc/version 完整内容
#define FAKE_PROC_VERSION \
    "Linux version 6.1.75-android14-11-gc52c7e3e6 (build@ab8) " \
    "(Android clang version 17.0.2, LTO) #1 SMP PREEMPT Mon Jan  1 00:00:00 UTC 2024"
#define FAKE_PROC_VERSION_SIZE \
    (sizeof(FAKE_PROC_VERSION) - 1)

// ============================================================
// 路径匹配表
// ============================================================

struct gpu_path_entry {
    const char *path;       // 要匹配的 sysfs 路径
    int path_len;           // 路径字符串长度
    const char *fake_data;  // 对应的伪造数据
    int fake_size;          // 伪造数据大小
};

// 路径表在 .c 中定义
extern struct gpu_path_entry gpu_paths[];

#endif /* _GPUINFO_H */
