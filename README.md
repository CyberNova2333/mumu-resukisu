<h1 align="center">MuMu ReSukiSU</h1>

<p align="center">
  <strong>面向固定 MuMu 引擎的 Root 适配、离线构建与可回退安装工具</strong>
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPLv3-2563eb?style=flat-square" alt="独立工具许可：GPLv3"></a>
  <a href="targets/mumu-android15-x86_64/profile.json"><img src="https://img.shields.io/badge/Target-Android%2015%20x86__64-3ddc84?style=flat-square" alt="目标：Android 15 x86_64"></a>
  <a href="#开始使用"><img src="https://img.shields.io/badge/Python-3.11%2B-3776ab?style=flat-square" alt="Python 3.11 及以上"></a>
</p>

<p align="center">
  <a href="#开始使用">开始使用</a> · <a href="repo-docs/README.md">项目文档</a> · <a href="docs/build-and-use.md">构建与操作</a> · <a href="CONTRIBUTING.md">贡献指南</a>
</p>

为特定 MuMu Android 15 x86_64 引擎适配 ReSukiSU，提供驱动补丁、启动工具、本地镜像组装，以及绑定单个实例的安装和回退流程。

当模拟器自带的 Root 实现无法满足新工具的接口要求时，本项目从用户自己的原始镜像构建替换候选，核对允许变动的分区，再对指定的停止实例执行可回退安装。**目前只支持目标描述中指纹完全匹配的一版引擎，Android 12 不受支持。**

此前的独立实验已经验证冷启动、32/64位调用、用户脚本与自建进程 Frida 附加。本仓库是这套实现的参数化提取；重构入口现已完成固定目标上的安装与回退流程、原厂daemon初装迁移、授权前拒绝，以及授权后的 Root、后续冷启动和固定32位 su 双入口验证。另行通过本地受限编排完成单次已有 Firefox 进程附加及回收；通用 Firefox 编排入口、脚本三阶段生命周期和32位 daemon 仍未验收，分阶段结果见 [验证记录](docs/validation.md)。它不是通用刷机工具，也不是官方 ReSukiSU 或 MuMu 发行版。


## 阅读路径

| 读者目标 | 从这里开始 | 读完后获得什么 |
| --- | --- | --- |
| 准备输入并构建候选 | [开始使用](#开始使用) → [构建与操作](docs/build-and-use.md) | 环境、外部输入、分区准备与命令顺序。 |
| 理解安装为何可回退 | [项目指南](repo-docs/README.md) → [主流程](repo-docs/walkthroughs/one-real-run.md) | 原盘、候选、绑定实例和恢复事务怎样交接。 |
| 获取后续上游发行版 | [发行版获取与兼容性](repo-docs/modules/release-lifecycle.md) | 如何查询最新版，以及下载后还需通过哪些验证。 |
| 修改实现 | [代码地图](repo-docs/code-map.md)、[贡献指南](CONTRIBUTING.md) | 每类修改的入口、测试与提交约定。 |
| 核对支持范围和依据 | [镜像边界](repo-docs/modules/image-boundaries.md)、[源码证据](repo-docs/references/source-evidence.md) | 已实现的保护、历史实验与待做的设备验收。 |

## 开始使用

需要 Python 3.11+；原生构建和分区处理使用 Linux／WSL，实例绑定与写盘使用 Windows。当前以源码检出方式使用，安装 Python 命令后仍需保留仓库中的目标描述、原生源码和脚本。

```powershell
git clone https://github.com/CyberNova2333/mumu-resukisu.git
cd mumu-resukisu
python -m venv .venv
.venv/Scripts/python -m pip install -e .
.venv/Scripts/Activate.ps1
mumu-resukisu --help
mumu-resukisu profile --profile targets/mumu-android15-x86_64/profile.json
python -m unittest discover -s tests -v
```

完整的输入准备、离线构建和回退命令见 [构建与操作](docs/build-and-use.md)。首次阅读建议从 [一个实际流程](repo-docs/walkthroughs/one-real-run.md) 开始，再用 [代码地图](repo-docs/code-map.md) 定位修改位置。

## 获取最新版

查询每次直接访问官方发行元数据，不依赖仓库内手写版本号。默认查询正式发行；若上游只有预发行，命令会明确失败，需要显式选择包含预发行的通道：

```powershell
mumu-resukisu fetch-release --check
mumu-resukisu fetch-release --channel prerelease --check
mumu-resukisu fetch-release --channel prerelease --output inputs/latest-release
```

下载结果进入新的本地目录，保留制品摘要和源码提交。下载完成不会更新已适配版本、支持清单或模拟器。上游仓库更名及登记、构建、设备验收的区别见 [发行版生命周期](repo-docs/modules/release-lifecycle.md)。

## 包含的成果

| 部分 | 当前内容 |
| --- | --- |
| 原生适配 | 完整 ReSukiSU 内核补丁、目标 ABI 头增量、旧初始化停用工具、PID1 加载器。 |
| 用户态 | 启动前检查、受支持 daemon 保留与缺失补齐、授权／过滤器／32位调用探针、跨启动脚本用例。 |
| 实际工具用例 | Frida受限监督器、自建目标和注入脚本源码；独立构建与主机模型。设备侧运行编排仍待迁移。 |
| 镜像工具 | 固定启动文件数据槽编辑、system 文件替换、vendor 覆盖动作移除、VDI 差分组装和整盘对照。 |
| 安装回退 | 显式实例绑定、稳定停机、路径与磁盘身份核对、独占句柄、持久事务、精确系统盘恢复。 |
| 验证 | 不需要模拟器的主机模型、临时文件故障测试，以及单独保存的本地构建清单。 |

镜像、APK、游戏、构建产物、实例标识、本机配置和原始运行记录都不进入源码仓库。上游源码使用固定版本的外部输入，适配补丁在本仓库维护。

## 维护边界

已有 daemon 只有匹配支持清单才会保留；未知版本会原样保留并阻止继续启动，不自动降级。只有一个真实发行版得到验证，不能把可维护清单当成任意版本升级兼容。

回退只恢复系统差分盘，数据盘中的管理器、授权和应用数据不会随之撤销。当前支持冷启动与停机回退；不支持热卸载，不更改全局安全策略，不写共享基础盘。原目标处于 Permissive，不能据此推断 Enforcing、第三方模块或所有 Root 工具可用。

独立工具默认采用 GPL-3.0-or-later，内核补丁和部分组件保留不同许可。详见 [来源与许可](THIRD_PARTY.md)，发布二进制还需履行对应源码与依赖声明义务。

## 文档站

指南正文维护在 `repo-docs/`，独立站点以 VitePress 默认主题引用同一份正文。准备 Node.js 22+，在仓库根运行：

```powershell
pnpm --dir docs-site install --frozen-lockfile
pnpm --dir docs-site run docs:build
pnpm --dir docs-site run docs:dev -- --host 127.0.0.1
```

输出仅位于 `docs-site/.vitepress/dist/`。指南链接在站内跳转，指南外的源码链接显示为仓库相对路径；本地输入、镜像、配置和源码文件不会复制到站点。构建文档不会构建原生工具、写盘或部署。
