---
version: 2.0.0
name: MuMu ReSukiSU Primer Tactile Design System
description: 面向 MuMu 模拟器 Android 15 引擎 Root 适配、VHD 镜像修改与可回退安装工具（mumu-resukisu）的 Primer 质感设计系统规范。将 GitHub Primer 功能原语与重工业磁盘扇区急救座舱（Heavy Industrial VHD Repair Cockpit）美学深度融合，定义分区状态指示、事务回滚锁徽章、十六进制扇区比对与安装向导视觉契约。
colors:
  primary: '#0969da'
  accent-fg: '#0969da'
  accent-emphasis: '#0969da'
  accent-subtle: '#ddf4ff'
  accent-dark-fg: '#2f81f7'
  canvas-default: '#ffffff'
  canvas-overlay: '#ffffff'
  canvas-inset: '#f6f8fa'
  canvas-subtle: '#f6f8fa'
  canvas-dark-default: '#0d1117'
  canvas-dark-overlay: '#161b22'
  canvas-dark-inset: '#010409'
  canvas-dark-subtle: '#161b22'
  fg-default: '#1f2328'
  fg-muted: '#57606a'
  fg-on-emphasis: '#ffffff'
  fg-dark-default: '#f0f6fc'
  fg-dark-muted: '#8b949e'
  border-default: '#d0d7de'
  border-muted: '#d8dee4'
  border-dark-default: '#30363d'
  border-dark-muted: '#21262d'
  success-fg: '#1a7f37'
  success-emphasis: '#1f883d'
  success-subtle: '#dafbe1'
  success-dark-fg: '#3fb950'
  danger-fg: '#cf222e'
  danger-emphasis: '#cf222e'
  danger-subtle: '#ffebe9'
  danger-dark-fg: '#f85149'
  attention-fg: '#9a6700'
  attention-subtle: '#fff8c5'
  attention-dark-fg: '#d29922'
typography:
  body-md:
    fontFamily: "-apple-system, BlinkMacSystemFont, 'Segoe UI', 'Noto Sans', Helvetica, Arial, sans-serif"
    fontSize: 14px
    fontWeight: 400
    lineHeight: 1.5
    letterSpacing: 0px
  body-sm:
    fontFamily: "-apple-system, BlinkMacSystemFont, 'Segoe UI', 'Noto Sans', Helvetica, Arial, sans-serif"
    fontSize: 12px
    fontWeight: 400
    lineHeight: 1.5
    letterSpacing: 0px
  heading-section:
    fontFamily: "-apple-system, BlinkMacSystemFont, 'Segoe UI', 'Noto Sans', Helvetica, Arial, sans-serif"
    fontSize: 16px
    fontWeight: 600
    lineHeight: 1.5
    letterSpacing: 0px
  code-inline:
    fontFamily: "ui-monospace, SFMono-Regular, 'SF Mono', Menlo, Consolas, 'Liberation Mono', monospace"
    fontSize: 12px
    fontWeight: 500
    lineHeight: 1.5
    letterSpacing: 0px
rounded:
  none: 0px
  sm: 3px
  md: 6px
  lg: 8px
  pill: 9999px
spacing:
  none: 0px
  xxs: 4px
  xs: 8px
  sm: 12px
  md: 16px
  lg: 24px
  xl: 32px
components:
  partition-box:
    backgroundColor: '{colors.canvas-default}'
    textColor: '{colors.fg-default}'
    typography: '{typography.body-md}'
    rounded: '{rounded.md}'
    padding: '{spacing.md}'
  rollback-pill:
    backgroundColor: '{colors.success-subtle}'
    textColor: '{colors.success-fg}'
    typography: '{typography.code-inline}'
    rounded: '{rounded.pill}'
    padding: '{spacing.xxs}'
---

# MuMu ReSukiSU 质感设计系统规范 (`DESIGN.md`)

本文档是 [`mumu-resukisu`](file:///D:/Projects/CyberNova2333/git-repos/mumu-resukisu) 的设计系统规范。针对特定 MuMu 模拟器 Android 15 x86_64 引擎的 Root 适配、VHD 分区修改与事务性回退安装流程，基于 GitHub Primer 功能原语融入**重工业磁盘扇区急救座舱物理质感（Heavy Industrial VHD Repair Cockpit Materiality）**，定义镜像验证、分区比对、回退状态印章与 CLI 向导视觉契约。

---

## 1. 核心设计哲学与物理质感

1. **磁盘扇区网格与金属拉丝外壳（Sector Grid & Brushed Titanium Materiality）**：
   - 展现分区修改（system/vendor）的精密物理感：卡片采用暗夜钛金属底板（`#0d1117 / #161b22`）、`1px solid #30363d` 精密加工槽线，以及顶边缘金属反光（`inset 0 1px 0 rgba(255, 255, 255, 0.1)`）。
2. **写保护与事务性回退印章（Transactional Rollback Seals）**：
   - 安装前的原始盘备份（Original Snapshot）与回退点（Rollback Point）带有立体微拟物安全锁印章：`rounded.pill` 结合绿色或金色防伪描边，明确标识事务边界，保证系统崩坏时随时一键还原。
3. **物理机械开关按压手感（Industrial Switch Kinematics）**：
   - 交互指令与操作按钮具备重工业微动按键触感（`:hover` `translateY(-0.5px)`，`:active` `translateY(0.5px) scale(0.99)` + `inset 0 2px 4px rgba(0, 0, 0, 0.35)`）。

---

## 2. 调色板与语义 Token

### 2.1 W3C `$value/$type` Token 注册表

| Primer 功能 Token | Light `$value` | Dark `$value` | 物理磁盘急救映射 |
| :--- | :--- | :--- | :--- |
| `canvas.default` | `#ffffff` | `#0d1117` | 镜像修补面板底色、报告正文 |
| `canvas.inset` | `#f6f8fa` | `#010409` | 原始盘扇区内凹槽、十六进制 Hex 视图底色 |
| `canvas.subtle` | `#f6f8fa` | `#161b22` | 分区列表头、驱动补丁列表项 |
| `fg.default` | `#1f2328` | `#f0f6fc` | 镜像指纹、修改进度主数值 |
| `fg.muted` | `#57606a` | `#8b949e` | 偏移量（Offset）、扇区大小、备份哈希 |
| `border.default` | `#d0d7de` | `#30363d` | 容器物理边框（1px solid） |
| `success.fg` | `#1a7f37` | `#3fb950` | 备份已落盘就绪、回退事务已就位、Root 激活成功 |
| `attention.fg` | `#9a6700` | `#d29922` | 实例正在运行需先安全关闭、非受控分区跳过提醒 |
| `danger.fg` | `#cf222e` | `#f85149` | 目标引擎指纹不匹配拒写、磁盘空间不足阻断 |

---

## 3. 字体栈与空间规范

- **工程字体栈（`--font-sans`）**：`-apple-system, BlinkMacSystemFont, 'Segoe UI', 'Noto Sans', Helvetica, Arial, sans-serif`
- **等宽数据字体（`--font-mono`）**：`ui-monospace, SFMono-Regular, 'SF Mono', Menlo, Consolas, 'Liberation Mono', monospace`
- **排版要求**：正文 `14px`，磁盘分区大小、十六进制偏移与 SHA256 镜像指纹严格使用 `12px` 等宽且强制对齐 `tabular-nums`。

---

## 4. 安全保护与回退契约

1. **版本指纹强制死锁**：严格校验目标引擎 profile 的指纹哈希，指纹不完全匹配时坚决拒写，并在界面展示红底警报。
2. **写盘前物理备份**：在对任何虚拟磁盘执行写入前，必须生成不可变的回退镜像文件，回退印章未激活前禁用直接覆盖操作。
3. **单实例绑定隔离**：写盘与测试流程严格限定于指定单个实例，不污染其他并行模拟器环境。
