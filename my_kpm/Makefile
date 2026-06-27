# ============================================================
# KPM 模块 Makefile
# ============================================================
# 使用方法:
#   1. 设置环境变量 ANDROID_NDK 或 ANDROID_NDK_LATEST_HOME
#   2. make           → 编译 release 版本
#   3. make debug     → 编译 debug 版本(带日志)
#   4. make clean     → 清理编译产物
# ============================================================

# 模块版本号
MYKPM_VERSION := 1.0.0

# KernelPatch 源码目录(相对于本 Makefile 的路径)
ifndef KP_DIR
    KP_DIR = ../KernelPatch
endif

# 检测操作系统和架构
OS_NAME = $(shell uname | tr A-Z a-z)
MACHINE = $(shell uname -m)
HOST_TAG = $(OS_NAME)-$(MACHINE)
ifeq ($(HOST_TAG), darwin-arm64)
    HOST_TAG = darwin-x86_64
endif

# NDK 工具链路径
NDK_BIN_DIR := toolchains/llvm/prebuilt/$(HOST_TAG)/bin
ifdef ANDROID_NDK_LATEST_HOME
    NDK_PATH ?= $(ANDROID_NDK_LATEST_HOME)/$(NDK_BIN_DIR)
else ifdef ANDROID_NDK
    NDK_PATH ?= $(ANDROID_NDK)/$(NDK_BIN_DIR)
endif

# 交叉编译工具
ifdef TARGET_COMPILE
    CC := $(TARGET_COMPILE)gcc
    LD := $(TARGET_COMPILE)ld
else ifdef NDK_PATH
    CC := $(NDK_PATH)/aarch64-linux-android31-clang
    LD := $(NDK_PATH)/ld.lld
endif

# 编译选项
CFLAGS = -Wall -O2 -fno-PIC -fno-asynchronous-unwind-tables -fno-stack-protector -fno-common -DMYKPM_VERSION=\"$(MYKPM_VERSION)$(MYKPM_VER)\"

# 头文件搜索路径
INCLUDE_DIRS := . include patch/include linux/include linux/arch/arm64/include linux/tools/arch/arm64/include

INCLUDE_FLAGS := $(foreach dir,$(INCLUDE_DIRS),-I$(KP_DIR)/kernel/$(dir))

# 额外包含父目录(用于 kpm_utils.h 的 #include "../kpm_utils.h")
INCLUDE_FLAGS += -I..

# 源文件列表
objs := my_kpm.c

# ============================================================
# 编译目标
# ============================================================

all: base

# 普通编译
base: my_kpm_$(MYKPM_VERSION).kpm

# Debug 编译
debug: CFLAGS += -DCONFIG_DEBUG
debug: MYKPM_VER := _d
debug: my_kpm_$(MYKPM_VERSION)_debug.kpm

# 链接生成 .kpm 文件
my_kpm_$(MYKPM_VERSION).kpm: ${objs}
	${CC} $(CFLAGS) $(CFLAG) $(INCLUDE_FLAGS) $^ -r -o $@

my_kpm_$(MYKPM_VERSION)_debug.kpm: ${objs}
	${CC} $(CFLAGS) $(CFLAG) $(INCLUDE_FLAGS) $^ -r -o $@

# 清理
.PHONY: clean
clean:
	rm -f *.kpm