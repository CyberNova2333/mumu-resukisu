# MuMu ReSukiSU 项目指南

本项目解决的是“已有模拟器引擎怎样装入配套Root实现，并能验证与回退”。先有固定引擎证据和本地输入，再构建候选，最后才允许绑定实例写盘。它没有自动适配任意内核的能力。

公开项目为 [CyberNova2333/mumu-resukisu](https://cnb.cool/CyberNova2333/mumu-resukisu)。独立工具采用 [GPLv3](../LICENSE)，内核等继承组件保留各自许可，详见[来源清单](../THIRD_PARTY.md)。先跟随一次本地候选的交接，再查具体机制和修改位置。

## 阅读路径

| 读者目标 | 从这里开始 | 读完后获得什么 |
| --- | --- | --- |
| 理解一次操作 | [一个实际流程](walkthroughs/one-real-run.md) | 输入、构建、审计、写盘与恢复如何衔接。 |
| 实际构建和运行 | [构建与操作](../docs/build-and-use.md) | 必须准备的本地输入和命令。 |
| 获取或评估上游更新 | [发行版生命周期](modules/release-lifecycle.md) | 最新发行、固定构建版本和已验收升级之间的区别。 |
| 判断恢复能覆盖什么 | [镜像与恢复边界](modules/image-boundaries.md) | 哪些字节、磁盘和状态可以恢复，何时会拒绝。 |
| 定位需要修改的代码 | [代码地图](code-map.md) | 通用流程和目标适配的责任边界。 |
| 核对证据 | [源码证据](references/source-evidence.md) | 代码已实现什么，哪些仍缺设备验证。 |
| 复核指南是否完整 | [读者模拟](references/quality-review.md)、[同步记录](change-log.md) | 新读者能否理解边界，文档依据哪个提交。 |
| 查术语或贡献方法 | [术语表](glossary.md)、[贡献指南](../CONTRIBUTING.md) | 名称含义、验证与提交约定。 |

## 覆盖范围

本指南采用 Standard 结构，覆盖发行输入、固定引擎适配、原生和用户态构建、分区准备、整盘审计与实例事务。主流程只走一条固定 Android 15 x86_64 目标路径。Android 12、自动适配未知内核、发布安装器及通用 Root 工具兼容不在支持范围内。

历史实验为设计提供依据；当前项目的离线构建和故障测试不替代重构入口的设备验收。源码、配置和测试是实现依据，具体已运行检查见[验证记录](../docs/validation.md)。

## 运行与验证

准备 Python 3.11+，在仓库根安装可编辑包并运行主机测试：

```sh
python -m pip install -e .
python -m unittest discover -s tests -v
```

原生构建需要 Linux 或 WSL；实例绑定和独占写盘使用 Windows。独立文档站以 VitePress 默认主题引用本指南，安装和构建方法见[项目使用说明](../README.md)。站点只呈现指南正文，外部源码定位显示为仓库相对路径，避免把本地输入或执行目录带进静态产物。

证据状态：除特别标注外，本页基于当前源码已确认。
