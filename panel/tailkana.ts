// SPDX-License-Identifier: GPL-3.0-or-later
type Choice = { roman: string, kana: string }
type Meta = { panelWidth?: number, autoExpanded?: boolean, compactIndices?: number[], autoExpandAction?: number, autoCollapseAction?: number, ai?: {text: string, reading: string, status: string, action: number, elapsedMs: number}, pinyinCorrectionSlots?: number[], corrections?: Record<string, string>, autoExpandAllowed?: boolean, glass?: boolean, customColors?: boolean, backgroundColor?: string, textColor?: string, highlightColor?: string, opacity?: number, showMeaning?: boolean, chineseSlots?: number[], emptySlots?: number[], definition?: {word: string, reading: string, text: string, source: string}, appearance?: string, furigana?: boolean, panelScale?: number, leftShiftEnabled?: boolean, rightShiftEnabled?: boolean, raw: string, cursor: number, expanded: boolean, kanaMode: boolean, selected: number, katakana?: boolean, swapKeys?: boolean, current: Choice & { length: number }, choices: Choice[] }
type PanelCandidate = { text: string, comment?: string }
let root: HTMLElement
let host: HTMLElement
let meta: Meta | null = null
let candidates: PanelCandidate[] = []
let candidatesReady = false
let pressedControl: { button: HTMLButtonElement, raw: string | undefined, candidates: string, aiAction: number | undefined } | null = null
let deferredRender = false
function finishPointerInteraction() {
  pressedControl = null
  if (deferredRender) { deferredRender = false; render() }
}
function element(tag: string, cls: string, text = '') {
  const e = document.createElement(tag)
  e.className = cls
  e.textContent = text
  return e
}
function button(cls: string, label: string, action: () => void) {
  const b = element('button', cls, label) as HTMLButtonElement
  b.type = 'button'
  b.addEventListener('mousedown', e => {
    if (e.button !== 0 || b.disabled) return
    e.preventDefault()
    e.stopPropagation() // Do not start the upstream panel drag gesture.
    pressedControl = { button: b, raw: meta?.raw, candidates: JSON.stringify(candidates), aiAction: meta?.ai?.action }
  })
  b.addEventListener('mouseup', e => {
    if (pressedControl?.button === b) e.stopPropagation()
  })
  b.addEventListener('click', e => {
    e.stopPropagation()
    const press = pressedControl
    const valid = !b.disabled && (!press || (press.button === b && press.raw === meta?.raw && press.candidates === JSON.stringify(candidates) && (!b.classList.contains('tailkana-ai') || press.aiAction === meta?.ai?.action)))
    pressedControl = null
    if (valid) action()
    finishPointerInteraction()
  })
  return b
}
const action = (id: number) => window.fcitx('tabAction', id)
export function initTailKana(container: HTMLElement) {
  host = container.querySelector('.fcitx-panel-blur')!
  root = element('div', 'tailkana')
  host.append(root)
  document.addEventListener('mouseup', () => {
    // Mouseup precedes click. Keep the DOM stable until click is dispatched;
    // also release a drag that ended outside the original control.
    const press = pressedControl
    if (press) setTimeout(() => { if (pressedControl === press) finishPointerInteraction() }, 0)
  }, true)
  window.addEventListener('blur', finishPointerInteraction)
  window.matchMedia('(prefers-color-scheme: dark)').addEventListener('change', applyTheme)
}
export function tailKanaMetadata(formatted: [string, number][]) {
  const text = formatted.map(([s]) => s).join('')
  if (!text.startsWith('tailkana:')) {
    meta = null
    candidatesReady = false
    host?.classList.remove('tailkana-active')
    host?.closest('#fcitx-theme')?.classList.remove('tailkana-themed')
    window.fcitx('tailkanaAppearance', 0)
    return false
  }
  try {
    meta = JSON.parse(text.slice(9)) as Meta
    candidatesReady = false
    host.classList.add('tailkana-active')
    host.closest('#fcitx-theme')?.classList.remove('fcitx-hidden')
    render()
    return true
  }
  catch { return false }
}
export function tailKanaCandidates(cands: PanelCandidate[]) {
  if (!meta) return false
  candidates = cands
  candidatesReady = true
  render()
  return true
}
function rgba(hex: string, opacity: number) { return `rgba(${[1, 3, 5].map(i => Number.parseInt(hex.slice(i, i + 2), 16)).join(', ')}, ${opacity})` }
function applyTheme() {
  if (!meta || !host) return
  const requested = meta.appearance ?? 'System'
  const dark = requested === 'Dark' || (requested !== 'Light' && window.matchMedia('(prefers-color-scheme: dark)').matches)
  const theme = host.closest<HTMLElement>('#fcitx-theme')
  if (theme) {
    theme.classList.add('tailkana-themed')
    theme.dataset.tailkanaScheme = dark ? 'dark' : 'light'
    const glass = meta.glass !== false
    theme.dataset.tailkanaGlass = String(glass)
    const opacity = glass ? Math.min(100, Math.max(0, meta.opacity ?? 20)) / 100 : 1
    const valid = (color?: string) => /^#[\da-f]{6}$/i.test(color || '')
    for (const [key, color] of [['background', meta.backgroundColor], ['ink', meta.textColor], ['highlight', meta.highlightColor]] as const) {
      if (meta.customColors && valid(color)) theme.style.setProperty(`--tk-${key}`, key === 'background' ? rgba(color!, opacity) : color!)
      else theme.style.removeProperty(`--tk-${key}`)
    }
    if (meta.customColors && valid(meta.textColor)) theme.style.setProperty('--tk-muted', rgba(meta.textColor!, 0.8))
    else theme.style.removeProperty('--tk-muted')
    for (const [key, alpha] of [['line', 0.22], ['border', 0.3], ['focus', 0.8]] as const) {
      if (meta.customColors && valid(meta.textColor)) theme.style.setProperty(`--tk-${key}`, rgba(meta.textColor!, alpha))
      else theme.style.removeProperty(`--tk-${key}`)
    }
    theme.style.setProperty('--tk-opacity', String(opacity))
    window.fcitx('tailkanaAppearance', ((requested === 'Light' ? 1 : requested === 'Dark' ? 2 : 3) + (glass ? 0 : 3)) as 1 | 2 | 3 | 4 | 5 | 6)
  }
}
// Fit each compact cell independently, including its reading. Expansion is
// reserved for text that cannot fit at 16px; list rows keep the normal 20px.
function fitCompact(row: HTMLElement) {
  let allFit = true
  for (const cell of row.querySelectorAll<HTMLElement>('.tailkana-candidate')) {
    const word = cell.querySelector<HTMLElement>('.tailkana-word')
    if (!word) continue
    const reading = cell.querySelector<HTMLElement>('.tailkana-furigana')
    let fits = false
    for (let size = 20; size >= 16; --size) {
      word.style.fontSize = `${size}px`
      if (reading) reading.style.fontSize = `${size / 2}px`
      fits = [...cell.querySelectorAll<HTMLElement>('.tailkana-word, .tailkana-base, .tailkana-furigana')]
        .every(text => text.scrollWidth <= text.clientWidth + 1)
      if (fits) break
    }
    allFit &&= fits
  }
  return allFit
}
function render() {
  if (!meta || !root) return
  if (pressedControl) { deferredRender = true; return }
  applyTheme()
  root.style.zoom = String((meta.panelScale ?? 100) / 100)
  root.style.width = `${Math.max(360, Math.min(1000, meta.panelWidth ?? 520))}px`
  root.replaceChildren()
  const left = element('section', 'tailkana-main')
  const expand = button('tailkana-expand', meta.expanded ? '−' : '+', () => action(200))
  expand.title = 'Tab 展开 / 收起候选'
  expand.setAttribute('aria-label', meta.expanded ? '收起候选' : '展开候选')
  if (meta.expanded) {
    const list = element('div', 'tailkana-more')
    // One row per candidate, including the first three. No duplicate top strip.
    for (let i = 0; i < candidates.length; ++i) list.append(candidateButton(i, 'tailkana-more-candidate'))
    left.append(list)
  } else {
    const short = element('div', 'tailkana-short')
    for (let i = 0; i < 3; ++i) short.append(candidateButton(i, 'tailkana-candidate'))
    short.append(expand)
    left.append(short)
  }
  if (meta.expanded && meta.autoExpanded && meta.compactIndices) {
    const probe = element('div', 'tailkana-compact-probe')
    probe.setAttribute('aria-hidden', 'true')
    probe.inert = true
    for (let slot = 0; slot < 3; ++slot) probe.append(candidateButton(meta.compactIndices[slot] ?? -1, 'tailkana-candidate'))
    probe.append(element('span', ''))
    left.append(probe)
  }
  if (meta.ai) {
    const ai = meta.ai
    const labels: Record<string, string> = { loading: 'AI 预热中…', pending: 'AI 推荐中…', unavailable: 'AI 暂不可用', incomplete: 'AI 等待拼写完成', length: 'AI 候选超过长度限制', off: 'AI 已关闭' }
    const row = button('tailkana-ai', '', () => action(ai.action))
    row.disabled = !ai.text
    row.setAttribute('aria-label', ai.text ? `AI 推荐：${ai.text}` : (labels[ai.status] || 'AI 暂无推荐'))
    row.append(element('span', 'tailkana-ai-label', 'AI'), element('span', 'tailkana-ai-text', ai.text || labels[ai.status] || '暂无推荐'))
    if (ai.text) row.append(element('span', 'tailkana-ai-shortcut', '⌥↵'))
    row.title = ai.text ? `${ai.text}（${ai.reading}） · ${ai.elapsedMs} ms · Option＋Enter 选用` : '设置中可以关闭 AI，或调整预测和上下文长度'
    left.append(row)
  }
  const hints = element('div', 'tailkana-hints')
  const tab = button('tailkana-hint', meta.expanded ? 'Tab 收起' : 'Tab 展开', () => action(200))
  const space = button('tailkana-hint', meta.swapKeys ? '空格 确定' : '空格 变换', () => action(meta?.swapKeys ? 205 : 204))
  const enter = button('tailkana-hint', meta.swapKeys ? 'Enter 变换' : 'Enter 确定', () => action(meta?.swapKeys ? 204 : 205))
  const swap = button('tailkana-swap', '⇄', () => action(203))
  swap.title = '对调空格与 Enter：确定 / 变换'
  swap.setAttribute('aria-label', '对调确定和变换')
  swap.setAttribute('aria-pressed', String(!!meta.swapKeys))
  const settings = button('tailkana-settings', '⚙', () => action(206))
  settings.title = 'TailKana 设置'
  settings.setAttribute('aria-label', 'TailKana 设置')
  hints.append(tab, space, enter, swap, settings)
  if (meta.expanded) hints.append(expand)
  left.append(hints)
  const cross = element('section', 'tailkana-cross')
  const count = element('div', 'tailkana-count')
  count.append(element('strong', '', String(meta.current.length).padStart(2, '0')))
  cross.append(count)
  const positions = ['up', 'left', 'right', 'down']
  for (let i = 0; i < 4; ++i) {
    const c = meta.choices[i] || { roman: '', kana: '' }
    const b = button(`tailkana-branch tailkana-${positions[i]}`, '', () => action(100 + i))
    b.disabled = !c.roman
    b.title = `右 Shift + ${['↑', '←', '→', '↓'][i]}`
    b.append(element('span', 'tailkana-kana', c.kana || '—'), element('span', 'tailkana-roman', c.roman))
    cross.append(b)
  }
  const current = element('div', 'tailkana-current')
  current.append(element('span', 'tailkana-caption', '当前'), element('span', 'tailkana-kana', meta.current.kana || meta.current.roman), element('span', 'tailkana-roman', meta.current.kana ? meta.current.roman : '未完成'))
  cross.append(current)
  const key = element('div', 'tailkana-cross-key')
  const meaning = button('tailkana-meaning-switch', '释义', () => action(208))
  meaning.setAttribute('aria-label', '显示中文释义')
  key.append(meaning)
  if (meta.rightShiftEnabled !== false) key.append(element('span', '', '⇧ + 方向'))
  key.title = '右 Shift + 方向键修改末尾'
  cross.append(key)
  const mode = button('tailkana-mode', meta.kanaMode ? '假名' : '罗马字', () => action(201))
  mode.title = '输入位置显示：罗马字 / 实时假名'
  cross.append(mode)
  const script = button('tailkana-script', meta.katakana ? 'ア' : 'あ', () => action(202))
  script.title = '平假名 / 片假名；按住左 Shift 临时切换'
  script.setAttribute('aria-label', '切换平假名和片假名')
  script.setAttribute('aria-pressed', String(!!meta.katakana))
  cross.append(script)
  root.append(left, meta.showMeaning ? definitionPanel() : cross)
  if (!candidatesReady) return
  const snapshot = meta
  const snapshotCandidates = candidates
  requestAnimationFrame(() => {
    if (meta !== snapshot || candidates !== snapshotCandidates || !root.offsetHeight) return
    if (meta.expanded) {
      const probe = root.querySelector<HTMLElement>('.tailkana-compact-probe')
      if (meta.autoExpanded && probe && fitCompact(probe)) action(meta.autoCollapseAction ?? 211)
      // Keep keyboard-selected rows in the scroll viewport without scrolling
      // the surrounding web page or stealing the client's keyboard focus.
      const list = root.querySelector<HTMLElement>('.tailkana-more')
      const selected = list?.querySelector<HTMLElement>('.tailkana-selected')
      if (list && selected) {
        if (selected.offsetTop < list.scrollTop) list.scrollTop = selected.offsetTop
        else if (selected.offsetTop + selected.offsetHeight > list.scrollTop + list.clientHeight)
          list.scrollTop = selected.offsetTop + selected.offsetHeight - list.clientHeight
      }
    } else {
      const short = root.querySelector<HTMLElement>('.tailkana-short')
      if (short && !fitCompact(short) && meta.autoExpandAllowed !== false) action(meta.autoExpandAction ?? 209)
    }
  })
}
function candidateButton(index: number, cls: string) {
  const c = candidates[index]
  const b = button(cls, '', () => window.fcitx('select', index))
  b.disabled = !c || !!meta?.emptySlots?.includes(index)
  if (c) {
    b.title = c.comment ? `${c.text}（${c.comment}）` : c.text
  }
  if (index === meta?.selected) b.classList.add('tailkana-selected')
  const indexColumn = element('span', 'tailkana-index')
  indexColumn.append(element('span', 'tailkana-number', String(index + 1)))
  const chinese = !!meta?.chineseSlots?.includes(index)
  if (chinese) {
    b.classList.add('tailkana-chinese')
    const badge = element('span', 'tailkana-chinese-badge', 'C')
    badge.title = '中文';badge.setAttribute('aria-label', '中文')
    indexColumn.append(badge)
  }
  const correction = meta?.corrections?.[String(index)]
  if (correction) {
    const pinyin = !!meta?.pinyinCorrectionSlots?.includes(index)
    const badge = element('span', 'tailkana-correction-badge', pinyin ? 'B' : 'E')
    badge.title = pinyin ? '模糊拼音' : '纠错'
    badge.setAttribute('aria-label', badge.title)
    indexColumn.append(badge)
    b.title += `；${pinyin ? '模糊拼音' : '纠错'}：${meta?.raw} → ${correction}`
  }
  b.append(indexColumn)
  const hasKanji = /[\u3400-\u9FFF\uF900-\uFAFF\u3005\u3006\u{20000}-\u{323AF}]/u.test(c?.text || '')
  if (!chinese && meta?.furigana !== false && c?.comment && hasKanji) {
    const ruby = element('ruby', 'tailkana-word')
    ruby.append(element('span', 'tailkana-base', c.text), element('rt', 'tailkana-furigana', c.comment))
    b.append(ruby)
  } else b.append(element('span', 'tailkana-word', c?.text || '—'))
  return b
}

function definitionPanel() {
  const panel = element('section', 'tailkana-gloss')
  const definition = meta?.definition
  const header = element('div', 'tailkana-gloss-header')
  const word = element('strong', '', definition?.word || '中文释义')
  word.title = definition?.word || ''
  const wheel = button('tailkana-gloss-back', '轮盘', () => action(208))
  wheel.setAttribute('aria-label', '显示十字轮盘')
  header.append(word, wheel)
  const text = element('div', 'tailkana-gloss-text', definition?.text || '暂无释义')
  text.title = definition?.text || '本地词典暂未收录这个候选'
  panel.append(header, text)
  if (definition?.source) panel.append(button('tailkana-gloss-source', '维基词典 ↗', () => action(207)))
  return panel
}
