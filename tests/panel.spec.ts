import test, { expect } from '@playwright/test'
import { getCppCalls, init, setCandidates, setStyle, updateInputPanel } from './util'
const meta = {
  emptySlots: [], chineseSlots: [], raw: 'ti', cursor: 2, expanded: false, kanaMode: false, selected: 0, katakana: false, swapKeys: false,
  current: { roman: 'ti', kana: 'ち', length: 2 },
  choices: [{ roman: 'ki', kana: 'き' }, { roman: 'qi', kana: 'くぃ' }, { roman: 'tyo', kana: 'ちょ' }, { roman: 'tyu', kana: 'ちゅ' }],
}
const candidates = [{ text: '血', comment: 'ち' }, { text: 'ち', comment: 'ち' }, { text: '地', comment: 'ち' }, { text: '知', comment: 'ち' }, { text: '値', comment: 'ち' }, { text: 'チ', comment: 'ち' }]
test.beforeEach(async ({ page }) => { await init(page); await setStyle(page, {}); })
test('partition is visible on the very first input and does not overlap', async ({ page }) => {
  await updateInputPanel(page, 'ti', '', `tailkana:${JSON.stringify(meta)}`)
  await setCandidates(page, candidates.slice(0, 3), 0)
  await expect(page.locator('.tailkana')).toBeVisible()
  await expect(page.locator('.tailkana-candidate')).toHaveCount(3)
  await expect(page.locator('.tailkana-branch')).toHaveCount(4)
  const selectedColor = await page.locator('.tailkana-selected').evaluate(e => getComputedStyle(e).backgroundColor)
  expect(selectedColor).not.toBe('rgba(0, 0, 0, 0)')
  expect(selectedColor).not.toBe('transparent')
  const swapWidth = (await page.locator('.tailkana-swap').boundingBox())!.width
  expect(swapWidth).toBeGreaterThan(18)
  await expect(page.locator('.tailkana-count strong')).toHaveText('02')
  await expect(page.locator('.tailkana-current')).toContainText('ち')
  await expect(page.locator('.tailkana-current')).toContainText('ti')
  await expect(page.locator('.tailkana-candidate').first().locator('ruby')).toHaveText('血ち')
  await expect(page.locator('.tailkana-candidate').first().locator('rt')).toHaveText('ち')
  await expect(page.locator('.tailkana-candidate').nth(1).locator('rt')).toHaveCount(0)
  const left = await page.locator('.tailkana-main').boundingBox()
  const right = await page.locator('.tailkana-cross').boundingBox()
  expect(left!.x + left!.width).toBeLessThan(right!.x)
  expect(right!.y).toBeCloseTo(left!.y, 0)
  expect(right!.height).toBeLessThanOrEqual(85)
  expect(right!.width).toBeLessThanOrEqual(127)
  expect(left!.height).toBeLessThanOrEqual(85)
  await expect(page.locator('.tailkana-composition')).toHaveCount(0)
  await expect(page.locator('.tailkana-reading')).toHaveCount(0)
  await expect(page.getByText('影响字母数', { exact: true })).toHaveCount(0)
  const footer = await page.locator('.tailkana-hints').boundingBox()
  expect(left!.y + left!.height - footer!.y - footer!.height).toBeLessThan(5)
  const mode = await page.locator('.tailkana-mode').boundingBox()
  const script = await page.locator('.tailkana-script').boundingBox()
  expect(mode!.x).toBeLessThan(right!.x + 42)
  expect(mode!.y).toBeGreaterThan(right!.y + 56)
  expect(script!.x).toBeGreaterThan(right!.x + 84)
})
test('expanded candidates start at 1, one row each, with no compact strip', async ({ page }) => {
  await updateInputPanel(page, 'ti', '', `tailkana:${JSON.stringify({ ...meta, expanded: true })}`)
  await setCandidates(page, candidates, 0)
  await expect(page.locator('.tailkana-more-candidate')).toHaveCount(6)
  await expect(page.locator('.tailkana-short')).toHaveCount(0)
  await expect(page.locator('.tailkana-more-candidate').first().locator('.tailkana-number')).toHaveText('1')
  await expect(page.locator('.tailkana-more-candidate').nth(2).locator('rt')).toHaveText('ち')
  await expect(page.locator('.tailkana-cross')).toBeVisible()
  const rows = await page.locator('.tailkana-more-candidate').all()
  for (let i = 1; i < rows.length; ++i) {
    const previous = (await rows[i - 1].boundingBox())!
    const next = (await rows[i].boundingBox())!
    expect(next.y).toBeGreaterThanOrEqual(previous.y + previous.height)
    expect(next.x).toBe(previous.x)
  }
  await page.locator('.tailkana-more-candidate').nth(1).click()
  expect(await getCppCalls(page)).toContainEqual({ select: [1] })
})
test('cross, expansion and top display buttons use the native bridge without moving focus', async ({ page }) => {
  await updateInputPanel(page, 'ti', '', `tailkana:${JSON.stringify(meta)}`)
  await setCandidates(page, candidates.slice(0, 3), 0)
  await page.evaluate(() => { const input = document.createElement('input'); input.id = 'test-host'; document.body.append(input); input.focus(); })
  await page.locator('.tailkana-right').click()
  expect(await getCppCalls(page)).toContainEqual({ tabAction: [102] })
  expect(await page.evaluate(() => document.activeElement?.id)).toBe('test-host')
  await page.getByRole('button', { name: '展开候选', exact: true }).click()
  expect(await getCppCalls(page)).toContainEqual({ tabAction: [200] })
  await page.locator('.tailkana-mode').click()
  expect(await getCppCalls(page)).toContainEqual({ tabAction: [201] })
  await updateInputPanel(page, 'ti', '', `tailkana:${JSON.stringify({ ...meta, kanaMode: true })}`)
  await expect(page.locator('.tailkana-mode')).toHaveText('假名')
  await expect(page.locator('.tailkana-reading')).toHaveCount(0)
  await page.getByRole('button', { name: '切换平假名和片假名', exact: true }).click()
  expect(await getCppCalls(page)).toContainEqual({ tabAction: [202] })
  await page.getByRole('button', { name: '对调确定和变换', exact: true }).click()
  expect(await getCppCalls(page)).toContainEqual({ tabAction: [203] })
  await updateInputPanel(page, 'ti', '', `tailkana:${JSON.stringify({ ...meta, katakana: true, swapKeys: true })}`)
  await expect(page.locator('.tailkana-script')).toHaveText('ア')
  await page.getByRole('button', { name: '空格 确定', exact: true }).click()
  expect(await getCppCalls(page)).toContainEqual({ tabAction: [205] })
  await page.getByRole('button', { name: 'Enter 变换', exact: true }).click()
  expect(await getCppCalls(page)).toContainEqual({ tabAction: [204] })
})
test('commit hides the custom panel and another engine can render normally', async ({ page }) => {
  await updateInputPanel(page, 'ti', '', `tailkana:${JSON.stringify(meta)}`)
  await setCandidates(page, candidates.slice(0, 3), 0)
  await page.evaluate(() => window.fcitx.hidePanel())
  await expect(page.locator('.tailkana')).not.toBeVisible()
  await updateInputPanel(page, 'nihao')
  await setCandidates(page, [{ text: '你好' }], 0)
  await expect(page.locator('.tailkana')).not.toBeVisible()
  await expect(page.locator('.fcitx-text')).toHaveText('你好')
})

test('very long candidates expand after bounded shrink and use normal size in vertical rows', async ({ page }) => {
  const longCandidates = [
    { text: '日本語', comment: 'にほんご' },
    { text: '日本語と英語', comment: 'にほんごとえいご' },
    { text: '日本語を勉強してから映画を見に行きます', comment: 'にほんごをべんきょうしてからえいがをみにいきます' },
  ]
  await updateInputPanel(page, 'nihongo', '', `tailkana:${JSON.stringify({...meta,raw:'nihongo'})}`)
  await setCandidates(page, longCandidates, 0)
  await expect.poll(async () => (await getCppCalls(page)).some(call => call.tabAction?.[0] === 209)).toBe(true)
  expect(await page.locator('.tailkana-candidate').nth(2).locator('.tailkana-word').evaluate(e => getComputedStyle(e).fontSize)).toBe('16px')
  // Simulate the engine response; its full-list mapping has a separate C++ test.
  await updateInputPanel(page, 'nihongo', '', `tailkana:${JSON.stringify({...meta,raw:'nihongo',expanded:true})}`)
  await setCandidates(page,longCandidates,0)
  await expect(page.locator('.tailkana-short')).toHaveCount(0)
  const cells=page.locator('.tailkana-more-candidate')
  await expect(cells).toHaveCount(3)
  for(let i=0;i<3;++i) {
    const cell=cells.nth(i)
    const box=(await cell.boundingBox())!
    expect(await cell.locator('.tailkana-word').evaluate(e => getComputedStyle(e).fontSize)).toBe('20px')
    const word=(await cell.locator('.tailkana-base').boundingBox())!
    expect(word.x+word.width).toBeLessThanOrEqual(box.x+box.width)
    expect(await cell.locator('.tailkana-base').evaluate(e => e.scrollWidth<=e.clientWidth+1)).toBe(true)
  }
  await expect(cells.nth(1).locator('rt')).toHaveText('にほんごとえいご')
  await page.locator('.fcitx-panel').screenshot({path:test.info().outputPath('long-vertical.png')})
  await page.evaluate(() => {window.cppCalls=[]})
  await updateInputPanel(page,'nihongo','',`tailkana:${JSON.stringify({...meta,raw:'nihongo',autoExpandAllowed:false})}`)
  await setCandidates(page,longCandidates,0)
  await page.evaluate(() => new Promise(resolve => requestAnimationFrame(() => requestAnimationFrame(resolve))))
  expect((await getCppCalls(page)).some(call => call.tabAction?.[0]===209)).toBe(false)
})

test('rounded candidates, settings and display preferences', async ({ page }) => {
  await updateInputPanel(page, 'ti', '', `tailkana:${JSON.stringify({ ...meta, furigana: false, panelScale: 90 })}`)
  await setCandidates(page, candidates.slice(0, 3), 0)
  await expect(page.locator('.tailkana-furigana')).toHaveCount(0)
  expect(await page.locator('.tailkana-candidate').first().evaluate(e => parseFloat(getComputedStyle(e).borderRadius))).toBeGreaterThanOrEqual(6)
  expect((await page.locator('.tailkana').boundingBox())!.width).toBeCloseTo(520 * .9, 0)
  await page.getByRole('button', { name: 'TailKana 设置', exact: true }).click()
  expect(await getCppCalls(page)).toContainEqual({ tabAction: [206] })
})

test('theme covers the frame and candidates, respects explicit mode and follows system changes', async ({ page }) => {
  const show = async (appearance: string) => {
    await updateInputPanel(page, 'ti', '', `tailkana:${JSON.stringify({ ...meta, appearance })}`)
    await setCandidates(page, candidates.slice(0, 3), 0)
  }
  const theme = page.locator('#fcitx-theme')
  const background = () => page.locator('.tailkana-active').evaluate(e => getComputedStyle(e).backgroundColor)
  await page.emulateMedia({ colorScheme: 'dark' })
  await show('Light')
  await expect(theme).toHaveAttribute('data-tailkana-scheme', 'light')
  expect(await background()).toBe('rgba(247, 248, 247, 0.2)')
  await page.locator('.fcitx-panel').screenshot({ path: test.info().outputPath('moss-light.png') })
  const lightSize = await page.locator('.tailkana').boundingBox()
  await show('Dark')
  expect(await getCppCalls(page)).toContainEqual({ tailkanaAppearance: [2] })
  expect(await page.locator('.tailkana-active').evaluate(e => getComputedStyle(e).backdropFilter)).toBe('none')
  expect(await background()).toBe('rgba(28, 28, 28, 0.2)')
  expect(await page.locator('.tailkana-selected').evaluate(e => getComputedStyle(e).backgroundColor)).toBe('rgb(53, 91, 71)')
  expect(await page.locator('.tailkana-current').evaluate(e => getComputedStyle(e).backgroundColor)).toBe('rgb(53, 91, 71)')
  await page.locator('.fcitx-panel').screenshot({ path: test.info().outputPath('moss-dark.png') })
  const darkSize = await page.locator('.tailkana').boundingBox()
  expect(darkSize!.width).toBe(lightSize!.width)
  expect(darkSize!.height).toBe(lightSize!.height)
  await page.emulateMedia({ colorScheme: 'light' })
  await expect(theme).toHaveAttribute('data-tailkana-scheme', 'dark')
  await show('System')
  expect(await getCppCalls(page)).toContainEqual({ tailkanaAppearance: [3] })
  await expect(theme).toHaveAttribute('data-tailkana-scheme', 'light')
  await page.emulateMedia({ colorScheme: 'dark' })
  await expect(theme).toHaveAttribute('data-tailkana-scheme', 'dark')
  await updateInputPanel(page, 'nihao')
  await expect(theme).not.toHaveClass(/tailkana-themed/)
})

test('custom glass colors and opacity apply to the entire candidate panel', async ({ page }) => {
  await updateInputPanel(page, 'ti', '', `tailkana:${JSON.stringify({ ...meta, appearance: 'Dark', customColors: true, backgroundColor: '#112244', textColor: '#fff4de', highlightColor: '#334499', opacity: 55 })}`)
  await setCandidates(page, candidates.slice(0, 3), 0)
  expect(await page.locator('.tailkana-active').evaluate(e => getComputedStyle(e).backgroundColor)).toBe('rgba(17, 34, 68, 0.55)')
  expect(await page.locator('.tailkana-word').first().evaluate(e => getComputedStyle(e).color)).toBe('rgb(255, 244, 222)')
  expect(await page.locator('.tailkana-current').evaluate(e => getComputedStyle(e).backgroundColor)).toBe('rgb(51, 68, 153)')
  await page.locator('.fcitx-panel').screenshot({ path: test.info().outputPath('custom-glass.png') })
})

test('Chinese definition replaces the wheel and Chinese candidate stays in slot three', async ({ page }) => {
  await updateInputPanel(page, 'nihongo', '', `tailkana:${JSON.stringify({ ...meta, showMeaning: true, chineseSlots: [2], definition: { word: '日本語', reading: 'にほんご', text: '日语', source: '日本語' } })}`)
  await setCandidates(page, [{ text: '日本語', comment: 'にほんご' }, { text: '日本', comment: 'にほん' }, { text: '你好', comment: '拼音：nihao' }], 0)
  await expect(page.locator('.tailkana-cross')).toHaveCount(0)
  await expect(page.locator('.tailkana-gloss-text')).toHaveText('日语')
  await expect(page.locator('.tailkana-candidate').nth(2).locator('.tailkana-chinese-badge')).toHaveText('C')
  await expect(page.locator('.tailkana-candidate').nth(2).locator('ruby')).toHaveCount(0)
  await page.locator('.fcitx-panel').screenshot({ path: test.info().outputPath('meaning-chinese.png') })
  await page.locator('.tailkana-candidate').nth(2).click()
  expect(await getCppCalls(page)).toContainEqual({ select: [2] })
  await updateInputPanel(page, 'nihao', '', `tailkana:${JSON.stringify({ ...meta, showMeaning: true, emptySlots: [1], chineseSlots: [2] })}`)
  await expect(page.locator('.tailkana-candidate').nth(1)).toBeDisabled()
  await updateInputPanel(page, 'nihongo', '', `tailkana:${JSON.stringify({ ...meta, showMeaning: true, chineseSlots: [2], definition: { word: '日本語', reading: 'にほんご', text: '日语', source: '日本語' } })}`)
  await page.locator('.tailkana-gloss-source').click()
  expect(await getCppCalls(page)).toContainEqual({ tabAction: [207] })
  await page.getByRole('button', { name: '显示十字轮盘' }).click()
  expect(await getCppCalls(page)).toContainEqual({ tabAction: [208] })
  await updateInputPanel(page, 'unrecorded', '', `tailkana:${JSON.stringify({ ...meta, showMeaning: true, definition: { word: 'unrecorded', reading: '', text: '', source: '' } })}`)
  await expect(page.locator('.tailkana-gloss-text')).toHaveText('暂无释义')
  await expect(page.locator('.tailkana-gloss-source')).toHaveCount(0)
})


test('glass off is fully opaque, preserves custom colors, and restores the native effect on re-enable', async ({page}) => {
  const show = async (glass: boolean, opacity: number) => {
    await updateInputPanel(page,'ti','',`tailkana:${JSON.stringify({...meta,appearance:'Dark',glass,customColors:true,backgroundColor:'#112244',textColor:'#fff4de',highlightColor:'#334499',opacity})}`)
    await setCandidates(page,candidates.slice(0,3),0)
  }
  await show(false,0)
  await expect(page.locator('#fcitx-theme')).toHaveAttribute('data-tailkana-glass','false')
  expect(await page.locator('.tailkana-active').evaluate(e => getComputedStyle(e).backgroundColor)).toBe('rgb(17, 34, 68)')
  expect(await getCppCalls(page)).toContainEqual({tailkanaAppearance:[5]})
  await show(true,0)
  expect(await getCppCalls(page)).toContainEqual({tailkanaAppearance:[2]})
  expect(await page.locator('.tailkana-active').evaluate(e => getComputedStyle(e).backgroundColor)).toBe('rgba(17, 34, 68, 0)')
  await show(true,100)
  expect(await page.locator('.tailkana-active').evaluate(e => getComputedStyle(e).backgroundColor)).toBe('rgb(17, 34, 68)')
  expect(await page.locator('.tailkana-word').first().evaluate(e => getComputedStyle(e).color)).toBe('rgb(255, 244, 222)')
})


test('corrected candidates show their origin without changing numbered selection', async ({page}) => {
  await updateInputPanel(page,'arigatpu','',`tailkana:${JSON.stringify({...meta,raw:'arigatpu',corrections:{'1':'arigatou'}})}`)
  await setCandidates(page,[{text:'ありがｔぷ',comment:'ありがｔぷ'},{text:'ありがとう',comment:'ありがとう'},{text:'—'}],0)
  const second=page.locator('.tailkana-candidate').nth(1)
  await expect(second.locator('.tailkana-correction-badge')).toHaveText('E')
  await expect(second).toHaveAttribute('title',/arigatpu → arigatou/)
  await expect(page.locator('.tailkana-candidate').first().locator('.tailkana-correction-badge')).toHaveCount(0)
  await second.click()
  expect(await getCppCalls(page)).toContainEqual({select:[1]})
})


test('pinyin-habit candidates have a distinct label and keep the Chinese third slot', async ({page}) => {
  await updateInputPanel(page,'ci','',`tailkana:${JSON.stringify({...meta,raw:'ci',corrections:{'1':'tsu'},pinyinCorrectionSlots:[1],chineseSlots:[2]})}`)
  await setCandidates(page,[{text:'し',comment:'し'},{text:'つ',comment:'つ'},{text:'词',comment:'拼音：ci'}],0)
  const second=page.locator('.tailkana-candidate').nth(1)
  await expect(second.locator('.tailkana-correction-badge')).toHaveText('B')
  await expect(second).toHaveAttribute('title',/模糊拼音：ci → tsu/)
  await expect(page.locator('.tailkana-candidate').nth(2).locator('.tailkana-chinese-badge')).toHaveText('C')
  await second.click()
  expect(await getCppCalls(page)).toContainEqual({select:[1]})
})


test('C E B markers sit below the index and share its narrow column', async ({page}) => {
  await updateInputPanel(page,'ci','',`tailkana:${JSON.stringify({...meta,raw:'ci',corrections:{'0':'shi','1':'tsu'},pinyinCorrectionSlots:[1],chineseSlots:[2]})}`)
  await setCandidates(page,[{text:'市',comment:'し'},{text:'つ',comment:'つ'},{text:'词',comment:'拼音：ci'}],0)
  for(let i=0;i<3;++i) {
    const cell=page.locator('.tailkana-candidate').nth(i)
    const column=cell.locator('.tailkana-index')
    const number=(await column.locator('.tailkana-number').boundingBox())!
    const badge=(await column.locator('.tailkana-correction-badge, .tailkana-chinese-badge').boundingBox())!
    expect(badge.y).toBeGreaterThanOrEqual(number.y+number.height)
    expect((await column.boundingBox())!.width).toBeLessThanOrEqual(12)
    expect(Math.abs((badge.x+badge.width/2)-(number.x+number.width/2))).toBeLessThan(1)
  }
  await page.locator('.fcitx-panel').screenshot({path:test.info().outputPath('index-markers.png')})
})

test('a held click survives a redundant panel refresh without starting a panel drag', async ({page}) => {
  await updateInputPanel(page,'ti','',`tailkana:${JSON.stringify(meta)}`)
  await setCandidates(page,candidates.slice(0,3),0)
  await page.evaluate(() => {const input=document.createElement('input');input.id='mouse-host';document.body.append(input);input.focus();window.cppCalls=[]})
  const target=page.locator('.tailkana-right')
  const box=(await target.boundingBox())!
  await page.mouse.move(box.x+box.width/2,box.y+box.height/2)
  await page.mouse.down()
  await updateInputPanel(page,'ti','',`tailkana:${JSON.stringify(meta)}`)
  await setCandidates(page,candidates.slice(0,3),0)
  await page.mouse.move(box.x+box.width/2+1,box.y+box.height/2+1)
  await page.mouse.up()
  await expect.poll(async () => (await getCppCalls(page)).filter(call=>call.tabAction?.[0]===102).length).toBe(1)
  expect((await getCppCalls(page)).filter(call=>call.resize && (call.resize[1]!==0 || call.resize[2]!==0))).toHaveLength(0)
  expect(await page.evaluate(()=>document.activeElement?.id)).toBe('mouse-host')
})

test('a click on an outdated candidate cannot submit a new composition', async ({page}) => {
  await updateInputPanel(page,'ti','',`tailkana:${JSON.stringify(meta)}`)
  await setCandidates(page,candidates.slice(0,3),0)
  const box=(await page.locator('.tailkana-candidate').nth(1).boundingBox())!
  await page.mouse.move(box.x+box.width/2,box.y+box.height/2)
  await page.mouse.down()
  await updateInputPanel(page,'nihongo','',`tailkana:${JSON.stringify({...meta,raw:'nihongo'})}`)
  await setCandidates(page,[{text:'日本語',comment:'にほんご'}],0)
  await page.mouse.up()
  expect((await getCppCalls(page)).filter(call=>call.select)).toHaveLength(0)
})

test('AI recommendation is independent, compact and has its own action token',async({page})=>{
  const ai={text:'箸',reading:'はし',status:'ready',action:10042,elapsedMs:38}
  await updateInputPanel(page,'hashi','',`tailkana:${JSON.stringify({...meta,raw:'hashi',ai})}`)
  await setCandidates(page,candidates.slice(0,3),0)
  await expect(page.locator('.tailkana-candidate')).toHaveCount(3)
  await expect(page.locator('.tailkana-candidate').first()).toContainText('血')
  await expect(page.getByRole('button',{name:'AI 推荐：箸',exact:true})).toBeVisible()
  const height=(await page.locator('.tailkana-ai').boundingBox())!.height
  expect(height).toBeLessThanOrEqual(26)
  await page.getByRole('button',{name:'AI 推荐：箸',exact:true}).click()
  expect(await getCppCalls(page)).toContainEqual({tabAction:[10042]})
  await updateInputPanel(page,'hash','',`tailkana:${JSON.stringify({...meta,raw:'hash',ai:{...ai,text:'',status:'pending',action:10043}})}`)
  await expect(page.locator('.tailkana-ai')).toBeDisabled()
  await expect(page.locator('.tailkana-ai')).toContainText('推荐中')
})

test('AI click held across a newer recommendation cannot select stale text',async({page})=>{
  const ai={text:'箸',reading:'はし',status:'ready',action:10042,elapsedMs:38}
  await updateInputPanel(page,'hashi','',`tailkana:${JSON.stringify({...meta,raw:'hashi',ai})}`)
  await setCandidates(page,candidates.slice(0,3),0)
  const bounds=(await page.locator('.tailkana-ai').boundingBox())!
  await page.mouse.move(bounds.x+12,bounds.y+8);await page.mouse.down()
  await updateInputPanel(page,'hashi','',`tailkana:${JSON.stringify({...meta,raw:'hashi',ai:{...ai,text:'橋',action:10043}})}`)
  await setCandidates(page,candidates.slice(0,3),0)
  await page.mouse.up()
  expect((await getCppCalls(page)).filter(call=>call.tabAction?.[0]>=10000)).toHaveLength(0)
})

test('compact candidates shrink within a readable range and regain 20px after shortening',async({page})=>{
  const medium=[{text:'日本語文字',comment:'にほんごもじ'},{text:'例',comment:'れい'},{text:'短い',comment:'みじかい'}]
  await updateInputPanel(page,'nihongomoji','',`tailkana:${JSON.stringify({...meta,raw:'nihongomoji',autoExpandAction:-301})}`)
  await setCandidates(page,medium,0)
  await expect.poll(async()=>Number.parseFloat(await page.locator('.tailkana-candidate').first().locator('.tailkana-word').evaluate(e=>getComputedStyle(e).fontSize))).toBeLessThan(20)
  const size=Number.parseFloat(await page.locator('.tailkana-candidate').first().locator('.tailkana-word').evaluate(e=>getComputedStyle(e).fontSize))
  expect(size).toBeGreaterThanOrEqual(16)
  expect((await getCppCalls(page)).some(call=>call.tabAction?.[0]===-301)).toBe(false)
  await updateInputPanel(page,'ni','',`tailkana:${JSON.stringify({...meta,raw:'ni'})}`)
  await setCandidates(page,[{text:'日',comment:'に'},{text:'に'},{text:'二',comment:'に'}],0)
  await expect.poll(async()=>page.locator('.tailkana-candidate').first().locator('.tailkana-word').evaluate(e=>getComputedStyle(e).fontSize)).toBe('20px')
})

test('only automatically expanded lists fold when their actual compact page fits',async({page})=>{
  const long={text:'日本語を勉強してから映画を見に行きます',comment:'にほんごをべんきょうしてからえいがをみにいきます'}
  const short=[{text:'日',comment:'に'},{text:'に'},{text:'二',comment:'に'}]
  await updateInputPanel(page,'nihongo','',`tailkana:${JSON.stringify({...meta,raw:'nihongo',expanded:true,autoExpanded:true,compactIndices:[0,1,2],autoCollapseAction:-302})}`)
  await setCandidates(page,[...short,long],0)
  await expect.poll(async()=>(await getCppCalls(page)).some(call=>call.tabAction?.[0]===-302)).toBe(true)
  await page.evaluate(()=>{window.cppCalls=[]})
  // Selected compact page, rather than a fixed first three, controls folding.
  await updateInputPanel(page,'nihongo','',`tailkana:${JSON.stringify({...meta,raw:'nihongo',expanded:true,autoExpanded:true,compactIndices:[3,4,5],autoCollapseAction:-304})}`)
  await setCandidates(page,[...short,long,long,long],3)
  await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))))
  expect((await getCppCalls(page)).some(call=>call.tabAction?.[0]===-304)).toBe(false)
  await updateInputPanel(page,'ni','',`tailkana:${JSON.stringify({...meta,raw:'ni',expanded:true,autoExpanded:false,compactIndices:[0,1,2],autoCollapseAction:-306})}`)
  await setCandidates(page,short,0)
  await page.evaluate(()=>new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve))))
  expect((await getCppCalls(page)).some(call=>call.tabAction?.[0]===-306)).toBe(false)
  await expect(page.locator('.tailkana-more-candidate')).toHaveCount(3)
})


test('custom width changes the whole frame and its fit probe while keeping the wheel fixed',async({page})=>{
  const sample=[{text:'日本語文字',comment:'にほんごもじ'},{text:'例',comment:'れい'},{text:'短い',comment:'みじかい'}]
  for(const width of [360,720,1000]) {
    await updateInputPanel(page,'nihongomoji','',`tailkana:${JSON.stringify({...meta,raw:'nihongomoji',panelWidth:width})}`)
    await setCandidates(page,sample,0)
    await expect.poll(async()=>Math.round((await page.locator('.tailkana').boundingBox())!.width)).toBe(width)
    expect((await page.locator('.tailkana-cross').boundingBox())!.width).toBeLessThanOrEqual(127)
    if(width===720) await expect.poll(async()=>page.locator('.tailkana-candidate').first().locator('.tailkana-word').evaluate(e=>getComputedStyle(e).fontSize)).toBe('20px')
  }
  await updateInputPanel(page,'nihongomoji','',`tailkana:${JSON.stringify({...meta,raw:'nihongomoji',panelWidth:720,expanded:true,autoExpanded:true,compactIndices:[0,1,2],autoCollapseAction:-402})}`)
  await setCandidates(page,sample,0)
  await expect.poll(async()=>(await getCppCalls(page)).some(call=>call.tabAction?.[0]===-402)).toBe(true)
  expect((await page.locator('.tailkana-compact-probe').boundingBox())!.width).toBeCloseTo((await page.locator('.tailkana-main').boundingBox())!.width,0)
})
