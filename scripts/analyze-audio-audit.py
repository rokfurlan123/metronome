"""Analyze a WASAPI loopback capture; requires NumPy in an existing Linux environment."""
import csv, json, sys
from pathlib import Path
import numpy as np
base=Path(sys.argv[1] if len(sys.argv)>1 else 'build/windows-audio-audit')
rate,channels=map(int,Path(str(base)+'.format.txt').read_text().split())
raw=np.fromfile(str(base)+'.f32',dtype='<f4').reshape(-1,channels)
channel=int(np.argmax(np.mean(raw.astype(np.float64)**2,axis=0)))
results=[]
for row in csv.DictReader(open(str(base)+'.markers.csv')):
    x=raw[int(row['start_frame']):int(row['end_frame']),channel].astype(np.float64)
    sub=int(row['subdivision']); poly=int(row['poly'])
    period=60/int(row['bpm'])/sub
    # Energy envelope finds complete click peaks, not individual waveform cycles.
    width=max(1,round(rate*.003))
    cumulative=np.r_[0.,np.cumsum(x*x)]
    envelope=(cumulative[width:]-cumulative[:-width])/width
    work=envelope.copy(); margin=round(rate*.13)
    work[:margin]=0; work[-margin:]=0
    spacing=round(rate*(.065 if poly else period*.45))
    threshold=float(work.max())*.08
    peaks=[]
    while work.max()>threshold and len(peaks)<256:
        index=int(work.argmax()); peaks.append(index+width//2)
        work[max(0,index-spacing):min(len(work),index+spacing)]=0
    peaks.sort()
    # A limited waveform can have tied envelope maxima a half-cycle apart.
    # Align to its leading edge instead, so peak selection cannot invent jitter.
    threshold_amplitude=float(np.max(np.abs(x)))*.02
    onsets=[]
    for peak in peaks:
        start=max(0,peak-round(rate*.010))
        above=np.flatnonzero(np.abs(x[start:peak+1])>threshold_amplitude)
        if len(above): onsets.append(start+int(above[0]))
    peaks=sorted(set(onsets))
    if len(peaks)<5: raise RuntimeError(f"Insufficient audible clicks in {row['case']}")
    energies=[]; sample_peaks=[]; windows=[]
    pre=round(rate*.006)
    length=round(rate*min(.115,period-.012))
    for peak in peaks:
        segment=x[peak-pre:peak-pre+length]
        energies.append(float(segment@segment)); sample_peaks.append(float(np.max(np.abs(segment))))
        windows.append(segment)
    energy=np.array(energies)
    spread=float(10*np.log10(energy.max()/energy.min()))
    intervals=np.diff(peaks)/rate
    correlations=[float(w@windows[0]/np.sqrt((w@w)*(windows[0]@windows[0]))) for w in windows]
    result={**row,'detected_clicks':len(peaks),'energy_spread_db':spread,'peak_spread_db':float(20*np.log10(max(sample_peaks)/min(sample_peaks))), 'median_interval_ms':float(np.median(intervals)*1000),'interval_range_ms':[float(intervals.min()*1000),float(intervals.max()*1000)],'minimum_peak_aligned_correlation':min(correlations),'click_energy':energies}
    results.append(result)
    print(f"{row['case']}: {len(peaks)} clicks, energy spread {spread:.6f} dB, peak spread {result['peak_spread_db']:.6f} dB, interval {result['median_interval_ms']:.3f} ms")
Path(str(base)+'.analysis.json').write_text(json.dumps({'rate':rate,'channels':channels,'selected_channel':channel,'cases':results},indent=2)+'\n')
if '--expect-flat' in sys.argv:
    failures=[]
    for result in results:
        if int(result['accents'])==0:
            if result['energy_spread_db']>.05 or result['peak_spread_db']>.05:
                failures.append(result['case']+': unintended level variation')
            if result['minimum_peak_aligned_correlation']<.999:
                failures.append(result['case']+': differing click waveform')
            if int(result['poly'])==0:
                expected=60000/int(result['bpm'])/int(result['subdivision'])
                if max(abs(t-expected) for t in result['interval_range_ms'])>1000/rate+.0001:
                    failures.append(result['case']+': irregular onset timing')
        elif result['energy_spread_db']<.5:
            failures.append(result['case']+': positive accent control not detected')
    if failures:
        raise SystemExit('FAIL: '+'; '.join(failures))
    print('PASS: unaccented levels/waveforms/timing and positive accent controls')
