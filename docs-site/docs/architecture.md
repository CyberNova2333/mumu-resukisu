# 系统架构与数据流全景

MuMu ReSukiSU 将高风险的虚拟机磁盘修改拆分为三个职责隔离的运行域：**Linux 离线编译与分区组装域**、**Windows 主机独占控制写盘域**、**Android 15 早期启动预置与内核模块运行域**。只有通过前序全部指纹、符号与全盘字节门禁后，候选系统盘才允许在单实例上激活。

## 交互式架构全景图

<DiagramFrame
  src="/diagrams/resukisu-architecture.html"
  title="MuMu-ResuKSU 架构拓扑图（支持平移、缩放、节点属性探查）"
  aspect="950 / 308"
  new-tab-label="在新窗口全屏打开 ↗"
/>

## 三域分层与安全边界

### 1. 离线构建与全盘审计域（Linux / 跨平台 Python）

- **目标锁死与分区提取**：`src/mumu_resukisu/images.py` 依据 `targets/mumu-android15-x86_64/profile.json` 读取 `base.vdi` 与只读 `source.vdi` 差分盘，严格验证 `2,465,270,784` 虚拟字节及 `boot`、`system`、`vendor` 三分区原生摘要。
- **固定内核模块与无越界分区编辑**：
  - `scripts/build-native.py` 基于 `native/driver/target-profile.json` 导出固定 ReSukiSU 提交及 ACK 内核配置增量，编译 `kernelsu.ko` 与 `driver-init` 并验证 235 个未定义外部符号及模块结构布局。
  - `BootFs` 与分区脚本（`prepare-boot.py` / `prepare-system.py` / `prepare-vendor.py`）仅在原文件数据槽尺寸内部完成加载器、`/resukisu-migrate-prestage` 与 `ksud` 放置，同时通过替换 `vendor` 覆盖动作阻止模拟器在启动时回写原厂二进制。
- **全盘 COW 块审计**：`assemble` 与 `audit` 按照 VDI 的 1 MiB 块表（父盘继承、差分块映射、零块与未分配块）重新遍历全部虚拟字节，确保允许修改的分区外侧任意偏移（哪怕跨块边界 1 个字节）保持与原始快照一致。

### 2. 单实例绑定与原子写盘域（Windows 主机）

- **实时身份与三检停机门禁**：`src/mumu_resukisu/binding.py` 调用用户显式传入的 `MuMuManager.exe` 索引指定模拟器实例，实时校验实例关闭状态、内置 Root 关闭与磁盘文件路径一致性。
- **独占写锁与可回退事务**：`src/mumu_resukisu/controller.py` 在写盘前连续观察到三次实例稳定停机后，使用 Win32 `CreateFileW`（共享模式 `0`）结合非阻塞 `msvcrt.locking` 独占打开目标 `system-diff.vdi`，在同目录 `.state/<sha256>/` 下先写出完整 `rollback.vdi` 与原子状态日志再逐块写入并回读。

### 3. Android 早期启动与内核运行域（Guest Early-Boot）

- **早期引导门禁**：在 `switch_root` 之前，修改后的启动流调用 `driver-init`（仅限早期初始化上下文执行，非 PID 1 调用固定退出码 `64`）检查内核模块与参数 `norc=1 preinit=0`，并执行 `/resukisu-migrate-prestage`。
- **原厂守护进程安全迁移**：`native/prestage/stock-migration.h` 将数据盘持久备份与系统盘写盘回退明确分离。当探测到 `/system/xbin/dusk` 精准符合 `3753880` 字节原厂指纹时，先在数据盘耐久化原厂备份并复核，再发布受支持的 64 位 `ksud`；面对任何未知内容则保留证据中止，杜绝盲目覆盖。

## 延续阅读

- 追踪一次完整候选交接细节：阅读 [一次可回退安装全流程](/docs/walkthrough)
- 查看系统盘与数据盘分离边界：阅读 [镜像变化与恢复边界](/docs/image-boundaries)
- 查看源码模块与验证对应关系：阅读 [代码地图](/docs/code-map)
