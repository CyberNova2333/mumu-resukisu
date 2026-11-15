---
layout: home

hero:
  name: MuMu ReSukiSU
  text: 固定引擎 Root 镜像构建与可回退安装
  tagline: 面向指纹匹配的 MuMu Android 15 x86_64 模拟器，完成离线原生构建、整盘 VDI COW 字节审计与 Windows 独占句柄单实例事务写入。
  actions:
    - theme: brand
      text: 起步与完整构建
      link: /docs/
    - theme: alt
      text: 架构与安全边界
      link: /docs/architecture
    - theme: alt
      text: GitHub 仓库
      link: https://github.com/CyberNova2333/mumu-resukisu

features:
  - title: 固定目标指纹守卫
    details: 严格核对基础磁盘与启动/系统/厂商分区 SHA256、虚拟容量及 235 个内核符号清单，不通过改摘要或版本号字符串放行未知引擎。
  - title: 整盘 VDI COW 字节审计
    details: 离线重组差分候选时按父盘、差分盘、零块与未分配块语义重读全部 2,465,270,784 虚拟字节，确保允许分区之外无任何字节越界。
  - title: 独占写盘与原子回退
    details: Windows 控制器连续三次确认目标实例已停机且内置 Root 关闭后获取独占写句柄，保存完整原盘备份并支持基于事务号的精确恢复。
---

## 按身份 / 目标快速入口

| 你的身份与目标 | 优先阅读入口 | 读完后获得什么 |
| --- | --- | --- |
| 首次构建并写入测试实例 | [项目全貌与完整构建流程](/docs/) | Linux/WSL 离线构建、分区重组与 Windows 绑定安装/回退命令链。 |
| 核对 JSON 契约与命令行参数 | [输入契约、配置与脚本参数](/docs/configuration) | `profile.json`、`target-profile.json`、`userspace.json` 与 10 个 CLI 子命令说明。 |
| 理解三域隔离与安全红线 | [系统架构与数据流全景](/docs/architecture) | 离线构建、Windows 写盘控制与 Android 早期启动预置的三域边界与交互图。 |
| 评估一次可回退候选交接 | [一次可回退安装全流程](/docs/walkthrough) | 从输入核验、整盘审计到写盘事务与故障恢复的端到端衔接机制。 |
| 判断镜像字节与数据盘边界 | [镜像变化与恢复边界](/docs/image-boundaries) | 系统盘回退与数据盘原厂 `/system/xbin/dusk` 持久备份的分离约束。 |
| 查看已验收阶段与代码定位 | [阶段验证与架构决策](/docs/validation) · [代码地图](/docs/code-map) | 分阶段实机检查范围、明确未覆盖项与逐目录源码/测试定位。 |
| 排查拒绝报错或构建门禁失败 | [常见问题与排错诊断](/docs/faq) | 指纹拒绝、写锁冲突、未知 daemon 阻止启动与符号缺失的诊断路径。 |

## 架构拓扑全景

<div class="archify-diagram-card" style="margin: 1.5rem 0; border: 1px solid var(--vp-c-border); border-radius: 12px; overflow: hidden; background: var(--vp-c-bg-soft); box-shadow: 0 4px 16px rgba(0,0,0,0.06);">
  <div style="display: flex; justify-content: space-between; align-items: center; padding: 10px 18px; background: var(--vp-c-bg-elv); border-bottom: 1px solid var(--vp-c-border); font-size: 13px;">
    <span style="font-weight: 600; color: var(--vp-c-text-1); display: flex; align-items: center; gap: 8px;">
      <span>📐</span> MuMu ReSukiSU 三域交互架构图（支持平移、缩放与节点探查）
    </span>
    <a href="/diagrams/resukisu-architecture.html" target="_blank" rel="noreferrer" style="font-size: 12px; color: var(--vp-c-brand-1); text-decoration: none; font-weight: 500;">
      在新窗口打开 ↗
    </a>
  </div>
  <iframe src="/diagrams/resukisu-architecture.html" width="100%" height="520px" frameborder="0" style="display: block; border: none;" allowfullscreen loading="lazy"></iframe>
</div>

## 快速起步验证

在 Python 3.11+ 环境安装可编辑包并运行主机离线校验套件（不触碰任何模拟器磁盘）：

```sh
python -m pip install -e .
python -m unittest discover -s tests -v
```

完整的本地输入准备、原生与用户态编译、VDI 候选重组及 Windows 实例绑定安装步骤，请直接前往 [起步与完整构建流程](/docs/)。
