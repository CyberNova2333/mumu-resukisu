// SPDX-License-Identifier: GPL-3.0-or-later
import { posix } from 'node:path'

// Includes retain repository-relative links. Publish guide pages only; source
// references stay readable without copying the source tree into the site.
export function guideLinks(pages) {
  return (markdown) => {
    markdown.core.ruler.after('inline', 'guide-link-targets', (state) => {
      const source = state.env.frontmatter?.repoGuideSource
      if (!source) return
      for (const block of state.tokens) {
        const tokens = block.children ?? []
        for (let index = 0; index < tokens.length; index++) {
          const opening = tokens[index]
          if (opening.type !== 'link_open') continue
          const href = opening.attrGet('href')
          if (!href || /^(?:[a-z][a-z\d+.-]*:|\/|#)/i.test(href)) continue
          const fragmentAt = href.indexOf('#')
          const file = fragmentAt < 0 ? href : href.slice(0, fragmentAt)
          const fragment = fragmentAt < 0 ? '' : href.slice(fragmentAt)
          const target = posix.normalize(posix.join(posix.dirname(source), decodeURIComponent(file)))
          if (target.startsWith('../')) throw new Error(`Guide reference escapes repository: ${href}`)
          if (Object.hasOwn(pages, target)) {
            opening.attrSet('href', '/' + pages[target] + fragment)
            continue
          }
          if (target.startsWith('repo-docs/')) throw new Error(`Guide page has no route: ${target}`)
          const closing = tokens.slice(index + 1).find((token) => token.type === 'link_close')
          if (!closing) throw new Error('Unclosed source reference')
          Object.assign(opening, { type: 'html_inline', tag: '', nesting: 0, attrs: null, content: '<span>' })
          Object.assign(closing, {
            type: 'html_inline', tag: '', nesting: 0, attrs: null,
            content: `（仓库文件：<code>${markdown.utils.escapeHtml(target + fragment)}</code>）</span>`,
          })
        }
      }
    })
  }
}
