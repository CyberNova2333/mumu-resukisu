# 项目全貌与完整构建流程

`mumu-resukisu` 面向指纹固定的 MuMu Android 15 `x86_64` 模拟器引擎，提供一条先离线验证、后绑定单实例写入且可按事务恢复原系统盘的 Root 镜像构建工具链。仓库不分发原始内核、系统磁盘、第三方 APK 或已编译工具链，所有本地输入均由使用者在受忽略目录自行准备。

::: warning 严格作用域边界
所有构建与写入步骤默认先对照 `targets/mumu-android15-x86_64/profile.json` 与 `native/driver/target-profile.json` 中的指纹和容量门禁。发现任何指纹、分区边界或未定义符号集合不匹配时，应当立即停止，禁止通过手动放宽期望摘要强行放行未知引擎或 Android 12 目标。
:::

## 运行环境与跨平台分工

项目把**离线编译与分区组装**同**Windows 主机磁盘写入**严格解耦：

| 阶段 | 推荐平台 | 核心依赖 | 产物目录 |
| --- | --- | --- | --- |
| 主机单元测试与下载查询 | Windows / Linux | Python 3.11+、`mumu-resukisu` 可编辑安装 | `inputs/` |
| 原生模块、内核、用户态与分区准备 | Linux / WSL | Clang/LLVM、make、musl-gcc、gcc、binutils、pyelftools、Unicorn、e2fsprogs、`libext2fs` 开发头、`zstd`、`liblz4`、Android NDK Clang（可选 Frida 工具） | `build/` |
| VDI 重组审计、实例绑定、写盘与恢复 | Windows（审计与组装亦可离线在 Linux 验证） | `MuMuManager.exe`、Windows 文件独占写句柄 | `bindings/` 与 `.state/` |

首先在仓库根目录安装 Python 包并执行离线主机测试：

```sh
python -m pip install -e .
python -m unittest discover -s tests -v
```

> 在离线环境缺少在线构建依赖且系统已有 `setuptools` 时，可改用 `python -m pip install -e . --no-build-isolation`。在 Linux 上执行 `unittest` 会跳过依赖 Windows 独占句柄 API 的 29 项测试用例（总计 77 项用例，完整覆盖需在 Windows 验证）。

## 第一步：准备本地输入与提取原始分区

在受 Git 忽略的 `inputs/` 目录中放入：

- 已停止测试实例对应的共享基础盘 `base.vdi` 与系统差分盘副本 `source.vdi`；
- 与当前固定引擎匹配的原始 `kernel`、未压缩 `kernel.elf` 与核对用核心符号清单 `core-kallsyms.txt`；
- 固定提交的 ReSukiSU 上游 Git 检出目录（`inputs/resukisu`）、ACK 内核归档（`inputs/ack.tar.gz`）与目标内核配置（`inputs/prepared.config`）；
- 与支持清单匹配的配套 64 位守护进程 `ksud` 及从原始 system 提取的原厂 `/system/xbin/dusk` 副本（`inputs/stock-dusk`）。

### 查询或下载上游发行版（独立辅助通道）

查询与下载命令只面向已登记的官方上游仓库身份（原 ReSukiSU 目前迁移至 BakaSU），写入独立的 `inputs/latest-release` 目录，不会改写构建目标版本：

```powershell
mumu-resukisu fetch-release --check
mumu-resukisu fetch-release --channel prerelease --check
mumu-resukisu fetch-release --channel prerelease --output inputs/latest-release
```

- 默认仅查询正式版（stable）；当上游尚无正式版时默认模式直接失败，不会静默降级为预发行版。
- 下载源码归档仅用于版本调查或配合维护者显式检出的 Git 提交执行后续版本评估，不可直接作为 `--resukisu-source`（构建器要求真实 Git 工作树与提交历史）。版本登记与签名核对见 [发行版获取与兼容性边界](/docs/release-lifecycle)。

### 验证镜像指纹并提取分区

在确认输入盘快照匹配目标档案后，提取原始 `boot`、`system`、`vendor` 分区镜像：

```powershell
mumu-resukisu profile --profile targets/mumu-android15-x86_64/profile.json
mumu-resukisu extract --profile targets/mumu-android15-x86_64/profile.json --base inputs/base.vdi --source inputs/source.vdi --output build/extracted
```

输出目录必须尚不存在。若因指纹或虚拟容量不符而失败，系统会保留失败快照现场以便排查，重试必须换用新输出目录。

## 第二步：离线构建内核模块、加载器与处理内核

在 Linux 或 WSL 终端中运行原生构建与内核重打包（路径使用 POSIX 风格）：

```sh
python3 scripts/build-native.py \
  --resukisu-source inputs/resukisu \
  --kernel-archive inputs/ack.tar.gz \
  --kernel-config inputs/prepared.config \
  --clang inputs/clang \
  --target-symbols inputs/core-kallsyms.txt \
  --output build/native

python3 scripts/prepare-kernel.py \
  --kernel inputs/kernel \
  --elf inputs/kernel.elf \
  --output build/kernel
```

关键门禁说明：

- `build-native.py` 依据 `native/driver/target-profile.json` 固定提交导出源码，不带入本地未提交修改，并在新工作目录应用内核配置增量并核对配置摘要。
- 完成后会严格校验 `kernelsu.ko` 的 235 个未定义外部符号、`vermagic=6.1.90-perf+ `、模块结构尺寸（960 字节）与 `driver-init` 加载器完整性，并执行加载器主机模型。
- `prepare-kernel.py` 只处理已核验原内核，同时执行双解压交叉核对、原压缩槽尺寸边界校验与 CRC 校验。

## 第三步：构建用户态预置工具与迁移模型

先创建本地配置 `bindings/userspace.json`（参见 [输入契约、配置与脚本参数](/docs/configuration) 中的三键字段约束）：

- `schemaVersion`: 固定为 `1`
- `expectedManagerUid`: 绑定目标实例内实际核对到的管理器应用 UID（不可照抄其他实例）
- `daemonFile`: 配套 `ksud` 本地路径（当前仅接受匹配 `native/prestage/supported-daemons.json` 摘要的固定版本）

随后运行用户态构建并传入从原系统镜像提取的原厂 `dusk` 样本：

```sh
python3 scripts/build-userspace.py \
  --config bindings/userspace.json \
  --stock-daemon inputs/stock-dusk \
  --output build/userspace
```

该步骤同时在主机编译并执行 `stock-unit` 等用户态与 ASAN/UBSAN 失败注入模型（覆盖 32 位调用接口及原厂迁移状态机），失败时非零退出。产出清单中的 `deviceValidated: false` 为正常隔离边界，表示静态及模型构建尚未经过目标实例冷启动验证。

可选工具：如需实验性构建与 Frida 相关的独立辅助组件，可单独执行 `scripts/build-frida.py`；当前仓库主流程尚未迁入自动附加编排，不计入正式交付入口。

## 第四步：修改三分区并重组审计完整候选 VDI

依次准备修改后的 `boot`、`system` 与 `vendor` 分区镜像：

```sh
python3 scripts/prepare-boot.py \
  --source build/extracted/boot.img \
  --kernel build/kernel/kernel \
  --loader build/native/driver-init \
  --module build/native/kernelsu.ko \
  --native-manifest build/native/manifest.json \
  --userspace-manifest build/userspace/manifest.json \
  --prestage build/userspace/resukisu-migrate-prestage \
  --output build/boot

python3 scripts/prepare-system.py \
  --source build/extracted/system.img \
  --daemon inputs/ksud \
  --output build/system

python3 scripts/prepare-vendor.py \
  --source build/extracted/vendor.img \
  --output build/vendor
```

分区写入边界说明：

- `prepare-boot.py` 维持原镜像数据槽大小不变，嵌入内核、加载器及 `/resukisu-migrate-prestage` 早期启动预置工具。
- `prepare-system.py` 通过 `e2fsprogs` / 原生编辑工具仅修改目标守护进程路径及分配元数据并执行 `e2fsck` 核验。
- `prepare-vendor.py` 仅替换一条固定固件/启动覆盖条目，避免厂商服务每次启动覆盖守护进程。
- 修改后保留原 AVB 尾部，但校验树已不再匹配原签名，因此不可将候选盘视为通用已验签镜像。

接着生成差分候选 VDI 并触发全盘字节级审计：

```powershell
mumu-resukisu assemble \
  --profile targets/mumu-android15-x86_64/profile.json \
  --base inputs/base.vdi \
  --source inputs/source.vdi \
  --partition boot=build/boot/boot.img \
  --partition system=build/system/system-candidate.img \
  --partition vendor=build/vendor/vendor.img \
  --output build/candidate
```

命令将生成 `build/candidate/candidate.json` 与 `build/candidate/audit.json`，按真实 VDI COW 块语义核验全部 `2,465,270,784` 虚拟字节，确保允许修改的三分区以外无任何越界改动，同时也可通过 `mumu-resukisu audit` 独立复审已有候选。

## 第五步：单实例绑定、安装写入与事务恢复

在 Windows 主机上显式指定已停止的测试实例编号与文件路径，生成绑定档案（绝不自动扫描或猜测默认实例）：

```powershell
mumu-resukisu bind \
  --profile targets/mumu-android15-x86_64/profile.json \
  --manager '<MuMu安装目录>/nx_main/MuMuManager.exe' \
  --index '<实例编号>' \
  --instance-directory '<该实例目录>' \
  --private-disk '<该实例目录>/system-diff.vdi' \
  --base '<共享基础盘路径>' \
  --output bindings/test.json
```

`bind` 会实时核查实例身份、停机状态与内置 Root 关闭开关，并将本机信息写入被 Git 忽略的 `bindings/test.json`。请务必保持绑定文件与其同目录派生出的 `.state/<sha256>/` 事务目录相邻，切勿复制绑定文件对同一实例并行维护两套事务状态。

写入、查看状态与回退的标准命令链如下：

```powershell
mumu-resukisu inspect --profile targets/mumu-android15-x86_64/profile.json --binding bindings/test.json --candidate build/candidate/candidate.json
mumu-resukisu install --profile targets/mumu-android15-x86_64/profile.json --binding bindings/test.json --candidate build/candidate/candidate.json
mumu-resukisu status --profile targets/mumu-android15-x86_64/profile.json --binding bindings/test.json
mumu-resukisu restore --profile targets/mumu-android15-x86_64/profile.json --binding bindings/test.json --transaction '<status返回的事务号>'
```

核心事务保障（详见 [镜像变化与恢复边界](/docs/image-boundaries)）：

- `install` 前必须通过三次连续停机轮询与前置预检，拿到 Windows 独占句柄（`CreateFileW` 共享模式为 `0` + 非阻塞 `msvcrt.locking` 锁定）并先持久化完整 `rollback.vdi` 原盘备份，才会落盘候选差分字节并执行回读校验。
- `install` 仅负责写入与回读，不自动重启实例；后续冷启动后才在 Android 早期启动阶段执行数据盘 `/system/xbin/dusk` 原厂迁移与 Root 授权验证。
- `restore` 只依赖事务备份与原子日志恢复私有系统盘，即使构建目录或候选文件已被清除仍可执行恢复；恢复操作不会改写或清空 Android 数据盘上的原厂备份与管理器授权记录。
