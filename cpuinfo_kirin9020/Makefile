# ============================================================
# cpuinfo_kirin9020 KPM 模块 Makefile
# ============================================================

CPUINFO_VERSION := 1.0.0

ifndef KP_DIR
    KP_DIR = ../KernelPatch
endif

OS_NAME = $(shell uname | tr A-Z a-z)
MACHINE = $(shell uname -m)
HOST_TAG = $(OS_NAME)-$(MACHINE)
ifeq ($(HOST_TAG), darwin-arm64)
    HOST_TAG = darwin-x86_64
endif
NDK_BIN_DIR := toolchains/llvm/prebuilt/$(HOST_TAG)/bin
ifdef ANDROID_NDK_LATEST_HOME
    NDK_PATH ?= $(ANDROID_NDK_LATEST_HOME)/$(NDK_BIN_DIR)
else ifdef ANDROID_NDK
    NDK_PATH ?= $(ANDROID_NDK)/$(NDK_BIN_DIR)
endif

ifdef TARGET_COMPILE
    CC := $(TARGET_COMPILE)gcc
    LD := $(TARGET_COMPILE)ld
else ifdef NDK_PATH
    CC := $(NDK_PATH)/aarch64-linux-android31-clang
    LD := $(NDK_PATH)/ld.lld
endif

CFLAGS = -Wall -O2 -fno-PIC -fno-asynchronous-unwind-tables -fno-stack-protector -fno-common -DCPUINFO_VERSION=\"$(CPUINFO_VERSION)$(CPUINFO_VER)\"

INCLUDE_DIRS := . include patch/include linux/include linux/arch/arm64/include linux/tools/arch/arm64/include

INCLUDE_FLAGS := $(foreach dir,$(INCLUDE_DIRS),-I$(KP_DIR)/kernel/$(dir))

objs := cpuinfo_kirin9020.c

all: base

base: cpuinfo_kirin9020_$(CPUINFO_VERSION).kpm

debug: CFLAGS += -DCONFIG_DEBUG
debug: CPUINFO_VER := _d
debug: cpuinfo_kirin9020_$(CPUINFO_VERSION)_debug.kpm

cpuinfo_kirin9020_$(CPUINFO_VERSION).kpm: ${objs}
	${CC} $(CFLAGS) $(CFLAG) $(INCLUDE_FLAGS) $^ -r -o $@

cpuinfo_kirin9020_$(CPUINFO_VERSION)_debug.kpm: ${objs}
	${CC} $(CFLAGS) $(CFLAG) $(INCLUDE_FLAGS) $^ -r -o $@

.PHONY: clean
clean:
	rm -f *.kpm