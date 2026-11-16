// SPDX-License-Identifier: AGPL-3.0-only
import { defineConfig } from 'vitepress'
import { guideLinks } from './guide-links.mjs'

const pages: Record<string, string> = {
  'repo-docs/README.md': 'docs/guide',
  'repo-docs/walkthroughs/one-real-run.md': 'docs/walkthrough',
  'repo-docs/code-map.md': 'docs/code-map',
  'repo-docs/modules/image-boundaries.md': 'docs/image-boundaries',
  'repo-docs/modules/release-lifecycle.md': 'docs/release-lifecycle',
  'repo-docs/glossary.md': 'docs/glossary',
  'repo-docs/references/source-evidence.md': 'docs/source-evidence',
  'repo-docs/references/quality-review.md': 'docs/quality-review',
  'repo-docs/change-log.md': 'docs/change-log',
  'docs-site/index.md': '',
  'docs-site/docs/index.md': 'docs/',
  'docs-site/docs/configuration.md': 'docs/configuration',
  'docs-site/docs/faq.md': 'docs/faq',
  'docs-site/docs/architecture.md': 'docs/architecture',
  'docs-site/docs/validation.md': 'docs/validation',
}

export default defineConfig({
  cleanUrls: true,
  markdown: {
    config: guideLinks(pages),
  },
  locales: {
    root: {
      label: '简体中文',
      lang: 'zh-CN',
      title: 'MuMu ReSukiSU 文档站',
      description: '固定 MuMu Android 15 x86_64 引擎的 ReSukiSU 离线镜像构建、COW 审计与可回退单实例安装工具文档。',
      themeConfig: {
        siteTitle: 'MuMu ReSukiSU',
        nav: [
          { text: '首页', link: '/' },
          { text: '起步与构建', link: '/docs/' },
          { text: '架构与边界', link: '/docs/architecture' },
          { text: '源码与验证', link: '/docs/code-map' },
          { text: 'GitHub', link: 'https://github.com/CyberNova2333/mumu-resukisu' },
        ],
        sidebar: [
          {
            text: '起步与构建安装',
            items: [
              { text: '项目全貌与完整构建流程', link: '/docs/' },
              { text: '输入契约、配置与脚本参数', link: '/docs/configuration' },
              { text: '常见排错与边界 FAQ', link: '/docs/faq' },
            ],
          },
          {
            text: '镜像边界与校验契约',
            items: [
              { text: '系统架构与数据流全景', link: '/docs/architecture' },
              { text: '一次可回退安装全流程', link: '/docs/walkthrough' },
              { text: '镜像变化与恢复边界', link: '/docs/image-boundaries' },
              { text: '发行版获取与兼容性边界', link: '/docs/release-lifecycle' },
            ],
          },
          {
            text: '源码导览与版本迁移',
            items: [
              { text: '代码地图', link: '/docs/code-map' },
              { text: '阶段验证与架构决策', link: '/docs/validation' },
              { text: '源码证据台账', link: '/docs/source-evidence' },
              { text: '工程指南入口', link: '/docs/guide' },
              { text: '质量复核与术语表', link: '/docs/quality-review' },
              { text: '核心术语表', link: '/docs/glossary' },
              { text: '文档同步记录', link: '/docs/change-log' },
            ],
          },
        ],
        outline: { level: [2, 3], label: '本页目录' },
        docFooter: { prev: '上一页', next: '下一页' },
        returnToTopLabel: '返回顶部',
        sidebarMenuLabel: '目录',
        darkModeSwitchLabel: '主题',
      },
    },
    en: {
      label: 'English',
      lang: 'en-US',
      link: '/en/',
      title: 'MuMu ReSukiSU Documentation',
      description: 'Offline ReSukiSU root image builder, whole-disk VDI COW auditor, and reversible single-instance installer for fingerprinted MuMu Android 15 x86_64.',
      themeConfig: {
        siteTitle: 'MuMu ReSukiSU',
        nav: [
          { text: 'Home', link: '/en/' },
          { text: 'Build & Install', link: '/en/docs/' },
          { text: 'Architecture', link: '/en/docs/architecture' },
          { text: 'Code & Validation', link: '/en/docs/code-map' },
          { text: 'GitHub', link: 'https://github.com/CyberNova2333/mumu-resukisu' },
        ],
        sidebar: [
          {
            text: 'Getting Started & Build',
            items: [
              { text: 'Overview & Build Workflow', link: '/en/docs/' },
              { text: 'Inputs, Contracts & CLI Matrix', link: '/en/docs/configuration' },
              { text: 'Troubleshooting & FAQ', link: '/en/docs/faq' },
            ],
          },
          {
            text: 'Boundaries & Safety Contracts',
            items: [
              { text: 'System Architecture', link: '/en/docs/architecture' },
              { text: 'One Reversible Run Walkthrough', link: '/en/docs/walkthrough' },
              { text: 'Image & Recovery Boundaries', link: '/en/docs/image-boundaries' },
              { text: 'Release Lifecycle & Compatibility', link: '/en/docs/release-lifecycle' },
            ],
          },
          {
            text: 'Code Map & Validation',
            items: [
              { text: 'Code Map', link: '/en/docs/code-map' },
              { text: 'Staged Validation & Decisions', link: '/en/docs/validation' },
              { text: 'Source Evidence', link: '/en/docs/source-evidence' },
              { text: 'Engineering Guide Index', link: '/en/docs/guide' },
              { text: 'Quality Review', link: '/en/docs/quality-review' },
              { text: 'Glossary', link: '/en/docs/glossary' },
              { text: 'Sync Changelog', link: '/en/docs/change-log' },
            ],
          },
        ],
        outline: { level: [2, 3], label: 'On this page' },
        docFooter: { prev: 'Previous page', next: 'Next page' },
        returnToTopLabel: 'Back to top',
        sidebarMenuLabel: 'Menu',
        darkModeSwitchLabel: 'Theme',
      },
    },
  },
  themeConfig: {
    search: { provider: 'local' },
  },
})
