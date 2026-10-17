# SPDX-License-Identifier: GPL-3.0-or-later
import json
import resource
import statistics
import subprocess
import time
from models import DATA
from server import Trial

trial=Trial()
report={'models': json.loads((DATA/'models/manifest.json').read_text()), 'rank': [], 'search': []}
def rss(pid):
    return int(subprocess.check_output(['ps','-o','rss=','-p',str(pid)], text=True).strip())*1024
try:
    for context, roman in [('ご飯を食べるために','hashi'),('川を渡るために','hashi'),('宇宙船が地球に','kikan'),('使用できる','kikan'),('胃や腸などの','kikan'),('学校で','nihongo')]:
        value=trial.run('rank',dict(context=context,roman=roman))
        report['rank'].append(dict(context=context,roman=roman,elapsed_ms=value['elapsed_ms'],original=value['candidates'],ranked=value['ranked']))
        print(context,roman,value['ranked'][:2],value['elapsed_ms'],'ms',flush=True)
    for query in ['下雨时用来遮雨的东西','借书和看书的地方','忘记','筷子','不小心把东西弄丢了']:
        value=trial.run('search',dict(query=query))
        report['search'].append(dict(query=query,elapsed_ms=value['elapsed_ms'],results=value['results']))
        print(query,[(x['word'], x['reading']) for x in value['results'][:5]], value['elapsed_ms'],'ms',flush=True)
    report['rank_warm_median_ms']=statistics.median(x['elapsed_ms'] for x in report['rank'][1:])
    report['search_warm_median_ms']=statistics.median(x['elapsed_ms'] for x in report['search'][1:])
    import os
    report['rss_loaded_bytes']={'service':rss(os.getpid()),'zenz':rss(trial.zenz.process.pid),'mozc':rss(trial.helper.pid),'semantic':rss(trial.semantic.process.pid)}
    trial.run('unload',{})
    time.sleep(1)
    report['service_rss_after_unload_bytes']=rss(os.getpid())
    report['model_child_peak_rss_bytes']=resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
finally:
    trial.closed=True
    trial.pool.shutdown()
    trial.unload()
(DATA/'benchmark.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('models','rank','search')},indent=2))
