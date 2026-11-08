---
title: 快速入门
description: MuMu ReSukiSU 是专为网易 MuMu 模拟器深度适配的**非侵入式 KernelSU 内核桥接与细粒度权限管理工具**。
---

# 快速入门

MuMu ReSukiSU 是专为网易 MuMu 模拟器深度适配的**非侵入式 KernelSU 内核桥接与细粒度权限管理工具**。

## 解决的核心痛点

- **痛点 1**：传统 Android 模拟器获取 Root 必须全盘重新刷入已被修改的内核，破坏系统镜像完整性
- **痛点 2**：传统全局 su 二进制文件缺乏细粒度的应用名单授权，极易被恶意应用滥用
- **痛点 3**：宿主机与模拟器之间的 IPC 桥接缺乏防护与版本锁定

---

## 环境准备

Windows 10/11 操作系统，已安装网易 MuMu 模拟器（Android 12/15 内核版）。

---

## 快速安装与运行

```bash
# 1. 克隆工具库
git clone https://github.com/CyberNova2333/mumu-resukisu.git
cd mumu-resukisu

# 2. 检查 MuMu 模拟器运行状态与 ADB 连通性
python check_env.py

# 3. 一键向 MuMu 模拟器挂载 KernelSU 桥接驱动
python install_bridge.py
```

---

## 接下来探索

- 深入了解系统内部流转：查阅 [系统架构与设计](/docs/architecture)
- 探索全部配置参数与高级场景：查阅 [配置与使用指南](/docs/configuration)
- 遇到问题排错：查阅 [常见问题与排错](/docs/faq)
