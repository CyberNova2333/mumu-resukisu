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

依据 `LICENSE-ADDITIONAL_TERMS-zh-CN.md`（v1.6.7-r4）第 1.7—1.9 款，向本项目提交拉取请求（PR）、补丁或其他贡献时，你须确认对所提交内容享有合法授权权利并同意按对应基础许可证（第一方原创代码为 [GNU AGPLv3](LICENSE)）提供。如你希望将个人享有版权的贡献纳入 [`LICENSE-ADDITIONAL_TERMS-zh-CN.md`](LICENSE-ADDITIONAL_TERMS-zh-CN.md)（英文参考：[`LICENSE-ADDITIONAL_TERMS-en.md`](LICENSE-ADDITIONAL_TERMS-en.md)，实施指南：[`IMPLEMENTATION-GUIDE.zh-en.md`](IMPLEMENTATION-GUIDE.zh-en.md)）适用范围，请在 PR 说明或可核实记录中**分别明确声明**对附加要求以及对第 9.5–9.6 款有条件附加许可的授权范围；未作明确声明的贡献将如实记为 `[附加条款：未发表意见]` 并按原适用基础许可证处理。引入第三方组件或附带独立图标、美术、字体资产的，须按 [`ASSETS_LICENSES.md`](ASSETS_LICENSES.md) 如实登记来源、原始作者与有效许可原文。既有上游或第三方多许可组件（如内核 GPLv2、上游 MIT / Apache-2.0 / GPL-2.0-or-later 代码）保留其各自原有许可。

Pursuant to Sections 1.7–1.9 of [`LICENSE-ADDITIONAL_TERMS-zh-CN.md`](LICENSE-ADDITIONAL_TERMS-zh-CN.md) (v1.6.7-r4; English reference: [`LICENSE-ADDITIONAL_TERMS-en.md`](LICENSE-ADDITIONAL_TERMS-en.md); implementation guide: [`IMPLEMENTATION-GUIDE.zh-en.md`](IMPLEMENTATION-GUIDE.zh-en.md)), submitting a pull request, patch, or other contribution confirms that you hold the requisite rights to license your submission under the applicable base license ([GNU AGPLv3](LICENSE) for first-party code). Express authorization for the Additional Requirements and for the conditional additional permission in Sections 9.5–9.6 is verified separately: please state your covered materials and consent scope explicitly in the PR or a verifiable record; submissions without an express verifiable statement are recorded as `[Additional terms: No recorded position]` and remain governed by their base license. Any third-party components or standalone artwork, fonts, or icons must be truthfully documented in [`ASSETS_LICENSES.md`](ASSETS_LICENSES.md). Pre-existing upstream or third-party components retain their respective original licenses.
