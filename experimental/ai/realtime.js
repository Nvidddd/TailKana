// SPDX-License-Identifier: GPL-3.0-or-later
// One running request and one replaceable latest input. Old replies never paint.
(function(root){
class LatestRequest {
  constructor({run,onResult,onError=()=>{},onState=()=>{},delay=16,now=()=>performance.now()}) {
    Object.assign(this,{run,onResult,onError,onState,delay,now});
    this.revision=0;this.pending=null;this.running=false;this.timer=null;this.discarded=0;
  }
  update(data,{immediate=false}={}) {
    const inputAt=this.now();
    this.pending={data,revision:++this.revision,inputAt,dueAt:inputAt+(immediate?0:this.delay)};
    this.schedule();return this.revision;
  }
  cancel(){++this.revision;this.pending=null;clearTimeout(this.timer);this.timer=null;this.onState(this.active)}
  get active(){return this.running||!!this.pending}
  schedule(){
    clearTimeout(this.timer);this.timer=null;this.onState(this.active);
    if(this.running||!this.pending)return;
    this.timer=setTimeout(()=>this.pump(),Math.max(0,this.pending.dueAt-this.now()));
  }
  async pump(){
    this.timer=null;if(this.running||!this.pending)return;
    if(this.pending.dueAt>this.now()){this.schedule();return}
    const item=this.pending;this.pending=null;this.running=true;this.onState(true);
    const requestAt=this.now();
    try{
      const result=await this.run(item.data);
      const responseAt=this.now();
      if(item.revision===this.revision)await this.onResult(result,{...item,requestAt,responseAt,discarded:this.discarded});
      else ++this.discarded;
    }catch(error){if(item.revision===this.revision)this.onError(error)}
    finally{this.running=false;this.schedule()}
  }
}
if(typeof module!=='undefined'&&module.exports)module.exports={LatestRequest};
else root.TailKanaLatestRequest=LatestRequest;
})(globalThis);
