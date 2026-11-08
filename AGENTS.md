# 工作约定

本项目提取已有模拟器适配成果；首次目标仅为已核验的 Android 15 x86_64 引擎。Android 12 与其他版本必须有独立目标描述和验收，禁止通过修改期望哈希冒充兼容。

- 保留原实验目录及证据，只迁入可维护源码、补丁、脱敏目标描述与测试。
- 不入库镜像、APK、构建产物、运行日志、实例标识、本机路径、凭据或私有服务信息。配置样例使用占位符；实际配置、输入、备份和报告位于忽略目录。
- 设备写入必须经过明确实例绑定、停机、镜像身份与哈希校验、独占句柄和回退事务。仅操作用户指定的独立测试实例；不写共享基础盘，不热卸载驱动，不更改全局安全策略。
- 不使用 Computer Use，不启动浏览器，不将此次重构当作新的设备测试授权。
- 保留上游版权与许可；第三方文件逐项登记来源。不要把既往实验成功写成重构代码已实机验收。
- 提交使用正式分支、Conventional Commits、人类作者、SSH 签名及 `Co-authored-by` 的实际 Agent 通用名；禁止模型档位、版本、会话标记及 Agent 专用签名密钥。未经明确授权不创建远端或推送。
- 验证入口由 README 与实际 package 定义确定；最终核对工作树和每项测试真实退出状态。

## 验证与文档站

主机测试使用 `python -m unittest discover -s tests -v`。文档链接测试使用 `pnpm --dir docs-site test`，文档构建使用 `pnpm --dir docs-site run docs:build`；先核对当前脚本定义，再按改动选择验证。文档站采用 VitePress 默认主题，从 `repo-docs/` 引用正文，产物与原生构建分离。文档通过不能记作设备验收。

## Repo docs

`repo-docs/` is the living guide; start with [the reading path](repo-docs/README.md) and [the main walkthrough](repo-docs/walkthroughs/one-real-run.md).
This repo's `repo-docs/` guide is reader-facing Chinese documentation. When updating reader-facing guide pages, use `repo-docs-zh` when available; keep Chinese reader handles in the prose and preserve exact source identifiers for lookup.
Read current source, scripts and tests before treating the guide as implementation truth. Choose `none`, `answer-only`, `foreground patch`, or `background sync` according to the actual knowledge gap. Patch the smallest owning page before the final answer when a durable gap would mislead. Update the walkthrough for boundary changes and the code map for responsibility changes. Record meaningful guide changes and actual verification in `repo-docs/change-log.md` with `Synced through <sha>`. Historical device evidence does not validate a refactored implementation.
