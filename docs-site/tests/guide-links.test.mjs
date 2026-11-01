// SPDX-License-Identifier: GPL-3.0-or-later
import assert from 'node:assert/strict'
import test from 'node:test'
import { createMarkdownRenderer } from 'vitepress'
import { guideLinks } from '../.vitepress/guide-links.mjs'

const pages = { 'repo-docs/README.md': '', 'repo-docs/modules/example.md': 'modules/example' }
const md = await createMarkdownRenderer(process.cwd(), { config: guideLinks(pages) })
const env = () => ({ frontmatter: { repoGuideSource: 'repo-docs/modules/example.md' } })

test('include references route inside the guide while source files remain readable', () => {
  const html = md.render('[返回](../README.md#阅读路径) [实现](../../src/example.py)', env())
  assert.match(html, /href="\/#阅读路径"/)
  assert.match(html, /实现（仓库文件：<code>src\/example.py<\/code>）/)
  assert.doesNotMatch(html, /href="[^\"]*example\.py/)
})

test('unmapped guide pages and repository escape stop the build', () => {
  assert.throws(() => md.render('[未知](missing.md)', env()), /no route/)
  assert.throws(() => md.render('[越界](../../../outside.md)', env()), /escapes repository/)
})

test('external and in-page links remain navigable', () => {
  const html = md.render('[外部](https://example.org/guide) [本页](#本页)', env())
  assert.match(html, /href="https:\/\/example\.org\/guide"/)
  assert.match(html, /href="#本页"/)
})
