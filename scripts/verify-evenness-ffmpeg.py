"""Cross-check whole-click RMS/peak with FFmpeg astats (separate DSP code)."""
import argparse,json,subprocess,math
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--expect-flat',action='store_true');a=p.parse_args()
analysis=json.loads((a.folder/'analysis.json').read_text());results=[];failures=[]
for case in analysis['cases']:
    base=a.folder/case['case'];metadata=Path(str(base)+'.ffmpeg-astats.txt')
    subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-f','f32le','-ar',str(case['rate']),'-ac','1','-i',str(base)+'.cells.f32','-af',f'asetnsamples=n={case["rate"]//10+2}:p=0,astats=metadata=1:reset=1,ametadata=print:file={metadata}','-f','null','-'],check=True)
    values={'RMS_level':[],'Peak_level':[],'Number_of_NaNs':[],'Number_of_Infs':[]}
    for line in metadata.read_text().splitlines():
        for name in values:
            prefixes=['lavfi.astats.Overall.'+name+'=']
            # FFmpeg 6.x emits these two aggregate keys with spaces and no
            # Overall prefix, unlike the RMS/peak keys. Require actual data.
            if name in ['Number_of_NaNs','Number_of_Infs']:
                prefixes.append('lavfi.astats.'+name.replace('_',' ')+'=')
            if any(line.startswith(prefix) for prefix in prefixes):values[name].append(float(line.split('=',1)[1]))
    r={'case':case['case'],'clicks':len(values['RMS_level']),
       'rms_spread_db':max(values['RMS_level'])-min(values['RMS_level']),
       'peak_spread_db':max(values['Peak_level'])-min(values['Peak_level']),
       'nan_count':sum(values['Number_of_NaNs']),'inf_count':sum(values['Number_of_Infs'])}
    results.append(r);print(r,flush=True)
    if r['nan_count'] or r['inf_count'] or any(len(v)!=case['clicks'] or not all(math.isfinite(x) for x in v) for v in values.values()):failures.append(r['case']+': invalid samples/count or unavailable metadata')
    if 'positive_control' in r['case']:
        if r['rms_spread_db']<.5:failures.append(r['case']+': missed positive control')
    elif r['rms_spread_db']>.05 or r['peak_spread_db']>.05:failures.append(r['case']+': uneven RMS/peak')
(a.folder/'ffmpeg-analysis.json').write_text(json.dumps({'cases':results,'failures':failures},indent=2)+'\n')
if failures:
    print('FAILURES: '+'; '.join(failures))
    if a.expect_flat:raise SystemExit(1)
else:print('PASS FFmpeg per-click RMS/peak/count/finite checks')
