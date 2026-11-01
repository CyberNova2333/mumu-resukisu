# 来源与许可

本仓库是多许可源码集合。独立编写的 Python 工具、启动加载器、用户态辅助程序和测试按 GPL-3.0-or-later 发布，全文见 [LICENSE](LICENSE)。内核适配补丁和镜像编辑器按下表分别处理；根许可不改变上游文件原有许可。

| 范围 | 来源与固定基线 | 本仓库处理 |
| --- | --- | --- |
| `native/driver/` 中 ReSukiSU 内核适配补丁 | ReSukiSU 提交 `5785bdb8091304322f597f67d50e0b79f9815cb2`；其 [kernel/LICENSE](https://github.com/ReSukiSU/ReSukiSU/blob/5785bdb8091304322f597f67d50e0b79f9815cb2/kernel/LICENSE) 为 GPLv2 | 补丁保留原文件上下文与版权，新增适配按 GPL-2.0-only；上游完整源码由构建者提供，仓库不附带模块二进制。 |
| `native/driver/` 中 ACK 头文件增量补丁 | 固定 Android Common Kernel 源码，具体提交与 ABI 见对应目标配置 | 保留目标头的 SPDX／版权；只存必要增量，不把整份 Linux 头改标为 GPLv3。 |
| `native/loader/`、`native/prestage/`、`native/probes/`、`native/common/`、`native/tools/frida/` | 本次适配任务编写的独立程序与主机测试 | GPL-3.0-or-later。共享 SHA256 实现从本项目先前加载器提取，是独立实现而非复制的第三方密码库；固定算法常量不表示引入了外部库。 |
| `native/images/system-edit.c` | 本项目使用 libext2fs API 编写的镜像编辑器 | GPL-2.0-only，单独构建。libext2fs 的具体文件混有 LGPLv2、GPLv2；以所用发行版和上游 [NOTICE](https://github.com/tytso/e2fsprogs/blob/master/NOTICE) 为准，不假定整个库是单一宽松许可。 |
| 配套管理器与 daemon | ReSukiSU 同一固定发行版；项目根 [LICENSE](https://github.com/ReSukiSU/ReSukiSU/blob/5785bdb8091304322f597f67d50e0b79f9815cb2/LICENSE) 为 GPLv3，子目录可有不同条款 | 仅保存版本、摘要和接口约束，不复制 APK 或 `.so`；来源鉴别不能代替对应源码义务。 |

GPLv2 和 GPLv3 全文分别保存在 [licenses/GPL-2.0.txt](licenses/GPL-2.0.txt) 与 [licenses/GPL-3.0.txt](licenses/GPL-3.0.txt)。这些许可文本保留原文，不受本项目源码标记覆盖。

## 外部构建依赖

- musl：静态用户态程序通过系统安装的 musl 构建。musl 主体使用 MIT，部分来源有独立声明；发行二进制时应保留实际链接版本的版权与许可说明。项目不打包 libc 源码或库。
- GCC、binutils、Clang／LLVM、Python：由用户现有工具链提供，不在源码仓库中复制；编译器运行库和例外条款以实际工具链为准。
- e2fsprogs／libext2fs：用于本地镜像编辑和只读检查，源码和库不捆绑。
- Unicorn 与 pyelftools：仅主机模型测试使用，由用户单独安装，不进入 Android 产物。
- Frida 官方服务器：仅本地工具验收输入，服务器及 Python client 不捆绑；本仓库只提供独立编写的监督器、目标和注入脚本。
- zstd 及其它打包工具：作为本地工具调用，不捆绑可执行文件。

## 输入与发布边界

MuMu 的虚拟磁盘、内核原镜像、Android 分区、用户数据和商业应用不由本仓库授权再分发。使用者从自己的安装中取得输入，构建结果保存在本地；本项目提供的是源码、适配补丁与可核验操作流程。

本次重构未发布安装包。后续若发布模块、静态工具或镜像增量，需要针对实际产物保留完整来源、许可证和对应源码；单有本文件不代表完成了二进制发行所需的全部材料。
