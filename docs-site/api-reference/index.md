---
layout: page
sidebar: false
title: API 与组件参考
description: MuMu ReSukiSU 的 10 个 CLI 子命令（profile/extract/assemble/audit/bind/inspect/install/status/restore/fetch-release）、8 组构建脚本与 JSON 契约参考。
---

<script setup lang="ts">
import { apiSpecZh } from '../.vitepress/data/api-spec'
</script>

<ApiExplorer :spec="apiSpecZh">

### 目标档案核对与原始分区提取 (`mumu-resukisu profile` / `extract`) {#profile-and-extract}

校验 `targets/mumu-android15-x86_64/profile.json`（`virtualBytes: 2465270784`）与基础/分区摘要后只读提取 `boot`、`system`、`vendor`，拒绝 Android 12 或未指纹化磁盘。

### 原生内核模块与重打包内核构建 (`scripts/build-native.py` / `prepare-kernel.py`) {#build-native-and-kernel}

在 Linux/WSL 编译 `kernelsu.ko`（`vermagic=6.1.90-perf+ `、`moduleStructBytes: 960`）与 `driver-init`，检查 235 个未定义符号并运行非 PID 1 退出 64 断言。

### 用户态预置工具与 `boot`/`system`/`vendor` 分区构建 (`build-userspace.py` / `prepare-*.py`) {#build-userspace-and-partitions}

要求三键 `bindings/userspace.json`、固定 64 位 `ksud`（`6,001,344` B）与原厂 `dusk`（`3,753,880` B），产出清单显式标明 `deviceValidated: false`。

### 差分盘合成与全盘 COW 字节审计 (`mumu-resukisu assemble` / `audit`) {#assemble-and-audit}

按 1 MiB VDI COW 语义重读全部 `2,465,270,784` 虚拟字节并生成 `candidate.json` 与 `audit.json`，确保三分区之外零越界修改。

### 单实例绑定、预检与 Windows 独占锁事务写入 (`bind` / `inspect` / `install`) {#bind-inspect-install}

通过 `MuMuManager.exe` 与 Win32 `CreateFileW(share=0)` 先备份完整 `rollback.vdi` 再写入私有差分盘并校验回读，写后不自动重启。

### 事务状态查询与原盘原子恢复 (`status` / `restore`) {#status-and-restore}

不依赖构建产物即可依据 `.state/<sha256>/rollback.vdi` 还原私有系统盘，且不改写 Android `/data` 备份与授权记录。

### 上游版本独立查询下载与待审登记 (`fetch-release` / `register-release.py`) {#fetch-and-register-release}

仅查询官方登记仓库且默认不静默降级至预发行版，`register-release.py` 始终保持 `catalogUpdated: false`。

</ApiExplorer>
