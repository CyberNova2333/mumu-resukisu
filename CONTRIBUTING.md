# 贡献指南

先读[一次可回退安装的主流程](repo-docs/walkthroughs/one-real-run.md)，再用[代码地图](repo-docs/code-map.md)找到修改入口。当前仅覆盖目标描述中指纹完全匹配的 Android 15 x86_64 引擎；新增目标必须提供独立描述与证据，不能通过放宽原指纹获得“兼容”。

## 修改与验证

在 Python 3.11+ 环境执行 `python -m pip install -e .`，然后运行 `python -m unittest discover -s tests -v`。Windows 文件句柄和链接边界需要在 Windows 验证；原生编译与模型验证按[构建与操作](docs-site/docs/index.md)单独运行。记录实际退出状态及未覆盖区域，不把主机模型称作模拟器验收。

行为、参数或责任边界改变时，前台同步 `repo-docs/` 的相应页面，并在变更记录中写下实际提交锚点与验证。只修改文档样式或导航时运行独立文档站构建，禁止把其成功当作原生构建或设备启动成功。

## 提交与许可

使用正式分支，提交标题采用 `type(scope): 中文描述`。作者为实际人类贡献者，使用其 SSH 签名；使用 AI 助手时附实际执行助手的通用名作为 `Co-authored-by`，不写型号、版本或会话标记。不使用助手专用签名密钥。

提交前核对暂存区，只包含本次修改。镜像、APK、构建输出、日志、实例身份、本机路径和凭据留在忽略目录。各组件许可见[来源清单](THIRD_PARTY.md)；独立工具采用 GPLv3，继承的内核补丁必须保留上游许可与版权。

报告疑似安全问题请先阅读[报告边界](SECURITY.md)。

## 贡献许可与附加条款约定 / Contribution Licensing & Additional Terms

依据 `LICENSE-ADDITIONAL_TERMS-zh-CN.md` 第 1.7 款，向本项目提交拉取请求（PR）、补丁或其他贡献时，即表示你确认对所提交内容享有合法授权权利，并同意你对第一方 AGPLv3 适用材料作出的贡献除适用 [GNU AGPLv3](LICENSE) 外，亦一并纳入 [`LICENSE-ADDITIONAL_TERMS-zh-CN.md`](LICENSE-ADDITIONAL_TERMS-zh-CN.md)（包括第 9.5–9.6 款有条件的扩展附加许可；英文参考译文见 [`LICENSE-ADDITIONAL_TERMS-en.md`](LICENSE-ADDITIONAL_TERMS-en.md)）适用范围；附带独立图标、美术或字体资产的，须按 [`ASSET_LICENSES.md`](ASSET_LICENSES.md) 如实提供来源与有效授权信息。既有上游或第三方多许可组件（如内核 GPLv2、上游 MIT / Apache-2.0 / GPL-2.0-or-later 代码）仍按仓库说明保留其各自原有许可。

By submitting a pull request, patch, or other contribution to this repository, you confirm that you hold the requisite rights to license your submission and agree pursuant to Section 1.7 of [`LICENSE-ADDITIONAL_TERMS-zh-CN.md`](LICENSE-ADDITIONAL_TERMS-zh-CN.md) (English reference: [`LICENSE-ADDITIONAL_TERMS-en.md`](LICENSE-ADDITIONAL_TERMS-en.md)) that your contributions to first-party Covered Material are licensed under GNU AGPLv3 (`LICENSE`) together with the Additional Terms (including the conditional additional permission in Sections 9.5–9.6, where applicable), and that any contributed standalone assets are documented under [`ASSET_LICENSES.md`](ASSET_LICENSES.md). Pre-existing upstream or third-party components retain their respective original licenses as documented in the repository.
