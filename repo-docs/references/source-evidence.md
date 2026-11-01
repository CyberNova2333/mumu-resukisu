# 源码证据

## 遍历记录

| 轮次 | 检查范围 | 结果 |
| --- | --- | --- |
| Pass 1 | 本仓库 CLI、目标描述、镜像与绑定控制器、原生构建和登记入口 | 核对输入、候选、绑定、事务的交接，以及外部工具和固定目标边界。 |
| Pass 2 | 下载获取器、主机测试、构建清单、现有文档和站点入口 | 反查错误输入、标签变化、独占写盘、回退及历史／重构验证区别；检查文字是否夸大证据。 |

覆盖本地源码行为；排除原厂镜像内容、其它应用数据和未做的设备验收。可证伪检查：向允许分区外写一个字节，`images.audit`必须拒绝；若未拒绝则“范围内修改”结论不成立。

读者后续问题：为什么历史成功还要重测？因为重构改变了配置和执行入口，原报告不能证明新入口在实际设备上的完整行为。

| Claim | Evidence | Confidence | Caveat | Used by |
| --- | --- | --- | --- | --- |
| 只接受固定引擎分区 | [目标描述](../../targets/mumu-android15-x86_64/profile.json)、[镜像实现](../../src/mumu_resukisu/images.py) | Confirmed | 不是版本号相同即兼容 | walkthrough |
| 分区外字节必须不变 | [镜像测试](../../tests/test_images.py) | Confirmed | 离线快照测试 | walkthrough |
| 回退依赖原备份和事务 | [控制器](../../src/mumu_resukisu/controller.py) | Confirmed | 不含数据盘；未知中断状态拒绝猜测覆盖 | walkthrough |
| 原生源码按固定输入构建 | [构建器](../../scripts/build-native.py) | Confirmed | 外部源码与工具链由用户提供 | code-map |
| 精确原厂daemon先持久备份再迁移，未知内容不覆盖 | [启动迁移](../../native/prestage/stock-migration.h)、[原厂身份](../../native/prestage/stock-daemon.json)、[失败模型](../../native/prestage/stock-unit.c) | Confirmed | 主机失败注入不等于实际断电；数据盘备份不随系统盘事务回退 | walkthrough、image-boundaries、code-map |
| 重构入口已完成原厂迁移、冷启动及授权前拒绝的阶段验证 | [分阶段验证记录](../../docs/validation.md)、[目标状态](../../targets/mumu-android15-x86_64/profile.json) | Confirmed | 授权后Root、再次冷启动与Firefox附加未测试；总体验收仍未完成 | README、walkthrough |
| 最新发行查询不改变目标版本 | [获取器](../../src/mumu_resukisu/releases.py)、[下载测试](../../tests/test_releases.py)、[固定原生目标](../../native/driver/target-profile.json) | Confirmed | 下载不验证 APK 签名或升级兼容 | release-lifecycle |
| 登记只产生待审材料 | [登记器](../../scripts/register-release.py)、[登记测试](../../tests/test_release_registration.py) | Confirmed | 源码与 APK 关联仍由维护者审查 | release-lifecycle |
| 站点使用同一份指南正文 | [站点首页](../../docs-site/index.md)、[默认主题配置](../../docs-site/.vitepress/config.mjs)、[链接转换](../../docs-site/.vitepress/guide-links.mjs) | Confirmed | 构建文档不构建业务；源码定位不复制文件 | README、code-map |

以上源码链接以仓库根目录为基准的相对位置，在文档验证阶段核对；本文件不存储运行时路径或实例身份。

文档格式参考公开 [modelbridge](https://cnb.cool/CyberNova2333/modelbridge) 的已提交 README、Standard 指南和默认主题站点结构；只借鉴阅读组织与呈现方式，不把参考项目的业务边界、验证结果或未提交主题带入本项目。实际验证记录和提交锚点见[同步记录](../change-log.md)。
