# 输入契约、配置与脚本参数

本页汇总 `mumu-resukisu` 当前源码树中已固定的目标描述、原生门禁清单、用户态配置契约及 CLI/脚本入口参数，用于核对构建边界与排错，不包含运行时生成的 `.local/` 或实例私有数据。

## 目标描述：`targets/mumu-android15-x86_64/profile.json`

`profile.json` 是全局安全准绳，规定唯一受支持的引擎快照物理指纹与允许写入的分区偏移范围：

| 字段 / 区域 | 当前源码取值 / 约束 | 校验点与拒绝语义 |
| --- | --- | --- |
| 目标代号与 ABI | MuMu Android 15 `x86_64` | 仅校验精确字节指纹，不以系统版本字符串作为兼容依据。 |
| 虚拟磁盘逻辑容量 (`virtualBytes`) | `2465270784` 字节 | `extract`、`assemble` 与 `audit` 均重算全盘容量，不符立即拒绝。 |
| `base` 与原始三分区摘要 | 固定 `base`、`boot`、`system`、`vendor` SHA256 与区间范围 | 提取时逐一核对；重组时强制分区外有效字节和未修改块保持原样。 |
| `refactoredDeviceValidation` | `false` | 代表当前正式重构入口尚未完成全量验收；只增补分阶段台账，不追溯改写离线清单。 |

## 原生构建描述与门禁清单

原生模块、预置迁移工具与内核配置门禁分布在 `native/` 下的 JSON 清单中：

| 清单文件 | 关键已确认约束 | 对应校验逻辑 |
| --- | --- | --- |
| `native/driver/target-profile.json` | 固定源码提交、`vermagic=6.1.90-perf+ `、`versionCode: 35253`、`uapiVersion: 6`、`moduleStructBytes: 960`、`norc=1 preinit=0`、精确 `235` 个未定义外部符号 | `scripts/build-native.py` 导出对应 Git 提交构建 `kernelsu.ko` 与 `driver-init`，缺失任一符号或模块结构偏移不符即中止。 |
| `native/prestage/supported-daemons.json` | 固定配套 64 位 `ksud` 文件尺寸 `6001344` 字节与对应 SHA256 (`d93436...`) | `prepare-boot` / `build-userspace` 与早期启动预置程序共同拦截清单之外的未知 daemon。 |
| `native/prestage/stock-daemon.json` | 原厂 `/system/xbin/dusk` 固定字节大小 `3753880` 与对应 SHA256 (`c87a46...`) | 仅用于识别原厂待迁移样本；匹配时先在数据盘耐久备份并校验再替换，未知字节直接保护性失败。 |

## 用户态绑定输入：`bindings/userspace.json`

`scripts/build-userspace.py` 要求传入的 JSON 配置文件严格包含三个字段，缺项或多余字段均视为配置错误：

```json
{
  "schemaVersion": 1,
  "expectedManagerUid": 10123,
  "daemonFile": "inputs/ksud"
}
```

| 字段 | 类型与校验约束 | 说明 |
| --- | --- | --- |
| `schemaVersion` | 整数 `1` | 模式版本号，非 `1` 直接拒绝。 |
| `expectedManagerUid` | 整数，`10000..99999` | 绑定目标实例上由使用者核对确认的 ReSukiSU 管理器应用 UID，严禁复制其他实例。 |
| `daemonFile` | 非空文件路径字符串 | 指向本地待嵌入或校验的配套 `ksud` 文件路径，必须与 `supported-daemons.json` 指纹一致。 |

> 可选实验工具 `scripts/build-frida.py` 另固定了 Frida 17.19.0 服务端二进制尺寸约束（`116223160` 字节）。

## 主包 CLI 子命令全览 (`mumu-resukisu`)

主包入口 `src/mumu_resukisu/cli.py` 共暴露 10 个子命令：

| 子命令 | 运行环境 | 关键输入参数 | 核心职责与输出 |
| --- | --- | --- | --- |
| `fetch-release` | 跨平台（联网） | `--channel`、`--check`、`--output` | 查询或下载已核对仓库的官方正式/预发行资源至新本地目录并写入清单。 |
| `profile` | 跨平台 | `--profile` | 解析并打印目标指纹、分区偏移与安全配置边界。 |
| `extract` | 跨平台 | `--profile`、`--base`、`--source`、`--output` | 在严格校验容量与基准摘要后，从 VDI 差分链导出原始 `boot`、`system`、`vendor` 分区。 |
| `assemble` | 跨平台 | `--profile`、`--base`、`--source`、`--partition boot=... system=... vendor=...`、`--output` | 合成仅修改目标分区块的新差分 VDI，执行整盘字节重读审计并生成 `candidate.json` 与 `audit.json`。 |
| `audit` | 跨平台 | `--profile`、`--candidate`（或对应盘链参数） | 对已有候选盘重跑全盘 VDI COW 字节完整性验证。 |
| `bind` | Windows | `--profile`、`--manager`、`--index`、`--instance-directory`、`--private-disk`、`--base`、`--output` | 现场核查 `MuMuManager.exe` 状态、实例停机和内置 Root 关闭后写入单实例绑定文件。 |
| `inspect` | Windows | `--profile`、`--binding`、`--candidate` | 仅做干跑预检：重算候选、实例当前磁盘、父盘身份及状态锁可行性，不写磁盘。 |
| `install` | Windows | `--profile`、`--binding`、`--candidate` | 经三次停机稳态确认并持有独占句柄和原子备份后写入私有系统盘并校验回读。 |
| `status` | Windows | `--profile`、`--binding` | 读取绑定实例旁边 `.state/<sha256>/` 下的最新事务编号、写盘阶段与备份校验状态。 |
| `restore` | Windows | `--profile`、`--binding`、`--transaction` | 核对目标停机状态与原备份摘要后把私有系统盘逐字节还原为事务前快照。 |

## 构建与登记辅助脚本 (`scripts/*.py`)

| 脚本路径 | 运行平台 | 核心输入参数 | 输出目录与作用 |
| --- | --- | --- | --- |
| `scripts/build-native.py` | Linux / WSL | `--resukisu-source`、`--kernel-archive`、`--kernel-config`、`--clang`、`--target-symbols`、`--output` | 导出固定源码、编译 `kernelsu.ko` 与 `driver-init` 并完成 235 符号门禁和加载器模型测试。 |
| `scripts/prepare-kernel.py` | Linux / WSL | `--kernel`、`--elf`、`--output` | 校验原内核与 ELF 指纹、执行双解压与压缩槽边界检查并重打包内核。 |
| `scripts/build-userspace.py` | Linux / WSL（支持从 Windows 调 WSL） | `--config`、`--stock-daemon`、`--output` | 编译预置迁移器与工具集，运行包含 ASAN/UBSAN 失败注入与 32 位模型在内的主机测试。 |
| `scripts/prepare-boot.py` | Linux / WSL | `--source`、`--kernel`、`--loader`、`--module`、`--native-manifest`、`--userspace-manifest`、`--prestage`、`--output` | 按固定数据槽容量注入加载器、内核模块与早期预置脚本/程序生成 `boot.img`。 |
| `scripts/prepare-system.py` | Linux / WSL | `--source`、`--daemon`、`--output` | 只修改目标 daemon 节点并保留 ext4 分配完整性校验后输出 `system-candidate.img`。 |
| `scripts/prepare-vendor.py` | Linux / WSL | `--source`、`--output` | 精确替换厂商启动覆盖行为防止覆盖 Root 守护进程，生成 `vendor.img`。 |
| `scripts/register-release.py` | Linux / 跨平台 | 官方下载清单、APK 签名、源码工作树与输出路径 | 核对 APK 签名、UAPI 与包身份并生成 `entry.json` 与 `proof.json`（显式标记 `catalogUpdated: false`）。 |
| `scripts/build-frida.py` | Linux / WSL + NDK | `--ndk-clang` 及服务端输入 | 独立构建可选 Frida 注入辅助与监督二进制并校验固定尺寸。 |

## 受忽略本地目录规范

为防止环境凭据、私有设备绑定和巨大磁盘镜像误入版本控制，`.gitignore` 已隔离以下工作路径（详见 `AGENTS.md`）：

- `inputs/`：原始 VDI 快照、上游源码、ACK 归档、内核与 APK。
- `build/`：提取分区、编译中间产物、候选差分盘与审计日志。
- `bindings/` 与 `.state/`：单实例绑定信息、`userspace.json`、`rollback.vdi` 原盘备份及事务日志。
- `.local/`：本地离线运行凭证或私有实验留档。
