# 代码地图

范围包括本仓库维护的构建、镜像及实例事务流程。先按 [实际流程](walkthroughs/one-real-run.md) 建立操作顺序，再根据改动目的选择入口。

| 路径 | 职责 | 关键代码 | 与主流程的关系 |
| --- | --- | --- | --- |
| `src/mumu_resukisu/` | Python命令、VDI、启动文件系统、实例事务 | `cli.main`、`images.audit`、`BootFs`、`Controller` | 从本地输入到安装回退。 |
| `native/driver/` | 固定内核适配 | `resukisu.patch`、`ack-task-layout.patch`、目标描述 | 决定ABI与源码适配边界。 |
| `native/loader/` | PID1加载器 | `loader.c` | 在早期启动校验并加载配套模块。 |
| `native/prestage/` | 数据预置 | `prepare.c`、`catalog.h`、`stock-migration.h` | 在Android接手前保留、补齐或迁移已知daemon。 |
| `native/probes/` | 受限验证程序 | 授权、过滤器、32位调用、只读检查、脚本用例 | 构建阶段模型与另行运行验收。 |
| `native/common/` | 小型通用辅助实现 | `sha256.h` | 固定内容摘要检查。 |
| `native/tools/frida/` | 受限实际工具用例 | supervisor、target、注入脚本 | 独立构建；设备编排尚待迁移。 |
| `native/images/` | 固定ext文件编辑 | `system-edit.c` | 保留不相关inode、内容和属性。 |
| `scripts/` | 构建与分区准备入口 | `build-*`、`prepare-*` | 调用既有工具链，写新本地输出。 |
| `targets/` | 引擎指纹与分区范围 | `profile.json` | 所有镜像操作的输入边界。 |
| `tests/` | Python主机故障模型 | `test_images.py`、`test_controller.py` | 拒绝错误输入、验证失败与回退路径。 |
| `docs-site/` | 独立默认主题文档站 | 站点配置、链接转换、正文引用页 | 将同一份指南转换成独立静态页面。 |

修改命令参数从 [CLI](../src/mumu_resukisu/cli.py) 开始；修改写盘条件从 [绑定](../src/mumu_resukisu/binding.py) 和 [控制器](../src/mumu_resukisu/controller.py) 开始；修改差分语义从 [镜像实现](../src/mumu_resukisu/images.py) 开始。原生构建入口分别为 [内核与加载器](../scripts/build-native.py) 和 [用户态辅助工具](../scripts/build-userspace.py)。

## `src/mumu_resukisu/`

`cli.py` 解析使用者动作并连接目标描述；`binding.py` 把本地实例身份固定下来，`controller.py` 管理事务和恢复。`images.py` 按VDI父子块语义审计完整有效盘，`bootfs.py` 只处理已核验启动文件系统。相关验证在 `tests/`。

| 重要代码 | 功能 | 关键符号 | 调用方 / 使用方 | 验证入口 |
| --- | --- | --- | --- | --- |
| `cli.py` | Python 命令入口，分发查询、构建与实例动作。 | `main` | 命令入口、准备脚本和实例控制器 | `test_cli_boot.py`、命令帮助 |
| `releases.py` | 获取官方发行、固定提交和本地下载清单；由 CLI 调用。 | — | 命令入口、准备脚本和实例控制器 | `test_releases.py` |
| `images.py` | 镜像准备和安装前重算都使用完整虚拟盘对照。 | `Vdi`、`audit`、`assemble` | 命令入口、准备脚本和实例控制器 | `test_images.py` |
| `bootfs.py` | 启动准备脚本编辑固定文件槽并构造提前失败路径。 | `BootFs`、`initramfs` | 命令入口、准备脚本和实例控制器 | `test_cli_boot.py` |
| `binding.py` | 绑定与控制器共同核对路径、身份和独占句柄。 | `create_binding`、`exclusive` | 命令入口、准备脚本和实例控制器 | `test_controller.py` |
| `controller.py` | 保存备份、阶段日志、写盘及恢复；由 CLI 组装。 | `Controller` | 命令入口、准备脚本和实例控制器 | `test_controller.py` |

## `native/driver/`

`resukisu.patch` 改变内核适配行为，`ack-task-layout.patch` 补充目标结构布局。固定符号名和目标描述供构建器拒绝接口漂移；更新目标需要独立证据和重新验收。

| 重要代码 | 功能 | 关键符号 | 调用方 / 使用方 | 验证入口 |
| --- | --- | --- | --- | --- |
| `resukisu.patch`、`ack-task-layout.patch` | 分别适配上游行为和目标结构布局，由原生构建器应用。 | — | 原生构建器 | `build-native.py` 的精确补丁与 ABI 检查 |
| `prepare-config.patch`、`build-metadata.patch` | 固定构建配置及离线版本信息。 | — | 原生构建器 | 原生构建清单 |
| `target-profile.json`、`undefined-symbols.json` | 编译指纹及允许的符号集合。 | — | 原生构建器 | `verify_module` |

## `native/loader/`

加载器校验配套模块，限制运行上下文并转交原始启动流程。构建器生成模块摘要头，不能使用旧头匹配新模块；主机门禁及加载模型随构建执行。

| 重要代码 | 功能 | 关键符号 | 调用方 / 使用方 | 验证入口 |
| --- | --- | --- | --- | --- |
| `loader.c` | PID1 检查、模块摘要与符号绑定；放入启动包。 | — | 启动包 | `test-loader.c`、非 PID1 宿主拒绝检查 |

## `native/prestage/`

启动预置负责daemon目录检查、支持清单比对、缺失时原子补齐，以及原厂版本的备份迁移。修改支持版本同时影响清单、固定内容摘要和编译输入，不能只编辑设备中的JSON。迁移状态与恢复边界见[机制说明](modules/image-boundaries.md)。

| 重要代码 | 功能 | 关键符号 | 调用方 / 使用方 | 验证入口 |
| --- | --- | --- | --- | --- |
| `prepare.c` | 启动包运行的预置程序；未知 daemon 保留并失败。 | — | 启动包和用户态构建器 | `unit.c` |
| `catalog.h`、`pins.h`、`supported-daemons.json` | 编译与登记审查共同维护的固定支持输入。 | — | 启动包和用户态构建器 | `build-userspace.py` / `verify_catalog` |
| `stock-migration.h` | 精确原厂输入的备份、发布和失败后重试。 | `stock_backup_durable`、`stock_retire`、`migrate_stock` | `prepare.c` 在观察脚本检查之后调用 | `stock-unit.c` |
| `stock-pin.h`、`stock-daemon.json` | 仅允许迁移的原厂文件身份，不加入可运行版本清单。 | — | 启动包和用户态构建器 | `build-userspace.py` / `verify_stock` |

## `native/probes/`

探针按授权、过滤器、直接32位查询、32位su、正常只读状态与脚本跨启动场景拆分。它们只验证各自受限用例，不是通用应用调试器。

| 重要代码 | 功能 | 关键符号 | 调用方 / 使用方 | 验证入口 |
| --- | --- | --- | --- | --- |
| `native/probes/grant/probe.c`、`native/probes/filtered-grant/probe.c` | 授权以及过滤器子进程用例。 | — | 用户态构建器及另行设备验收 | 各目录的 `unit.c` |
| `native/probes/direct32/direct32.S`、`native/probes/su32/probe.c` | 直接 32 位接口与 32 位 su 调用。 | — | 用户态构建器及另行设备验收 | `model-test.py`、`unit32.c` |
| `native/probes/normal-check/check.c` | 正常启动状态、管理器 UID 和配套 daemon 检查。 | — | 用户态构建器及另行设备验收 | `native/probes/normal-check/unit.c` |
| `native/probes/migration-fixture/fixture.c`、`90-resukisu-migration-test.sh` | 构造明确的跨启动脚本用例。 | — | 用户态构建器及另行设备验收 | `native/probes/migration-fixture/unit.c` |
| `native/probes/migration-rearm/recovery.c` | 同一测试用例的恢复与重新准备。 | — | 用户态构建器及另行设备验收 | `native/probes/migration-rearm/unit.c` |

## `native/common/`

独立摘要实现供加载器和辅助工具校验固定制品。修改后需要重跑依赖它的原生主机模型。

| 重要代码 | 功能 | 关键符号 | 调用方 / 使用方 | 验证入口 |
| --- | --- | --- | --- | --- |
| `sha256.h` | 独立摘要计算，供预置程序与探针使用。 | — | 用户态辅助程序 | 用户态构建的对应主机模型 |

## `native/images/`

`system-edit.c` 通过libext2fs修改固定inode。调用方负责源摘要、独占新输出、回读、inode与块范围审计，不能单独去掉调用方门禁使用。

| 重要代码 | 功能 | 关键符号 | 调用方 / 使用方 | 验证入口 |
| --- | --- | --- | --- | --- |
| `system-edit.c` | 只修改已核验的文件布局，由 system 准备脚本调用。 | — | system 准备脚本 | `prepare-system.py` 的回读、块范围和 e2fsck 检查 |

## `native/tools/frida/`

监督器固定官方服务制品并建立私有只读安全策略视图；目标和注入脚本只验证本项目自建进程。`build-frida.py` 编译并执行主机模型，当前不包含设备连接和附加编排。

| 重要代码 | 功能 | 关键符号 | 调用方 / 使用方 | 验证入口 |
| --- | --- | --- | --- | --- |
| `supervisor.c`、`sha256.h` | 核验指定服务、设置受限执行环境和回收自有进程。 | — | 独立工具构建器 | `unit.c` |
| `target.c`、`agent.js` | 固定自有测试进程与注入动作。 | — | 独立工具构建器 | `build-frida.py` 的 ELF 与导出检查；设备附加另行验收 |

## `scripts/`

构建脚本生成模块、加载器和辅助工具；准备脚本产生kernel、boot、system和vendor候选。`register-release.py`核验官方发行制品，生成待审核记录。它们由使用者显式调用，导入Python包不会自动开始构建或设备操作。

| 重要代码 | 功能 | 关键符号 | 调用方 / 使用方 | 验证入口 |
| --- | --- | --- | --- | --- |
| `fetch-resukisu.py`、`register-release.py` | 获取发行输入和离线制品登记，供维护者审查。 | — | 显式命令调用 | `test_releases.py`、`test_release_registration.py` |
| `build-native.py`、`build-userspace.py`、`build-frida.py` | 构建分工及对应主机模型；输出供分区准备使用。 | — | 显式命令调用 | 各构建脚本的最终清单；`test_userspace_build.py` |
| `prepare-kernel.py` | 固定内核检查及旧初始化停用候选。 | — | 显式命令调用 | `test_kernel_prepare.py`、两种解压及范围审计 |
| `prepare-boot.py`、`prepare-system.py`、`prepare-vendor.py` | 构造三份配套替换分区。 | — | 显式命令调用 | 文件回读、固定范围及产物摘要检查 |

## `targets/`

顶层目标描述固定磁盘容量、原始分区摘要和允许范围，并指向原生编译约束。新增引擎是新增适配任务，不能放宽旧摘要来“支持”。

| 重要代码 | 功能 | 关键符号 | 调用方 / 使用方 | 验证入口 |
| --- | --- | --- | --- | --- |
| `targets/mumu-android15-x86_64/profile.json` | CLI、镜像审计和控制器的目标边界。 | — | CLI、镜像审计和控制器 | `load_profile`、`test_images.py` |

## `tests/`

Python测试采用临时磁盘夹具和假管理器；Windows文件句柄、链接和锁边界使用真实本地文件接口。C与汇编模型由对应构建脚本运行，结果在忽略的构建清单中。

| 重要代码 | 功能 | 关键符号 | 调用方 / 使用方 | 验证入口 |
| --- | --- | --- | --- | --- |
| `test_controller.py`、`test_images.py` | 身份、越界、并发、部分写入与恢复故障模型。 | — | 主机测试入口 | `python -m unittest discover -s tests -v` |
| `test_cli_boot.py`、`test_kernel_prepare.py` | 启动与命令边界、内核解码模型。 | — | 主机测试入口 | 同一主机测试入口 |
| `test_userspace_build.py`、`test_release_registration.py`、`test_releases.py` | 构建输入、离线登记与网络下载边界。 | — | 主机测试入口 | 同一主机测试入口 |

## `docs-site/`

站点只把指南正文转换成静态页面，使用 VitePress 默认主题。源码树之外的本地输入不作为站点资源；文档生成不运行设备命令。

| 重要代码 | 功能 | 关键符号 | 调用方 / 使用方 | 验证入口 |
| --- | --- | --- | --- | --- |
| `docs-site/.vitepress/config.mjs` | 默认主题导航、搜索及 Markdown 处理。 | — | VitePress 文档构建 | `npm --prefix docs-site run docs:build` |
| `docs-site/.vitepress/guide-links.mjs` | 站内页面路由与仓库源码定位显示。 | — | VitePress 文档构建 | `npm --prefix docs-site test` |
| `index.md`、`modules/`、`walkthroughs/`、`references/` | 仅 include 原指南，无第二套正文。 | — | VitePress 文档构建 | 文档构建的内部链接检查 |

## 覆盖范围

覆盖以上维护源码及目标描述。排除用户原始镜像、完整上游源码、设备运行数据、外部工具链、发布安装器和云端流水线。这些不是源码树中已实现的服务。

证据状态：除特别标注外，本页基于当前源码已确认。
