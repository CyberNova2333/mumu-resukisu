# 从输入到回退

以下命令使用占位路径，不包含已授权实例的身份，也不自动选择模拟器。先对照目标指纹；不匹配时停止，不能通过改期望摘要强行放行。

## 1. 准备本地输入

在忽略目录 `inputs/` 保存原始 base、停止实例的系统差分盘副本、原始内核及其 ELF、固定上游源码／ACK 归档、目标配置与核心符号证据。APK 中的配套 daemon 同样由用户本地提供。不要把原始磁盘复制进源码目录或发布仓库。

原生构建需要 Clang／LLVM、make、musl-gcc、gcc、binutils，以及 pyelftools；用户态模型另需 Unicorn。分区处理需要 e2fsprogs、libext2fs 开发头、zstd。依赖由用户已有环境提供，脚本不会自动下载或安装软件。

在已停止的实例上取得输入快照后，可提取三个目标分区：

```powershell
mumu-resukisu extract --profile targets/mumu-android15-x86_64/profile.json --base inputs/base.vdi --source inputs/source.vdi --output build/extracted
```

输出目录必须不存在。失败输出保留供检查；重试使用新目录，不覆盖历史证据。

## 2. 构建内核模块和加载器

Linux／WSL 下，路径使用对应系统格式：

```sh
python3 scripts/build-native.py \
  --resukisu-source inputs/resukisu \
  --kernel-archive inputs/ack.tar.gz \
  --kernel-config inputs/prepared.config \
  --clang inputs/clang \
  --target-symbols inputs/core-kallsyms.txt \
  --output build/native

python3 scripts/prepare-kernel.py \
  --kernel inputs/kernel --elf inputs/kernel.elf --output build/kernel
```

固定提交、编译器和配置摘要见 [原生目标描述](../native/driver/target-profile.json)。`--kernel-config` 可提供原机解压配置或原配置gzip；构建器在新输出中应用对应配置增量，并检查最终配置摘要。构建从 Git 固定提交导出，不带入上游工作树里的未提交修改。内核准备只对已核验的原内核处理，保留压缩、CRC及改动范围检查。

## 3. 构建用户态辅助工具

在 `bindings/userspace.json` 写入三项配置：`schemaVersion` 为1，`expectedManagerUid` 为指定实例中实际核对的管理器UID，`daemonFile` 为配套daemon的本地路径。UID不可照抄其他实例。daemon目前只接受固定已核验版本。

```sh
python3 scripts/build-userspace.py --config bindings/userspace.json --output build/userspace
```

构建会运行主机模型，失败时保留输出并以非零状态退出。Windows 调用可明确传入 WSL 发行版和用户；用 `--help` 查看本机模型 Python 的选择方式。构建清单中 `deviceValidated:false` 是正常且必要的边界。

配套版本登记见 [原生工具说明](../native/README.md)，它只生成待审核记录，不自动扩大兼容清单。可选的 [Frida用例](../native/tools/frida/README.md) 单独构建；当前项目尚未迁入自动连接实例和附加进程的编排，不把生成两个工具当作本轮设备验收。

## 4. 组装三个分区

```sh
python3 scripts/prepare-boot.py --source build/extracted/boot.img \
  --kernel build/kernel/kernel --loader build/native/driver-init \
  --module build/native/kernelsu.ko \
  --native-manifest build/native/manifest.json \
  --userspace-manifest build/userspace/manifest.json \
  --prestage build/userspace/resukisu-migrate-prestage --output build/boot

python3 scripts/prepare-system.py --source build/extracted/system.img \
  --daemon inputs/ksud --output build/system

python3 scripts/prepare-vendor.py --source build/extracted/vendor.img --output build/vendor
```

辅助工具的实际产物路径以 `build/userspace/manifest.json` 为准。启动分区保持原文件数据槽大小；system编辑只允许目标文件及分配元数据变化；vendor只替换一条固定覆盖动作。原AVB尾部保留，但原校验树与修改内容不再匹配，不能将此候选称为通用已验签镜像。

```powershell
mumu-resukisu assemble --profile targets/mumu-android15-x86_64/profile.json --base inputs/base.vdi --source inputs/source.vdi --partition boot=build/boot/boot.img --partition system=build/system/system-candidate.img --partition vendor=build/vendor/vendor.img --output build/candidate
```

命令输出 `candidate.json` 和 `audit.json`，对整个虚拟磁盘重新比较；分区外的有效字节、原有块映射和不允许改动的磁盘身份必须保持。新建差分盘的修改标识会变化，不能要求不同构建的整个VDI文件摘要相同。

## 5. 绑定一个明确的实例

Windows 下，由使用者明确选择用于测试的停止实例。以下路径必须换成实际路径；绑定会重新读取管理器信息、关闭状态和磁盘身份，而不是信任手填哈希。

```powershell
mumu-resukisu bind --profile targets/mumu-android15-x86_64/profile.json --manager '<安装目录>/nx_main/MuMuManager.exe' --index '<实例编号>' --instance-directory '<该实例目录>' --private-disk '<该实例目录>/system-diff.vdi' --base '<共享基础盘>' --output bindings/test.json
```

绑定文件包含本机身份，应留在忽略目录。保持绑定文件位置不变：事务目录位于其旁边的 `.state/` 下，由绑定身份派生。不要复制绑定后另开一套状态来操作同一个实例。

## 6. 安装、检查和恢复

```powershell
mumu-resukisu inspect --profile targets/mumu-android15-x86_64/profile.json --binding bindings/test.json --candidate build/candidate/candidate.json
mumu-resukisu install --profile targets/mumu-android15-x86_64/profile.json --binding bindings/test.json --candidate build/candidate/candidate.json
mumu-resukisu status --profile targets/mumu-android15-x86_64/profile.json --binding bindings/test.json
mumu-resukisu restore --profile targets/mumu-android15-x86_64/profile.json --binding bindings/test.json --transaction '<status返回的事务号>'
```

安装不会启动实例或浏览器；后续启动和运行验收须单独进行。恢复可以停止已绑定实例，核对原备份后恢复系统盘；即使候选文件丢失，也不应阻止依赖原备份的恢复。

活动事务、备份与记录必须一起保留。写前检查被拒绝时保留外部改动；写到一半失败会尝试在同一独占句柄中恢复。未知的中断磁盘状态不会被猜测性覆盖，需保留证据另行处理。不要删状态文件或改摘要来绕过保护。
