// SPDX-License-Identifier: GPL-3.0-or-later
import { defineConfig } from 'vitepress'
import { guideLinks } from './guide-links.mjs'

const navigation = [
  ['README.md', '', '阅读路径'],
  ['walkthroughs/one-real-run.md', 'walkthroughs/one-real-run', '一次可回退安装'],
  ['code-map.md', 'code-map', '代码地图'],
  ['modules/release-lifecycle.md', 'modules/release-lifecycle', '发行版与兼容性'],
  ['modules/image-boundaries.md', 'modules/image-boundaries', '镜像与恢复边界'],
  ['glossary.md', 'glossary', '术语表'],
  ['references/source-evidence.md', 'references/source-evidence', '源码证据'],
  ['references/quality-review.md', 'references/quality-review', '文档质量复核'],
  ['change-log.md', 'change-log', '同步记录'],
]
const pages = Object.fromEntries(navigation.map(([file, route]) => ['repo-docs/' + file, route]))

export default defineConfig({
  lang: 'zh-CN',
  title: 'MuMu ReSukiSU 项目指南',
  description: '固定引擎的 Root 适配、离线构建与可回退安装',
  cleanUrls: true,
  markdown: { config: guideLinks(pages) },
  themeConfig: {
    siteTitle: 'MuMu ReSukiSU',
    nav: [
      { text: '官网首页', link: '/' },
      { text: '文档指南', link: '/walkthroughs/one-real-run' },
      { text: '代码地图', link: '/code-map' },
      { text: '镜像与恢复', link: '/modules/image-boundaries' },
      { text: 'GitHub', link: 'https://github.com/CyberNova2333/mumu-resukisu' },
    ],
    sidebar: [{ text: '项目指南', items: navigation.map(([, route, text]) => ({ text, link: '/' + route })) }],
    search: { provider: 'local' },
    outline: { level: [2, 3], label: '本页目录' },
    docFooter: { prev: '上一页', next: '下一页' },
    returnToTopLabel: '返回顶部',
    sidebarMenuLabel: '目录',
    darkModeSwitchLabel: '主题',
  },
})
