# MuMu ReSukiSU 人员署名、AI 工具、代码与资产许可综合清单

**标准文件名：** `ASSETS_LICENSES.md`  
**配套模板版本：** v1.6.7-r4  
**项目名称：** `MuMu ReSukiSU`  
**开发、维护或联合推出主体：** `CyberNova`  
**上游仓库地址：** `https://github.com/CyberNova2333/mumu-resukisu`  
**所对应的仓库版本及发行物：** `main` 分支（基于提交 `a079bfa` 及本次 v1.6.7-r4 合规更新工作树）  
**记录覆盖范围及尚未核实部分：** 已核实本仓库全部受 Git 跟踪源码，包括 Python CLI/脚本（`src/`、`scripts/`）、独立用户态 C 辅助工具（`native/{loader,prestage,probes,common,tools/frida}/`，`AGPL-3.0-only`）、`native/driver/` 内核补丁（`GPL-2.0-only`）、`native/images/system-edit.c` 独立镜像编辑器（`GPL-2.0-only`，单独构建不与 `AGPL-3.0-only` 链接）、`THIRD_PARTY.md`、`licenses/{GPL-2.0,GPL-3.0}.txt` 与 `docs-site/` 自托管字体及脚本。未核实范围：用户自行提供的 MuMu 虚拟磁盘/内核原镜像、上游 ReSukiSU/ACK 完整源码树、musl/Clang/e2fsprogs/Unicorn/Frida 等外部构建与测试工具（均不在本仓库分发）。  
**清单最后更新日期：** `2026-11-19`  
**配套附加条款：** [中文完整文本](./LICENSE-ADDITIONAL_TERMS-zh-CN.md)；[英文参考译文](./LICENSE-ADDITIONAL_TERMS-en.md)；[工程实施指南](./IMPLEMENTATION-GUIDE.zh-en.md)

> **填写状态：本清单已依据本仓库实际文件、Git 提交历史与各上游/字体原始许可文本核实填写。** 四个分区的排列与配套附加条款 v1.6.7-r4 第 3.2、7.1 款一致；本清单如实记录已核实范围与未覆盖范围，不扩大或缩减任何上游有效许可证授予的权利，也不以列名方式向第三方材料追加限制。

**法律效力与载体说明**

1. 本清单记录事实及授权依据，不由登记、标签、Git 字段或 `meta.json` 自行产生版权、转让、代表权限或同意；真实声明的效力与覆盖范围依附加条款第 1.2、1.7—1.9 款判断。
2. 下游可依第 3.7 款改用其他合规载体，无须沿用本文件格式或工作流。与实际采用材料有关、依法或依有效许可应保留的版权、许可、免责、`NOTICE` 和合理署名，仍须按该依据保留；原许可要求完整文本或特定位置、方式的，链接和摘要不能代替。纯工具记录、一般致谢和审计字段的边界见第 3.5—3.6 款。
3. 仓库内既有的 `AUTHORS`、`NOTICE`、`COPYING`、`LICENSE`、`LICENSE.TXT`、`THIRD_PARTY.md`、`THIRD_PARTY_NOTICES.md`、`UPSTREAM.md`、`licenses/` 及 `LICENSES/` 等文件继续原样保留并在此索引；不因采用本清单而删减应保留原文。

工程实践参考见 `IMPLEMENTATION-GUIDE.zh-en.md`（非规范性指南，不增设许可条件）。

**状态标签说明**

| 中文标签 | 英文等效标签 | 记录对象及效力边界 |
| --- | --- | --- |
| `[附加条款：已明确同意]` | `[Additional terms: Express agreement recorded]` | 记录可核实的实际权利人或其有效授权代表就特定材料、条款及版本作出的明确同意。须分别核实附加要求和第 9.5—9.6 款附加许可的覆盖范围；同意其中一项不自动证明同意另一项。 |
| `[附加条款：未发表意见]` | `[Additional terms: No recorded position]` | 本项目现有记录尚无该主体对相关材料适用相应条款的可核实明确意见。不表示拒绝、曾被单独征询、放弃权利或在其他场合从未表达意见；原有效许可、既有附加条款或例外仍依实际内容适用。 |
| `[附加条款：不适用（遵循原上游许可证）]` | `[Additional terms: Not applicable (original upstream license governs)]` | 用于第三分区的原上游及第三方材料，说明列名不使其纳入本项目新增附加要求，也不代表其权利人授予本项目第 9.5 款豁免。适用的原许可及组合义务仍须履行。 |

## 一、本项目人类作者与贡献者 (Project Human Authors & Contributors)

依据当前可核实的 Git 历史、仓库权利记录与 `meta.json`，分别记录本项目维护者、原作者与上游历史贡献者的真实角色。`Author`、`Committer`、`Co-authored-by` 与版权持有人并不当然相同。

### 1.1 主体、角色与贡献记录

| 记录编号 | 姓名、笔名或组织名称及真实角色 | Git 或平台账户关联 | 实际贡献及材料范围 | 提交、PR 或其他事实依据 | 附加条款意见状态及授权记录编号 |
| --- | --- | --- | --- | --- | --- |
| `P-01` | CyberNova（项目维护者、原作者、版权持有人、许可人；Git 角色：Committer, Author） | `https://github.com/CyberNova2333` / `295488275+CyberNova2333@users.noreply.github.com` | 本仓库由 CyberNova 原创编写的 Python 工具（`src/`、`scripts/`）、用户态 C 辅助程序（`native/{loader,prestage,probes,common,tools/frida}/`）及文档（`AGPL-3.0-only`）；**明确除外**：CyberNova 编写的 `native/images/system-edit.c`（调用 `libext2fs`）与 `native/driver/` 内核增量适配补丁单独采用 **`GPL-2.0-only`**（见 `THIRD_PARTY.md` 与 `licenses/GPL-2.0.txt`），与 `AGPL-3.0-only` 程序独立编译运行，不适用 `AGPL-3.0-only` 或附加条款 | 本仓库 Git 完整提交历史（至 `a079bfa`）及根目录 `meta.json` | `[附加条款：已明确同意]`（见 `AUTH-01`） |

### 1.2 授权事实与范围索引

下表严格分别记录“附加要求的覆盖范围”与“第 9.5—9.6 款附加许可的覆盖范围”；同意其中一项不自动推定同意另一项，且任何声明均仅及于声明人享有合法权限的材料范围。

| 授权记录编号及关联主体 | 实际声明主体与代表权限依据 | 材料、提交及版本范围 | 附加要求的覆盖范围 | 第 9.5—9.6 款附加许可的覆盖范围 | 声明、有效许可或证明依据 |
| --- | --- | --- | --- | --- | --- |
| `AUTH-01`；`P-01` | CyberNova（本人，对其原创受版权保护材料享有完整版权与许可权限） | 本仓库由 CyberNova 原创编写的 Python 工具（`src/`、`scripts/`）、用户态 C 辅助程序（`native/{loader,prestage,probes,common,tools/frida}/`）及文档（`AGPL-3.0-only`）；**明确除外**：CyberNova 编写的 `native/images/system-edit.c`（调用 `libext2fs`）与 `native/driver/` 内核增量适配补丁单独采用 **`GPL-2.0-only`**（见 `THIRD_PARTY.md` 与 `licenses/GPL-2.0.txt`），与 `AGPL-3.0-only` 程序独立编译运行，不适用 `AGPL-3.0-only` 或附加条款（至当前提交 `a079bfa` 及后续由本人提交并标明适用的版本） | 已明确同意适用 `LICENSE-ADDITIONAL_TERMS-zh-CN.md` **v1.6.7-r4** 全部附加要求（第 7(b)/(c)/(d) 条） | 已明确授权适用 `LICENSE-ADDITIONAL_TERMS-zh-CN.md` **v1.6.7-r4** 第 9.5—9.6 款有条件的附加许可豁免（仅限于本人享有权利的原创部分，不代第三人授予） | 仓库根目录 `meta.json`（`additionalTermsConsent.CyberNova = true`）及仓库公开发布记录 |

### 1.3 应保留的项目材料声明

| 对应材料或主体编号 | 应保留的具体版权、许可或合理署名内容及依据 | 实际承载文件或界面 | 适用范围与允许的调整方式 |
| --- | --- | --- | --- |
| `P-01` | GNU AGPLv3 完整原文、附加条款 v1.6.7-r4、合理署名与项目来源声明（依据 AGPLv3 第 4–7 条及附加条款第三条） | `LICENSE`、`LICENSE-ADDITIONAL_TERMS-zh-CN.md`、`LICENSE-ADDITIONAL_TERMS-en.md`、`ASSETS_LICENSES.md` 及各原创源文件 SPDX 头 | 适用于本项目第一方 AGPLv3 适用材料；下游可依附加条款第 3.7 款改用等效合规载体 |
| `P-01` / `C-01` / `C-02` | 多许可源码集合说明与 GPLv2 / GPLv3 完整原文（含 `pyproject.toml` `AGPL-3.0-only AND GPL-2.0-only` 声明） | `THIRD_PARTY.md`、`licenses/GPL-2.0.txt`、`licenses/GPL-3.0.txt`、`native/images/system-edit.c` | 内核补丁与 `system-edit.c` 均单独构建，绝不与 `AGPL-3.0-only` 目标代码静态或动态链接 |

## 二、AI Agent 与自动化辅助工具 (AI Agents & Automated Assistants)

本分区记录工程中实际使用的 AI 辅助工具和相关技术账户。依据附加条款第 1.4、1.8 款，本分区不将工具、模型或机器人账户视为独立的版权许可授权主体，也不对工具本身赋予附加条款同意状态。

| 工具记录编号及工具类别或名称 | 技术账户、机器人或环境标识 | 经核实的使用、控制或提交上下文 | 辅助范围及相关提交或材料 | 实际主体或组件记录关联 | 核实情况与必要说明 |
| --- | --- | --- | --- | --- | --- |
| `T-01`；Codex（AI 编程辅助工具） | `noreply@openai.com` | 在人类维护者（`P-01` CyberNova）指令、审阅与提交操作下参与工程辅助（Git 字段角色：Co-authored-by） | 代码草拟、重构、文档整理、测试编写与代码审查辅助（以 Git 历史 `Co-authored-by` / 工具记录为准） | `P-01`（工具本身非版权许可授权主体） | 已核实 Git 历史中存在该工具签名记录；工具及技术账户本身不标注附加条款同意状态，权属与许可依附加条款第 1.4、1.8 款按实际人类权利人及有效许可判断 |
| `T-02`；gemini-code-assist（AI 编程辅助工具） | `200291788+gemini-code-assist@users.noreply.github.com`（https://github.com/apps/gemini-code-assist） | 在人类维护者（`P-01` CyberNova）指令、审阅与提交操作下参与工程辅助（Git 字段角色：Co-authored-by） | 代码草拟、重构、文档整理、测试编写与代码审查辅助（以 Git 历史 `Co-authored-by` / 工具记录为准） | `P-01`（工具本身非版权许可授权主体） | 已核实 Git 历史中存在该工具签名记录；工具及技术账户本身不标注附加条款同意状态，权属与许可依附加条款第 1.4、1.8 款按实际人类权利人及有效许可判断 |

## 三、上游基线、依赖与第三方代码组件（含原始作者与贡献者） (Upstream Baselines, Dependencies & Third-Party Code Components)

本分区登记本仓库实际引入、分叉、快照化、内嵌或作为关键依赖使用的上游与第三方代码组件。组件未出现在 Git 提交历史中，不减少其权利人已有权利或应保留声明。

### 3.1 代码组件与原始作者主清单

| 组件编号及名称 | 类型与实际引入方式 | 仓库位置、发行物位置或调用关系 | 上游来源及所采用版本、提交或归档标识 | 原始作者、版权持有人及相关贡献署名出处 | 实际核实范围与备注 |
| --- | --- | --- | --- | --- | --- |
| `C-01`；ReSukiSU 内核适配补丁基线 | 上游固定提交补丁上下文（不入库完整源码或二进制模块） | `native/driver/resukisu.patch`、`native/driver/build-metadata.patch` | https://github.com/ReSukiSU/ReSukiSU（提交 `5785bdb8091304322f597f67d50e0b79f9815cb2`） | ReSukiSU Contributors | 已核实其 `kernel/LICENSE` 为 GPL-2.0-only、项目根 `LICENSE` 为 GPL-3.0；本仓仅存可审查补丁增量 |
| `C-02`；Android Common Kernel (ACK) 头文件增量补丁 | 内核头文件必要增量补丁（不入库完整 ACK 源码） | `native/driver/ack-task-layout.patch`、`native/driver/prepare-config.patch` | https://android.googlesource.com/kernel/common（提交 `c034535679d056565875fec24925fd21309040c5`） | The Linux Kernel Organization & Google AOSP Contributors | 已核实保留原头文件 SPDX 与 GPL-2.0-only 许可，未改标为 GPLv3/AGPLv3 |
| `C-03`；e2fsprogs / libext2fs（外部构建依赖） | 宿主系统安装库（不捆绑源码或库文件） | 由 `native/images/system-edit.c`（`GPL-2.0-only`）单独链接构建 | https://github.com/tytso/e2fsprogs | Theodore Ts'o & e2fsprogs Contributors | 已核实 `system-edit.c` 为规避 `libext2fs` 混用 LGPLv2/GPLv2 的潜在冲突而主动采用 `GPL-2.0-only` 并单独编译 |
| `C-04`；Anime.js 4.5.0 | 第三方内嵌脚本（静态复制） | `docs-site/.vitepress/theme/vendor/animejs/` | https://github.com/juliangarnier/anime（v4.5.0） | Julian Garnier（`Copyright (c) 2025 Julian Garnier`） | 已核实仓内随附 MIT 许可原文与静态脚本文件；未修改上游逻辑 |

### 3.2 原始许可、本地修改与组合边界

| 组件编号 | 原始部分的有效许可证及完整文本位置 | 本地新增或修改部分及其有效授权 | 构建、链接或交互方式与适用边界 | 原始材料的附加条款状态 |
| --- | --- | --- | --- | --- |
| `C-01` | GPL-2.0-only（内核侧；全文见 `licenses/GPL-2.0.txt`）与 GPL-3.0-only（配套管理器参考；见 `licenses/GPL-3.0.txt`） | 本项目内核适配增量按 `GPL-2.0-only` 保留原上下文与版权，不改标 `AGPL-3.0-only` | 独立内核模块构建链，与 `AGPL-3.0-only` 用户态工具严格进程与二进制分离（依第 7.4–7.5 款） | [附加条款：不适用（遵循原上游许可证）] |
| `C-02` | GPL-2.0-only（`licenses/GPL-2.0.txt`） | 仅存最小内核结构布局与配置增量补丁（`GPL-2.0-only`） | 独立内核模块构建使用，不与 `AGPL-3.0-only` 代码链接 | [附加条款：不适用（遵循原上游许可证）] |
| `C-03` | GPL-2.0-only / LGPL-2.0-or-later（以所用发行版与上游 `NOTICE` 为准） | 无上游源码修改；调用方 `system-edit.c` 遵循 `GPL-2.0-only` | 独立本地主机工具二进制，与 `AGPL-3.0-only` 程序分离构建与调用 | [附加条款：不适用（遵循原上游许可证）] |
| `C-04` | MIT（`docs-site/.vitepress/theme/vendor/animejs/LICENSE.md`） | 无本地源码修改（原样静态内嵌） | 作为独立前端／文档站动效库静态分发，与 AGPL-3.0-only 兼容 | [附加条款：不适用（遵循原上游许可证）] |

### 3.3 需保留的许可、NOTICE 与版权声明索引

| 组件编号 | 声明类别及原始出处 | 实际随附或提供位置 | 原许可要求保留的内容和适用范围 | 载体安排说明 |
| --- | --- | --- | --- | --- |
| `C-01` | GPLv2 / GPLv3 许可全文与 `THIRD_PARTY.md` 边界表 | `THIRD_PARTY.md`、`licenses/GPL-2.0.txt`、`licenses/GPL-3.0.txt` | 保留原补丁上下文版权与 GPL 原文；构建者对外分发编译内核模块须履行 GPLv2 对应源码义务 | 完整独立许可文本随仓保留 |
| `C-02` | Linux 内核 GPL-2.0-only 原文 | `licenses/GPL-2.0.txt`、`THIRD_PARTY.md` | 保留原内核 SPDX 与 GPLv2 原文 | 随仓提供完整 `GPL-2.0.txt` |
| `C-03` | 上游 `NOTICE` 与 GPLv2 边界说明 | `THIRD_PARTY.md`、`licenses/GPL-2.0.txt` | 再分发编译版 `system-edit` 二进制时须遵守 `e2fsprogs` 及 GPLv2 对应源码与声明义务 | 本仓库不分发预编译二进制 |
| `C-04` | MIT 版权与许可声明 | `docs-site/.vitepress/theme/vendor/animejs/LICENSE.md` | 随源代码及静态站点分发保留原版权声明与 MIT 许可全文 | 随仓提供完整 `LICENSE.md` / `animejs-MIT.txt` 文本文件 |

## 四、独立美术、字体与项目标识资产 (Independent Artwork, Fonts & Project Identifier Assets)

本分区登记可独立识别的美术图形、Logo、图标、字体及其他静态资产，依附加条款第 7.6—7.8 款及第八条区分许可与使用边界。

### 4.1 资产、来源与许可清单

| 资产编号、名称及类别 | 实际文件、来源与所采用版本 | 作者、版权持有人及可核实依据 | 实际有效许可证或具体授权及完整文本位置 | 是否为项目标识、随包方式及替换情况 | 适用范围、修改与尚待核实事项 |
| --- | --- | --- | --- | --- | --- |
| `A-01`；IBM Plex Sans（文档站自托管字体） | `docs-site/.vitepress/theme/fonts/ibm-plex-sans-latin-*.woff2`；来源：IBM Plex Sans | IBM Corp.（见同目录 `IBM-Plex-Sans-LICENSE.txt`） | SIL Open Font License 1.1（`docs-site/.vitepress/theme/fonts/IBM-Plex-Sans-LICENSE.txt`） | 否（非项目标识）；随 `docs-site` 构建静态分发；可由下游自由替换 | 按 SIL OFL 1.1 随文档站构建与浏览分发；未修改字形 |
| `A-02`；JetBrains Mono（文档站自托管字体） | `docs-site/.vitepress/theme/fonts/jetbrains-mono-latin-500-normal.woff2`；来源：JetBrains Mono | The JetBrains Mono Project Authors（见同目录 `LICENSE-jetbrains-mono.txt`） | SIL Open Font License 1.1（`docs-site/.vitepress/theme/fonts/LICENSE-jetbrains-mono.txt`） | 否（非项目标识）；随 `docs-site` 构建静态分发；可由下游自由替换 | 按 SIL OFL 1.1 随文档站代码块渲染使用；未修改字形 |

### 4.2 字体与其他资产的专门条件记录

以下版权声明与保留字体名称（RFN）均**逐字核对自本仓库实际随附的许可原文文件**，不从品牌名或文件名推定。

| 资产编号 | 实际保留的版权、许可及署名资料 | 保留字体名称或其他专门条件 | 本地修改及其许可要求 | 实际提供或展示方式 |
| --- | --- | --- | --- | --- |
| `A-01` | `Copyright 2019 IBM Corp. All rights reserved. IBMPlexSans-Italic[wdth,wght].ttf: Copyright 2019 IBM Corp. All rights reserved.`；全文见 `docs-site/.vitepress/theme/fonts/IBM-Plex-Sans-LICENSE.txt` | 未声明保留字体名称（无 RFN）；不得单独售卖字体文件本身，衍生字体须续用 SIL OFL 1.1 | 无本地字形修改（仅原样自托管 WOFF2） | 随源码仓库与 `docs-site` 主题字体目录同目录保留完整 OFL-1.1 原文 |
| `A-02` | `Copyright 2020 The JetBrains Mono Project Authors (https://github.com/JetBrains/JetBrainsMono) JetBrainsMono-Italic[wght].ttf: Copyright 2020 The JetBrains Mono Project Authors (https://github.com/JetBrains/JetBrainsMono)`；全文见 `docs-site/.vitepress/theme/fonts/LICENSE-jetbrains-mono.txt` | 未声明保留字体名称（无 RFN）；不得单独售卖字体文件本身，衍生字体须续用 SIL OFL 1.1 | 无本地字形修改（仅原样自托管 WOFF2） | 随源码仓库与 `docs-site` 主题字体目录同目录保留完整 OFL-1.1 原文 |

### 4.3 项目标识与既有许可边界

本仓库不含独立图形 Logo；项目名称 `MuMu ReSukiSU` 中提及 `MuMu` 与 `ReSukiSU` 仅用于真实说明适配目标，本项目与网易 MuMu 及 ReSukiSU 上游均无官方隶属或背书关系。

引用附加条款第八条（含第 8.2 款）时，完整保留资产独立性、实际权利人授权、可替换性、许可相容性及不追溯收回既有版本有效授权（第 7.8、9.3 款）等全部前提条件，不单独摘录或扩大限制。

---

本清单记录事实与许可索引，不增设原有效许可证未规定的条件，也不要求下游沿用相同文件名、分区或自动化工作流；下游载体调整依附加条款第 3.7 款处理。
