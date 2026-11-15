# 常见问题与排错诊断

本页梳理在构建校验、镜像审计、Windows 写盘及早期启动阶段遇到拒绝或异常时的实际根因与处理方式。本工具的所有“失败即停止”均属于安全门禁，禁止通过篡改配置摘要或强行删除锁文件绕过。

## 1. 提取或重组直接拒绝并报错指纹不匹配

- **典型现象**：`mumu-resukisu extract` 或 `assemble` 报错提示基础盘 `base.vdi` 虚拟容量不等于 `2465270784` 字节，或 `boot` / `system` / `vendor` 分区 SHA256 与档案不符。
- **根本原因**：输入盘快照来自非目标 MuMu Android 15 `x86_64` 版本、曾开启其它 Root/修改工具，或在实例未完全停止时复制了脏差分盘。
- **排查方式**：

```powershell
mumu-resukisu profile --profile targets/mumu-android15-x86_64/profile.json
```

核对本地基础盘与完全冷停机实例下的原始干净快照；如输出目录已经生成失败残留，下一次重试必须使用新的输出路径，不要在原目录直接覆盖。

## 2. `build-native.py` 提示未定义符号缺失（例如少 `shmem_file_setup`）

- **典型现象**：离线编译过程中模块虽编出，但精确符号集合门禁在核对 `235` 个未定义外部符号时拒绝退出。
- **根本原因**：内核配置增量未正确应用或内核 UAPI 头文件未显式导出安装（历史阶段曾因未找到 UAPI 头静默关闭预初始化特性导致少导出符号）。
- **解决方式**：使用 `--kernel-archive` 提供的完整原始 ACK 归档与未修改的 `--kernel-config`，从全新的空 `--output` 目录重新执行 `scripts/build-native.py`，确保配置补丁与 `headers_install` 步骤完整通过（不得放宽符号门禁，也不得手工修改 `preinit=0` 默认行为）。

## 3. `bind` 或 `install` 拒绝并提示实例未稳定停机或开启了内置 Root

- **典型现象**：Windows 控制器拒绝绑定或提示未满足三次连续停机观察，或检测到模拟器设置开启了官方自带 Root。
- **根本原因**：`src/mumu_resukisu/binding.py` 与 `controller.py` 会实时调用 `MuMuManager.exe` 检查实例运行态及内置 Root 选项，防止写入期间虚拟机后台进程或自带 `su` 破坏分区快照一致性。
- **解决方式**：在 MuMu 界面或管理器中彻底关闭目标实例（同时关闭同一实例的后台常驻选项及内置 Root），等待数秒后重新运行 `mumu-resukisu inspect` 或 `install`。

## 4. Windows 执行 `install` / `restore` 报共享冲突或无法加锁

- **典型现象**：打开 `system-diff.vdi` 或 `.state/` 事务文件时出现句柄共享拒绝（`CreateFileW` 共享参数 `0`）或非阻塞写锁失败（`msvcrt.locking`）。
- **根本原因**：目标实例仍在运行、另一终端正在操作同一 `bindings/<name>.json`，或杀毒/网盘同步软件正在占用虚拟磁盘及备份文件。
- **解决方式**：确认已退出 MuMu 主进程及所有占用 `.vdi` 的程序，确保仅由单个控制进程持有一份绑定目录执行读写，勿复制多份绑定 JSON 并发写盘。

## 5. `install` 写盘中断或外部改写了磁盘后如何恢复

- **典型现象**：执行 `restore` 时，系统拒绝按旧备份覆盖或报错外部字节与预期不一致。
- **根本原因**：`controller.py` 严格区分“写前外部变更”与“控制器自身写盘中途失败”：
  - 若写入开始前发现系统盘已被第三方或模拟器改动，拒绝用旧快照盲目抹除外部状态；
  - 若写入半途异常中断，先尝试在同一独占句柄内按事务备份恢复自身写过的字节；若遇到未知中断残局，则保留现场日志避免猜测破坏。
- **解决方式**：首先运行 `mumu-resukisu status` 检查最新事务 ID 与状态说明，再使用该明确事务号运行 `restore`；严禁手动删改 `.state/` 下的备份或校验记录。

## 6. 原厂 `/system/xbin/dusk` 迁移失败或启动期中止

- **典型现象**：首次启动 `resukisu-migrate-prestage` 遇到非预期内容或文件属性异常，保留当前文件并阻止继续执行切换。
- **根本原因**：早期启动迁移程序（`native/prestage/stock-migration.h`）仅允许精准匹配 `stock-daemon.json`（`3753880` 字节）的原厂文件先做数据盘持久备份与核验再发布 `ksud`；对任何来源不明的可执行体不会贸然覆盖。
- **解决方式**：使用未改动过 `dusk` 的干净原始系统盘重走离线组装与安装；若需检查迁移逻辑，可在 Linux 上通过 `scripts/build-userspace.py` 附带的 18 项 ASAN/UBSAN 场景测试定位。

## 7. `fetch-release` / `register-release` 已拿新版 APK，为什么构建仍不通过？

- **典型现象**：成功从上游仓库拉取到了最新 APK 或源码归档，但在构建原生模块或准备预置组件时仍提示不支持。
- **根本原因**：发行下载、离线登记与实际支持清单更新是三条严格隔离的阶段：下载器只核对文件下载摘要，登记器只记录维护者审核证据并保持 `catalogUpdated: false`，只有完成补丁审查、代码修改与实机冷启动检验后才会更改固定提交与 `supported-daemons.json`。详见 [发行版获取与兼容性边界](/docs/release-lifecycle)。

## 8. 在 Linux 下执行 `unittest` 为何显示 `skipped=29`？

- **典型现象**：执行 `python -m unittest discover -s tests -v` 输出 `OK (skipped=29)`。
- **根本原因**：`tests/test_controller.py` 中有 29 项模拟 Windows 独占句柄共享冲突与原子回退故障场景的用例依赖 Win32 API（`CreateFileW` / `msvcrt`），在 Linux 平台按设计自动跳过，其余 48 项跨平台逻辑与边界测试全部执行。完整的写盘故障矩阵应在 Windows Python 3.11+ 环境补测。
