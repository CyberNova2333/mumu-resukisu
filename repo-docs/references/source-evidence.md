# 源码证据

## 遍历记录

| 轮次 | 检查范围 | 结果 |
| --- | --- | --- |
| Pass 1 | 原实验补丁、启动工具、镜像编辑器和回退控制器 | 找到可复用实现及固定实例／路径耦合。 |
| Pass 2 | 本仓库CLI、target描述、构建脚本、主机测试 | 核对参数化边界、拒绝路径、产物清单及独立验证。 |

覆盖本地源码行为；排除原厂镜像内容、其它应用数据和未做的设备验收。可证伪检查：向允许分区外写一个字节，`images.audit`必须拒绝；若未拒绝则“范围内修改”结论不成立。

读者后续问题：为什么历史成功还要重测？因为重构改变了配置和执行入口，原报告不能证明新入口在实际设备上的完整行为。

| Claim | Evidence | Confidence | Caveat | Used by |
| --- | --- | --- | --- | --- |
| 只接受固定引擎分区 | [目标描述](../../targets/mumu-android15-x86_64/profile.json)、[镜像实现](../../src/mumu_resukisu/images.py) | Confirmed | 不是版本号相同即兼容 | walkthrough |
| 分区外字节必须不变 | [镜像测试](../../tests/test_images.py) | Confirmed | 离线快照测试 | walkthrough |
| 回退依赖原备份和事务 | [控制器](../../src/mumu_resukisu/controller.py) | Confirmed | 不含数据盘；未知中断状态拒绝猜测覆盖 | walkthrough |
| 原生源码按固定输入构建 | [构建器](../../scripts/build-native.py) | Confirmed | 外部源码与工具链由用户提供 | code-map |
| 新入口缺少设备验收 | [目标状态](../../targets/mumu-android15-x86_64/profile.json) | Confirmed | 历史固定用例成功不代替此次重测 | README |

以上源码链接以仓库根目录为基准的相对位置，在文档验证阶段核对；本文件不存储运行时路径或实例身份。
