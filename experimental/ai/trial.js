// SPDX-License-Identifier: GPL-3.0-or-later
const token=location.hash.slice(1)||sessionStorage.getItem('tailkana-trial-token')||'';
if(token)sessionStorage.setItem('tailkana-trial-token',token);
history.replaceState(null,'',location.pathname);
const $=id=>document.getElementById(id);
let controlsBusy=false,searchBusy=false,searchRevision=0,composing=false,stopped=false;
let liveSamples=[],benchRun=0,benchActive=false,lastPaint=null;
function status(id,text,error=false){$(id).textContent=text;$(id).classList.toggle('error',error)}
async function api(operation,data){
  const response=await fetch('/api/'+operation,{method:'POST',headers:{'Content-Type':'application/json','X-TailKana-Token':token},body:JSON.stringify(data)});
  const value=await response.json();if(!response.ok)throw Error(value.error||'暂时无法完成');return value;
}
function append(text){$('output').textContent+=text}
function element(tag,text,cls){const el=document.createElement(tag);el.textContent=text;if(cls)el.className=cls;return el}
function choice(item,index,recommended){
  const row=element('div','','candidate'+(recommended&&index===0?' recommended':''));
  row.append(element('span',String(index+1),'number'),element('span',item.text,'word'));
  const button=element('button','选用');button.onclick=()=>append(item.text);row.append(button);return row;
}
function setCandidatePending(value){
  $('rank-result').classList.toggle('pending',value);
  $('rank-result').querySelectorAll('button').forEach(b=>b.disabled=value);
}
function paintFrame(){return new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))}
function updateControls(){
  const active=controlsBusy||searchBusy||live.active;
  $('unload').disabled=active||stopped;$('shutdown').disabled=controlsBusy||stopped;
  $('refresh').disabled=controlsBusy||searchBusy||stopped;
  $('search-submit').disabled=controlsBusy||searchBusy||stopped;
  $('benchmark').disabled=active||benchActive||stopped;
}
const live=new TailKanaLatestRequest({
  delay:16,
  run:data=>api('rank',data),
  onState:()=>updateControls(),
  onError:error=>{status('rank-status',error.message,true);$('rank-result').hidden=true},
  onResult:async(data,timing)=>{
    $('original').replaceChildren(...data.candidates.slice(0,8).map((c,i)=>choice(c,i,false)));
    $('ranked').replaceChildren(...data.ranked.slice(0,8).map((c,i)=>choice(c,i,true)));
    $('rank-result').hidden=!data.candidates.length;setCandidatePending(false);
    status('rank-status',data.ranked.length?'正在显示最新推荐…':(data.skipped||'没有匹配的日语候选'));
    await paintFrame();
    // A key may arrive between the two animation frames. Don't publish obsolete
    // timing or re-enable obsolete rows after that key invalidates this result.
    if(timing.revision!==live.revision)return;
    const paintedAt=performance.now();
    const sample={revision:timing.revision,total_ms:Math.round(paintedAt-timing.inputAt),request_ms:Math.round(timing.responseAt-timing.requestAt),wait_ms:Math.round(timing.requestAt-timing.inputAt),paint_ms:Math.round(paintedAt-timing.responseAt),model_ms:data.model_ms||0,mozc_ms:data.mozc_ms||0,queue_ms:data.queue_ms||0,cold:!!data.cold_start,scored:!!data.ranked.length,discarded:timing.discarded};
    lastPaint={...sample,roman:timing.data.roman,context:timing.data.context};
    liveSamples.push(sample);liveSamples=liveSamples.slice(-60);
    const scored=liveSamples.filter(s=>s.scored&&!s.cold).map(s=>s.total_ms).sort((a,b)=>a-b);
    const p95=scored.length?scored[Math.ceil(scored.length*.95)-1]:null;
    $('realtime-metrics').textContent=JSON.stringify(sample);
    $('latency').textContent=p95===null?'':`最近 ${scored.length} 次热运行：P95 ${p95} ms`;
    status('rank-status',`读音：${data.reading} · 输入→显示 ${sample.total_ms} ms · 本机 ${data.elapsed_ms} ms${sample.cold?' · 首次加载':''}${data.skipped?' · '+data.skipped:''}`);
  }
});
function rankChanged({immediate=false}={}){
  setCandidatePending(true);
  if(composing||controlsBusy||searchBusy||stopped||$('rank-panel').hidden){live.cancel();return}
  const roman=$('roman').value.trim();
  if(!roman){live.cancel();$('rank-result').hidden=true;status('rank-status','输入罗马字后实时推荐');return}
  if(!/^[a-z'-]{1,96}$/.test(roman)){live.cancel();$('rank-result').hidden=true;status('rank-status','请使用小写罗马字母',true);return}
  status('rank-status','正在更新…');
  live.update({context:$('context').value,roman},{immediate});
}
for(const tab of ['rank','search'])$('tab-'+tab).onclick=()=>{
  for(const t of ['rank','search']){$(t+'-panel').hidden=t!==tab;$('tab-'+t).setAttribute('aria-selected',String(t===tab))}
  if(tab==='rank')rankChanged();else{live.cancel();++benchRun;benchActive=false;updateControls()}
};
$('rank-form').onsubmit=e=>{e.preventDefault();rankChanged({immediate:true})};
for(const id of ['context','roman']){
  $(id).addEventListener('compositionstart',()=>{composing=true;live.cancel();setCandidatePending(true)});
  $(id).addEventListener('compositionend',()=>{composing=false;rankChanged()});
  $(id).addEventListener('input',e=>{
    if(e.isTrusted&&benchActive){++benchRun;benchActive=false;status('bench-status','测试已因手动输入中止')}
    if(!e.isComposing)rankChanged();
  });
}
$('search-form').onsubmit=async e=>{
  e.preventDefault();if(controlsBusy||searchBusy||stopped)return;
  live.cancel();searchBusy=true;updateControls();const own=++searchRevision;
  $('search-result').replaceChildren();status('search-status','正在本机查找…');
  try{
    const data=await api('search',{query:$('query').value});if(own!==searchRevision)return;
    for(const item of data.results){
      const row=element('div','','entry'),body=element('div',''),title=element('div','');
      title.append(element('span',item.word,'word'),element('span',item.reading,'reading'));
      body.append(title,element('div',item.meaning,'meaning'));
      const button=element('button','选用');button.onclick=()=>append(item.word);row.append(body,button);$('search-result').append(row);
    }
    status('search-status',`${data.results.length} 条相近词 · ${data.elapsed_ms} ms（含本次所需加载）`);
  }catch(error){if(own===searchRevision)status('search-status',error.message,true)}
  finally{searchBusy=false;updateControls();if(!$('rank-panel').hidden)rankChanged()}
};
for(const button of document.querySelectorAll('[data-example]'))button.onclick=()=>{
  const examples={chopsticks:['ご飯を食べるために','hashi'],bridge:['川を渡るために','hashi'],return:['宇宙船が地球に','kikan']};
  [$('context').value,$('roman').value]=examples[button.dataset.example];rankChanged();
};
for(const button of document.querySelectorAll('[data-query]'))button.onclick=()=>{$('query').value=button.dataset.query;++searchRevision;$('search-result').replaceChildren()};
$('query').addEventListener('input',()=>{++searchRevision;$('search-result').replaceChildren();status('search-status','输入已更改，请重新查找')});
$('unload').onclick=async()=>{
  if(controlsBusy||searchBusy||live.active)return;
  live.cancel();controlsBusy=true;updateControls();
  try{await api('unload',{});status('rank-status','模型已释放，下次输入会重新加载');status('search-status','模型已释放')}
  catch(error){status('rank-status',error.message,true)}finally{controlsBusy=false;updateControls()}
};
$('shutdown').onclick=async()=>{
  live.cancel();++benchRun;controlsBusy=true;updateControls();
  try{await api('shutdown',{});stopped=true;status('rank-status','试验已结束，可关闭本页');status('search-status','试验已结束，可关闭本页')}
  catch(error){status('rank-status',error.message,true);controlsBusy=false;updateControls()}
};

// Controlled page input events measure this exact live pipeline; no IMK or
// physical-keyboard latency is claimed. Only canned benchmark values are used.
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function benchSet(id,value){$(id).value=value;$(id).dispatchEvent(new InputEvent('input',{bubbles:true,inputType:'insertText'}))}
async function waitForPaint(roman,context,run){
  const deadline=performance.now()+30000;
  while(performance.now()<deadline){
    if(run!==benchRun)throw Error('测试已中止');
    if(!live.active&&lastPaint?.roman===roman&&lastPaint?.context===context)return lastPaint;
    await delay(10);
  }
  throw Error('测试等待超时');
}
$('benchmark').onclick=async()=>{
  if(controlsBusy||searchBusy||live.active||benchActive||stopped)return;
  const saved={context:$('context').value,roman:$('roman').value};
  const run=++benchRun;benchActive=true;updateControls();const report={definition:'page input event to two requestAnimationFrame callbacks; includes 16 ms coalescing, local HTTP, Mozc, Zenz and DOM; excludes hardware keyboard/IMK',cases:[]};
  const cases=[
    {name:'首次加载',context:'ご飯を食べるために',roman:'hashi',cold:true},
    {name:'正常打字 100ms/字母',context:'川を渡るために',roman:'hashi',interval:100},
    {name:'快速打字 50ms/字母',context:'宇宙船が地球に',roman:'kikan',interval:50},
    {name:'长句 80ms/字母',context:'昨日友達と図書館に行って',roman:'watasihanihongowobenkyoushiteimasu',interval:80},
    {name:'长前文与长句 50ms/字母',context:'昨日はとても天気が良かったので友達と一緒に近くの川沿いの道を歩いてから',roman:'ashitahatoukyounoataraishiikaishadehatarakimasu',interval:50},
    {name:'删除与重输',context:'ご飯を食べるために',roman:'hashi',editing:true},
    {name:'仅修改前文',context:'川を渡るために',roman:'hashi',contextOnly:true}
  ];
  try{
    for(const item of cases){
      status('bench-status','测试中：'+item.name);const sampleStart=live.revision;
      if(item.cold){live.cancel();await api('unload',{})}
      if(run!==benchRun)throw Error('测试已中止');
      if(item.contextOnly){benchSet('context',item.context)}
      else{
        benchSet('roman','');benchSet('context',item.context);
        if(item.interval){
          for(let i=1;i<=item.roman.length;i++){if(run!==benchRun)throw Error('测试已中止');benchSet('roman',item.roman.slice(0,i));await delay(item.interval)}
        }else if(item.editing){
          for(const text of ['hashi','hash','has','ha','h','ha','has','hash','hashi']){benchSet('roman',text);await delay(55)}
        }else benchSet('roman',item.roman);
      }
      const final=await waitForPaint(item.roman,item.context,run);
      const samples=liveSamples.filter(s=>s.revision>sampleStart).map(s=>({...s}));
      report.cases.push({name:item.name,final:(({roman,context,...rest})=>rest)(final),samples});
    }
    $('bench-report').textContent=JSON.stringify(report,null,2);
    const warm=report.cases.filter(x=>!x.final.cold).map(x=>x.final.total_ms).sort((a,b)=>a-b);
    status('bench-status',`测试完成：最后一字→推荐显示，中位数 ${Math.round((warm[Math.floor((warm.length-1)/2)]+warm[Math.floor(warm.length/2)])/2)} ms，最慢 ${Math.max(...warm)} ms；首次加载 ${report.cases[0].final.total_ms} ms`);
  }catch(error){status('bench-status',error.message,true)}
  finally{
    benchActive=false;updateControls();
    if(run===benchRun){$('context').value=saved.context;$('roman').value=saved.roman;rankChanged()}
  }
};
if(!token){stopped=true;status('rank-status','此页面已失效，请重新打开 AI 试验入口',true);updateControls()}
else rankChanged();
