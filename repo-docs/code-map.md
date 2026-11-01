# 代码地图

范围包括本仓库维护的构建、镜像及实例事务流程。先按 [实际流程](walkthroughs/one-real-run.md) 建立操作顺序，再根据改动目的选择入口。

| 路径 | 职责 | 关键代码 | 与主流程的关系 |
| --- | --- | --- | --- |
| `src/mumu_resukisu/` | Python命令、VDI、启动文件系统、实例事务 | `cli.main`、`images.audit`、`BootFs`、`Controller` | 从本地输入到安装回退。 |
| `native/driver/` | 固定内核适配 | `resukisu.patch`、`ack-task-layout.patch`、目标描述 | 决定ABI与源码适配边界。 |
| `native/loader/` | PID1加载器 | `loader.c` | 在早期启动校验并加载配套模块。 |
| `native/prestage/` | 数据预置 | `prepare.c`、`catalog.h` | 在Android接手前检查或补齐daemon。 |
| `native/probes/` | 受限验证程序 | 授权、过滤器、32位调用、只读检查、脚本用例 | 构建阶段模型与另行运行验收。 |
| `native/common/` | 小型通用辅助实现 | `sha256.h` | 固定内容摘要检查。 |
| `native/tools/frida/` | 受限实际工具用例 | supervisor、target、注入脚本 | 独立构建；设备编排尚待迁移。 |
| `native/images/` | 固定ext文件编辑 | `system-edit.c` | 保留不相关inode、内容和属性。 |
| `scripts/` | 构建与分区准备入口 | `build-*`、`prepare-*` | 调用既有工具链，写新本地输出。 |
| `targets/` | 引擎指纹与分区范围 | `profile.json` | 所有镜像操作的输入边界。 |
| `tests/` | Python主机故障模型 | `test_images.py`、`test_controller.py` | 拒绝错误输入、验证失败与回退路径。 |

修改命令参数从 [CLI](../src/mumu_resukisu/cli.py) 开始；修改写盘条件从 [绑定](../src/mumu_resukisu/binding.py) 和 [控制器](../src/mumu_resukisu/controller.py) 开始；修改差分语义从 [镜像实现](../src/mumu_resukisu/images.py) 开始。原生构建入口分别为 [内核与加载器](../scripts/build-native.py) 和 [用户态辅助工具](../scripts/build-userspace.py)。

## src/mumu_resukisu/

`cli.py` 解析使用者动作并连接目标描述；`binding.py` 把本地实例身份固定下来，`controller.py` 管理事务和恢复。`images.py` 按VDI父子块语义审计完整有效盘，`bootfs.py` 只处理已核验启动文件系统。相关验证在 `tests/`。

## native/driver/

`resukisu.patch` 改变内核适配行为，`ack-task-layout.patch` 补充目标结构布局。固定符号名和目标描述供构建器拒绝接口漂移；更新目标需要独立证据和重新验收。

## native/loader/

加载器校验配套模块，限制运行上下文并转交原始启动流程。构建器生成模块摘要头，不能使用旧头匹配新模块；主机门禁及加载模型随构建执行。

## native/prestage/

启动预置负责daemon目录检查、支持清单比对、缺失时原子补齐。修改支持版本同时影响清单、固定内容摘要和编译输入，不能只编辑设备中的JSON。

## native/probes/

探针按授权、过滤器、直接32位查询、32位su、正常只读状态与脚本跨启动场景拆分。它们只验证各自受限用例，不是通用应用调试器。

## native/common/

独立摘要实现供加载器和辅助工具校验固定制品。修改后需要重跑依赖它的原生主机模型。

## native/images/

`system-edit.c` 通过libext2fs修改固定inode。调用方负责源摘要、独占新输出、回读、inode与块范围审计，不能单独去掉调用方门禁使用。

## native/tools/frida/

监督器固定官方服务制品并建立私有只读安全策略视图；目标和注入脚本只验证本项目自建进程。`build-frida.py` 编译并执行主机模型，当前不包含设备连接和附加编排。

## scripts/

构建脚本生成模块、加载器和辅助工具；准备脚本产生kernel、boot、system和vendor候选。`register-release.py`核验官方发行制品，生成待审核记录。它们由使用者显式调用，导入Python包不会自动开始构建或设备操作。

## targets/

顶层目标描述固定磁盘容量、原始分区摘要和允许范围，并指向原生编译约束。新增引擎是新增适配任务，不能放宽旧摘要来“支持”。

## tests/

Python测试采用临时磁盘夹具和假管理器；Windows文件句柄、链接和锁边界使用真实本地文件接口。C与汇编模型由对应构建脚本运行，结果在忽略的构建清单中。

## Coverage

覆盖以上维护源码及目标描述。排除用户原始镜像、完整上游源码、设备运行数据、外部工具链、发布安装器和云端流水线。这些不是源码树中已实现的服务。

证据状态：除特别标注外，本页基于当前源码已确认。
