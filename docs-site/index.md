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
      text: API 与 CLI 参考
      link: /api-reference/
    - theme: alt
      text: 架构与安全边界
      link: /docs/architecture

features:
  - title: 固定目标指纹守卫
    details: 严格核对基础磁盘与启动/系统/厂商分区 SHA256、虚拟容量及 235 个内核符号清单，不通过改摘要或版本号字符串放行未知引擎。
  - title: 整盘 VDI COW 字节审计
    details: 离线重组差分候选时按父盘、差分盘、零块与未分配块语义重读全部 2,465,270,784 虚拟字节，确保允许分区之外无任何字节越界。
  - title: 独占写盘与原子回退
    details: Windows 控制器连续三次确认目标实例已停机且内置 Root 关闭后获取独占写句柄，保存完整原盘备份并支持基于事务号的精确恢复。
---

::: info 它能做到什么程度（已实现能力）
- 在 Linux/WSL 下离线完成 `kernelsu.ko`、`driver-init`（`vermagic=6.1.90-perf+ `、`235` 个外部符号核对）、`resukisu-migrate-prestage` 与 `boot`/`system`/`vendor` 分区制备，并在主机跑通 ASAN/UBSAN 与 32 位模型自测。
- 跨平台离线按 VDI COW 1 MiB 块语义重读并审计全部 `2,465,270,784` 逻辑字节；Windows 端通过 `MuMuManager.exe` 与 `CreateFileW(share=0)` 完成可逆 `rollback.vdi` 事务写入与原子恢复。
- 所有 `10` 个 `mumu-resukisu` 主包子命令、`8` 组辅助脚本与 JSON 契约集中在 [API 与 CLI 参考](/api-reference/) 供交互检索。
:::

::: warning 明确不做什么（边界与硬限制）
- **不支持 Android 12 或未知 MuMu 引擎**：唯一面向指纹锁死的 MuMu Android 15 `x86_64` 目标快照；严禁修改摘要跳过指纹校验。
- **不分发磁盘/内核/APK，也不代跑 Windows 独占锁于 Linux**：原始 `base.vdi`、`source.vdi` 与 `ksud` 须自备；Linux `unittest` 会跳过 `29` 项 Win32 锁用例，且当前 `refactoredDeviceValidation` 为 `false`。
- **写后不自动重启、恢复不触碰 `/data`**：`fetch-release` 与 `register-release.py`（`catalogUpdated: false`）均不改写固定目标；`scripts/build-frida.py` 尚属独立可选辅助工具。
:::

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

## 文档分层导航

::: tip 按照读者需求快速跳转
- **首次构建并写入测试实例**：阅读 [项目全貌与完整构建流程](/docs/)，走通 Linux/WSL 离线构建、分区重组与 Windows 绑定安装/回退命令链；
- **核对 JSON 契约与排查报错**：查阅 [输入契约、配置与脚本参数](/docs/configuration) 与 [常见问题与排错诊断](/docs/faq)，核对 `profile.json`、10 个 CLI 子命令与指纹拒绝诊断；
- **理解三域隔离、走查与恢复边界**：查阅 [系统架构与数据流全景](/docs/architecture)、[一次可回退安装全流程](/docs/walkthrough) 与 [镜像变化与恢复边界](/docs/image-boundaries)；
- **查看已验收阶段与代码定位**：查阅 [阶段验证与架构决策](/docs/validation) 与 [代码地图](/docs/code-map)。
:::
