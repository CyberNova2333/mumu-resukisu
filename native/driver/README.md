# 固定内核的驱动适配

目标是描述文件中 SHA256 完全相同的 Android 15 x86_64 内核。这里保存的是可审查增量，构建时从用户提供的固定上游源码产生独立副本；不打包原厂内核、完整系统镜像或管理器 APK。

## 输入与离线构建

在 Linux 或 WSL 中运行项目脚本。需要 Python 3.12+、pyelftools、Git、make、musl-gcc、pahole 1.25、bc、bison、flex、libelf 开发文件及固定 Clang 工具链。脚本不下载资源，也不调用模拟器。

```sh
python scripts/build-native.py \
  --resukisu-source /path/to/upstream-git \
  --kernel-archive /path/to/ack.tar.gz \
  --kernel-config /path/to/config.gz \
  --clang /path/to/clang-r536225 \
  --target-symbols /path/to/kallsyms \
  --work-dir /path/to/new-linux-build-directory \
  --output build/native
```

`--check-inputs` 只做输入门禁。`--output` 必须是项目 `build/` 下的新目录；`--work-dir` 也必须不存在，源码、头文件和 Kbuild 输出保留于其中，避免在 Windows 挂载盘解包大量小文件。不会覆盖输入或旧输出。

| 输入 | 固定来源与验证 |
| --- | --- |
| ReSukiSU Git | 提交 `5785bdb8091304322f597f67d50e0b79f9815cb2`。使用 `git archive`，工作树修改不会被复制。 |
| ACK tar.gz | [固定 ACK 提交归档](https://android.googlesource.com/kernel/common/+archive/c034535679d056565875fec24925fd21309040c5.tar.gz)，整包摘要写在 `target-profile.json`。 |
| Clang | [固定 Android Clang 归档](https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/70a12c4fe88940ab2b0e6d763492dffa2f07e15a/clang-r536225.tar.gz)。脚本核对版本族，配置生成后还要求配置摘要完全相同；这不是任意编译器兼容证明。 |
| 配置 | 当前目标原始 `/proc/config.gz` 的只读导出，或从同一原内核的 IKCONFIG 提取；解压摘要必须为 `6a60ae3570151e23df0344c628afeea58b4b3cad5ad6e204839141a737291680`。也接受已准备的固定配置摘要。 |
| 核心符号 | 用户对该目标取得的本地 `kallsyms`，每行 `非零十六进制地址 类型 名称`。拒绝地址为0和带模块所有者的条目。只用名称交叉核对，地址不编入 loader。 |

如果已有该目标还原的带符号 ELF，可用 `nm --defined-only kernel-with-symbols.elf > kallsyms` 得到相同输入形状。符号还原本身是单独分析步骤；本项目不把未验证 ELF 的任意地址当作来源证明。原始 bzImage 与解压 ELF 的固定摘要另由准备内核步骤核对。

## 配置为什么能重建

`prepare-config.patch` 精确描述原始配置到 ACK 构建配置的差异：更新已核对工具链标识、加入两个编译链接能力标志、更新 pahole 版本，移除 ACK 中不存在的厂商 Kconfig 项。目标原配置已经关闭 CFI、FORTIFY、JUMP_LABEL 和 DEBUG_LIST，本项目没有将这些目标状态误写为新关闭的保护措施。

脚本在独立 Kbuild 目录应用补丁，要求结果摘要为 `2a8e4358afe32008951a92df1d2785441cff7d96a8cae176cfe9b7b50f8eb9e7`，执行 `olddefconfig` 后再核对一次。如果宿主工具改变配置，构建会停下来，不会把新增差异静默接受为兼容。

## 三份适配补丁

- `resukisu.patch`：完整正常启动候选的内核适配，包括 PID1 限制、初始化入口更名、24 字节任务布局相关访问、最小 seccomp 快照及 IA32 调用入口。保留上游 UAPI 符号链接，不带旧 Windows 抽取造成的链接删除。
- `ack-task-layout.patch`：只为该目标头文件加入24字节不透明保留区；不推断厂商字段名称或语义，也不修改 ACK 原源码。
- `build-metadata.patch`：将固定提交的版本编号35253写为构建元数据，移除上游构建期间的 Git 计数／联网补全浅克隆逻辑。适配后仍需运行验收，固定编号不是兼容证明。

上游 kernel 及这些派生补丁适用 GPL-2.0-only；ACK 派生内容保留 Linux 许可边界。项目其他独立工具的许可不改变这部分许可。

## 输出与局限

`manifest.json` 记录实际模块、PID1 loader 的摘要和静态检查结果。检查包括235个精确外部符号、模块结构大小960字节及初始化／退出偏移、正常RC接线、加载器绑定测试和非PID1宿主拒绝。重构后构建结果始终标为 `deviceValidated:false`，只有独立新运行才能改变验收结论。

不支持热卸载。加载器参数固定 `norc=1 preinit=0`，不会打开 Shell 自动授权。Android 12、不同内核、任意第三方模块和 Enforcing 环境不在当前已证范围内。

## 原生 KSU 初始化的离线处理

```sh
python scripts/prepare-kernel.py \
  --kernel /path/to/original-bzImage \
  --elf /path/to/original-kernel.elf \
  --output build/kernel
```

只有固定摘要的输入可以通过。脚本在原有内核命令行缓冲区增加 `initcall_blacklist=kernelsu_init`；新模块使用单独初始化入口。为放入原压缩槽，只清零不属于任何程序段的分析元数据，保留完整重定位尾部、段布局和所有可执行代码。随后独立解压交叉核验、核对修改范围并重算原格式CRC。输出新文件，不写安装镜像或切换全局安全策略。
