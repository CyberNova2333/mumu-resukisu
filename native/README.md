# 用户态辅助程序

启动预置程序保留已核验的守护进程，文件缺失时补齐；遇到精确匹配的原厂守护进程时，先备份再迁移到配套版本；其它未知内容保留原件并返回失败。它由启动包调用，失败时上层必须停止进入 Android。当前目标的数据挂载仍固定为已分析的 `sdc3`，UAPI 为 6，发布版本为固定兼容清单中的一项；这些是适配条件，不是本机路径配置。

验收程序分别检查原生授权、过滤器子进程、32 位入口、管理器身份，以及跨启动测试脚本的执行。`normal-check` 的管理器 UID 在构建时显式提供，不再沿用先前测试实例的 UID。迁移 fixture 与 rearm 会修改自身测试目录和固定 daemon，应由拥有完整回退和阶段检查的测试流程使用，不能当成日常清理命令。

## 本地构建

Linux 或 Windows＋WSL 需要 `musl-gcc`、`gcc`（支持 i386 freestanding 编译）、binutils，以及装有 `unicorn` 和 `pyelftools` 的 Python。模型必须在普通 Linux 用户下运行。脚本不安装依赖、不下载上游程序，也不连接设备。

在忽略的 `bindings/` 中创建本地 JSON，`expectedManagerUid` 填入目标实例实际查询到的 UID，`daemonFile` 指向已自行取得的配套 x64 守护进程。配置字段仅允许 `schemaVersion`、`expectedManagerUid`、`daemonFile`；版本值为 1。另以 `--stock-daemon` 提供从固定原始 system 镜像提取的原厂文件，用于迁移模型。两份输入均核对固定摘要，且不会被执行。不要把示例中的含义字符串照抄成配置值。

```text
python scripts/build-userspace.py --config bindings/userspace.json --stock-daemon inputs/stock-dusk --check-inputs
python scripts/build-userspace.py --config bindings/userspace.json --stock-daemon inputs/stock-dusk --output build/userspace
```

Windows 可追加 `--wsl-distro <发行版> --wsl-user <普通用户>`；需要指定模型 Python 时使用 `--model-python <Linux可执行路径>`。默认使用 WSL 的默认发行版和用户；若默认用户为 root，构建会拒绝。脚本将输出限制在新的 `build/` 子目录，不覆盖先前报告。

配套文件只通过已固定的完整 SHA256 和 ELF 身份验收，绝不执行输入文件来询问版本。当前编译清单只有一条官方发行版，新增版本需要重新核对来源、UAPI、探针及启动逻辑，不能只在配置中换一个哈希。

原厂输入由 `prestage/stock-daemon.json` 和编译头独立固定，允许的动作仅是保留备份后迁移，不能当作受支持的 UAPI 6 守护进程继续运行。备份冲突、重试与数据盘回退边界见[镜像变化与恢复边界](../repo-docs/modules/image-boundaries.md)。

## 产物与验证

`manifest.json` 列出八份可执行文件的内容摘要、ELF 位数与测试结果；`build-log.json` 保留本机编译和模型输出。这些文件、生成的 UID 头文件和二进制全部位于忽略的 `build/`，不入库。

构建必须完成六组既有 ASAN／UBSAN 主机模型、新增原厂迁移模型、32 位调用者模型、11 项直接 FD 模型、非特权宿主拒绝门禁及静态 ELF 检查，才能记录 `host_tests_passed_not_device_validated`。迁移模型包含 18 项临时文件用例，覆盖备份冲突、输入变化和失败后重试。模型模拟的是接口返回或 root 元数据，实际文件操作位于临时目录；它们不证明真实断电恢复或 Android 设备验收。

`prestage/`、`probes/`、`common/` 为本项目独立编写的辅助程序。`driver/` 的上游内核补丁按其自身许可处理，不能套用辅助程序的 GPLv3 标记。详细清单见[第三方来源与许可](../THIRD_PARTY.md)。

## 登记后续官方版本

`scripts/register-release.py` 接受明确的 APK 摘要、源码提交、对应关系已审查标志，以及本机 apksigner JAR／aapt 路径。它核对实际 Git 提交与 UAPI 头是否未改、完成 APK 签名校验、检查包身份与唯一 x64 ELF，然后在新的 `build/` 子目录生成 `entry.json` 和 `proof.json`。不会执行 APK 内程序，也不会直接修改受支持清单。

```text
python scripts/register-release.py --apk <本地APK> --expected-apk-sha256 <已核对摘要> --source-root <源码Git目录> --source-revision <完整提交> --reviewed-release-association --apksigner-jar <本地JAR> --aapt <本地工具> --output build/release-review
```

APK 与源码对应关系仍包含维护者的显式审查，不是可复现构建证明。生成登记材料后还需要审查并更新编译清单、探针版本、预置程序和目标测试；当前构建器会拒绝未经代码审查的新条目，防止把材料登记误当成升级兼容通过。

实际工具源码另见 [Frida 自有进程验收](tools/frida/README.md)。
