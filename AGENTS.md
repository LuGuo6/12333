# AGENTS.md

## What this is

KPM (Kernel Patch Module) template and modules for the [APatch](https://github.com/bmax121/APatch) framework. Each subdirectory with a `Makefile` is an independent KPM module that compiles to a `.kpm` binary loaded into a Linux kernel on Android.

`KernelPatch/` is a git submodule (v0.13.2) providing the hook framework, headers, and build infrastructure.

## Build

Requires Android NDK (r27c used in CI). Set one of:
- `ANDROID_NDK_LATEST_HOME` (recommended)
- `ANDROID_NDK`

```bash
# Build a specific module
cd my_kpm && make        # release
cd my_kpm && make debug  # debug with logging

# Clean
cd my_kpm && make clean
```

CI (`.github/workflows/build.yml`) builds **every subdirectory that has a Makefile**, skipping `KernelPatch/`, `target/`, `.github/`, and `my_kpm/` (the template). Outputs go to `target/`. If you add a new module directory with a Makefile, CI will auto-build it.

## Architecture

Two hook API styles coexist in this repo — know which one you're using:

| Style | Used by | Hook API | Include |
|-------|---------|----------|---------|
| Template style | `my_kpm/` | `hook_func()` / `unhook_func()` macros via `kpm_utils.h` | `#include "../kpm_utils.h"` |
| Direct style | `cpuinfo_kirin9020/` | `hook_syscalln()` / `unhook_syscalln()` via `<hook.h>` | `#include <hook.h>`, `#include <kpmodule.h>` |

**Template style** (`my_kpm`): Implements 3 lifecycle functions directly (`inline_hook_init`, `inline_hook_control0`, `inline_hook_cleanup`). Uses `lookup_name()` macro to find kernel symbols, `hook_func()` to install hooks with before/after callbacks.

**Direct style** (`cpuinfo_kirin9020`): Uses KPM_INIT/KPM_CTL0/KPM_EXIT macros to register lifecycle functions. Hooks syscalls specifically via `hook_syscalln()`. Uses `hook_fargs*_t` parameter structs.

## Module structure

Every module needs:
- `Makefile` (copy from `my_kpm/Makefile` or `cpuinfo_kirin9020/Makefile`)
- A `.c` file implementing the lifecycle functions
- Include `KP_DIR ?= ../KernelPatch` in Makefile (relative path to submodule)

Key Makefile variables: `KP_DIR`, `ANDROID_NDK_LATEST_HOME`, `CFLAGS` (includes `-fno-PIC -fno-asynchronous-unwind-tables -fno-stack-protector`).

## Headers

- `kpm_utils.h` (repo root) — shared utilities: `lookup_name()`, `hook_func()`, `unhook_func()`, `task_uid()`, ARM64 instruction decoders
- `my_kpm/my_kpm.h` — template module's private structs (task_struct_offset, cred_offset)
- `cpuinfo_kirin9020/cpuinfo_kirin9020.h` — fake cpuinfo content for Kirin 9030
- `cpuinfo_xuanjie_o1/cpuinfo_xuanjie_o1.h` — fake cpuinfo content for Xiaomi XuanJie O1
- `gpuinfo/gpuinfo.h` — fake GPU sysfs content for Adreno (骁龙) + Mali (天玑)

KernelPatch headers live in `KernelPatch/kernel/` under `include/`, `patch/include/`, `linux/include/`, `linux/arch/arm64/include/`.

## Gotchas

- ARM64 only (aarch64). No 32-bit support.
- Kernel versions: Linux 4.4 – 6.6.
- The `cpuinfo_kirin9020` module's Makefile produces `cpuinfo_kirin9030_*.kpm` (naming mismatch with directory name — this is intentional, the module targets Kirin 9030).
- `my_kpm/` is excluded from CI builds — it's a template, not a production module.
- Offsets for kernel structs (task_struct, cred) are computed **at runtime** via instruction analysis, not hardcoded. This is how KPM handles kernel version differences.
- The repo root has `cpuinfo_小米玄戒O1.prop` and `cpuinfo_Kirin9030_华为.prop` — these are reference prop files used as source data for the fake cpuinfo modules.
