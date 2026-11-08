---
layout: home
repoGuideSource: "repo-docs/README.md"

hero:
  name: MuMu ReSukiSU
  text: 固定引擎专属 Root 适配与可回退装配
  tagline: 严谨核验固定模拟器引擎证据与本地输入，安全构建候选包，支持完整实例绑定验证与秒级回退。
  actions:
    - theme: brand
      text: 开启文档阅读
      link: /walkthroughs/one-real-run
    - theme: alt
      text: 架构代码地图
      link: /code-map
    - theme: alt
      text: 镜像与恢复边界
      link: /modules/image-boundaries
    - theme: alt
      text: GitHub 仓库
      link: https://github.com/CyberNova2333/mumu-resukisu

features:
  - title: 严密引擎证据核验
    details: 在修改任何镜像前，严格核查目标模拟器底层的固定内核与镜像签名，不具备适配证据绝不执行写盘。
  - title: 离线构建与候选封装
    details: 独立工具与装配脚本离线化运行，所有输入组件哈希强校验，生成隔离的本地安装候选包。
  - title: 完整可回退保障
    details: 建立镜像分层与写入前自动快照机制，即使装配异常或系统校验失败，也能通过回退脚本一键还原。
---

## 这是什么

**MuMu ReSukiSU** 是一个解决“如何为已有安卓模拟器引擎安全装入配套 SukiSU/KernelSU Root 实现，并提供端到端校验与原子级回退”的专业系统工具。

它摒弃了盲目修改宿主系统文件的暴力方式，坚持“先有固定引擎证据和本地输入、再离线构建候选、最后才允许绑定实例受控写盘”的原则，确保玩家与开发者的开发环境在获得 Root 权限的同时拥有绝对的稳定保障。


## 架构全景

<div class="archify-diagram-card" style="margin: 1.5rem 0; border: 1px solid var(--vp-c-border); border-radius: 12px; overflow: hidden; background: var(--vp-c-bg-soft); box-shadow: 0 4px 16px rgba(0,0,0,0.06);">
  <div style="display: flex; justify-content: space-between; align-items: center; padding: 10px 18px; background: var(--vp-c-bg-elv); border-bottom: 1px solid var(--vp-c-border); font-size: 13px;">
    <span style="font-weight: 600; color: var(--vp-c-text-1); display: flex; align-items: center; gap: 8px;">
      <span style="font-size: 16px;">📐</span> 内核桥接与权限提权系统（交互式架构图表，支持平移 / 缩放 / 探查）
    </span>
    <a href="/diagrams/resukisu-architecture.html" target="_blank" rel="noreferrer" style="font-size: 12px; color: var(--vp-c-brand-1); text-decoration: none; font-weight: 500;">
      在新窗口全屏打开 ↗
    </a>
  </div>
  <iframe src="/diagrams/resukisu-architecture.html" width="100%" height="520px" frameborder="0" style="display: block; border: none;" allowfullscreen loading="lazy"></iframe>
</div>

## 怎么用 / 快速上手

### 1. 本地候选构建与检查

在 Windows 环境下准备好模拟器本地实例，运行构建验证：

```powershell
# 运行离线候选包构建脚本
python scripts/build_candidate.py --engine-path "D:\Program Files\MuMuPlayer"
```

### 2. 候选装配与回退验证

```powershell
# 绑定指定模拟器实例执行安全写入
python scripts/apply_root.py --instance-id 0

# 若需卸载或回退还原：
python scripts/rollback.py --instance-id 0
```

## 分层阅读与快速跳转

::: tip 按读者角色快速检索
- **初次接触 / 快速上手**：从[一次可回退安装](/walkthroughs/one-real-run)开始，跟随一次真实 Root 装配过程，体验证据检查、候选写入与启动验证。
- **镜像机制 / 恢复边界**：从[镜像与恢复边界](/modules/image-boundaries)及[发行版与兼容性](/modules/release-lifecycle)了解镜像差分结构与版本约束。
- **架构源码 / 核心贡献**：从[代码地图](/code-map)定位挂载驱动与补丁逻辑，查阅[源码证据](/references/source-evidence)核对真实测试基准。
:::

---

## 完整项目指南概览

<!--@include: ../repo-docs/README.md-->
