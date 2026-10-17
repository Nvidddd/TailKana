// SPDX-License-Identifier: GPL-3.0-or-later
const assert=require('node:assert/strict');
const {LatestRequest}=require('./realtime.js');
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
(async()=>{
 const requests=[],painted=[];let active=0,maximum=0;
 const gate=new LatestRequest({delay:0,run:data=>{active++;maximum=Math.max(maximum,active);return new Promise(resolve=>requests.push({data,resolve:value=>{active--;resolve(value)}}))},onResult:(data)=>painted.push(data)});
 gate.update('h');await sleep(5);
 gate.update('ha');gate.update('has');gate.update('hash');gate.update('hashi');
 assert.equal(requests.length,1,'typing must not queue per-key HTTP jobs');
 requests[0].resolve('obsolete h');await sleep(5);
 assert.deepEqual(painted,[],'obsolete replies must not paint');
 assert.equal(requests.length,2);assert.equal(requests[1].data,'hashi');
 requests[1].resolve('箸');await sleep(5);
 assert.deepEqual(painted,['箸']);assert.equal(maximum,1);
 gate.update('changed');await sleep(5);gate.cancel();requests[2].resolve('stale after clear');await sleep(5);
 assert.deepEqual(painted,['箸'],'clear, composition and navigation must invalidate replies');
 const errors=[];const retry=new LatestRequest({delay:0,run:async data=>{if(data==='bad')throw Error('model failed');return data},onResult:x=>painted.push(x),onError:e=>errors.push(e.message)});
 retry.update('bad');await sleep(5);retry.update('recovered');await sleep(5);
 assert.deepEqual(errors,['model failed']);assert.equal(painted.at(-1),'recovered');
 let hits=0;const debounce=new LatestRequest({delay:20,run:async()=>++hits,onResult:()=>{}});
 debounce.update('a');debounce.update('b');debounce.update('c');await sleep(40);assert.equal(hits,1);
 console.log('PASS: latest input, one request, stale replies, cancellation, errors, short input coalescing');
})().catch(error=>{console.error(error);process.exitCode=1});
