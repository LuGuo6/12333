# KPM Template - APatch 内核补丁模块模板

基于 [APatch](https://github.com/bmax121/APatch) 框架的 KPM (Kernel Patch Module) 开发模板项目。

## 项目结构

```
kpm-template/
├── .github/
│   └── workflows/
│       └── build.yml          ← GitHub Actions 自动编译
├── kpm_utils.h                ← 公共工具库(ARM64指令解析/Hook封装)
├── my_kpm/                    ← 你的 KPM 模块目录
│   ├── my_kpm.c               ← 主源码(必须实现3个生命周期函数)
│   ├── my_kpm.h               ← 模块头文件
│   ├── utils.h                ← 模块专用工具
│   └── Makefile               ← 编译脚本
└── README.md
```

## 快速开始

### 1. Fork 本仓库到你的 GitHub

### 2. 添加 KernelPatch 子模块

```bash
git submodule add https://github.com/bmax121/KernelPatch.git KernelPatch
```

### 3. 本地编译

```bash
# 设置 NDK 环境(二选一)
export ANDROID_NDK_LATEST_HOME=/path/to/ndk

# 编译
cd my_kpm
make
```

### 4. GitHub Actions 自动编译

Push 代码到 GitHub 后, Actions 会自动编译。编译产物在 Actions 页面的 Artifacts 中下载。

## KPM 模块开发要点

### 必须实现的 3 个函数

| 函数 | 调用时机 | 返回值 |
|------|---------|--------|
| `inline_hook_init()` | 模块加载时 | 0=成功, 负数=错误码 |
| `inline_hook_control0()` | 用户态调用 `kpatch ctl0` 时 | 0=成功 |
| `inline_hook_cleanup()` | 模块卸载时 | 0=成功 |

### 典型开发流程

1. **查找内核函数**: 使用 `lookup_name()` 宏
2. **计算偏移量**: 通过分析 ARM64 指令动态获取
3. **安装 Hook**: 使用 `hook_func()` 宏
4. **编写回调**: 实现 `_before` / `_after` 回调函数

### 用户态使用

```bash
# 加载模块
kpatch $SUPERKEY kpm load /path/to/my_kpm.kpm

# 运行时控制
kpatch $SUPERKEY kpm ctl0 my_kpm "set_pid=1234"

# 卸载模块
kpatch $SUPERKEY kpm unload my_kpm
```

## 兼容性

- 架构: ARM64 (aarch64)
- 内核: Linux 4.4 ~ 6.6
- 编译: Android NDK Clang

## License

GPL-2.0-or-later