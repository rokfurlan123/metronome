"""Whole-click level/waveform/spectral audit. Digital measurements, not acoustic loudness.
Offline onsets are the actual scheduler's markers; native captures use the same
scheduler grid aligned to the first click, not tied envelope maxima.
"""
import argparse, csv, json, subprocess
from pathlib import Path
import numpy as np
from scipy import signal
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt


def read(base):
    rate, channels = map(int, Path(str(base)+'.format.txt').read_text().split())
    raw = np.fromfile(str(base)+'.f32', dtype='<f4').reshape(-1, channels).astype(np.float64)
    if not np.isfinite(raw).all(): raise ValueError(f'Nonfinite audio: {base}')
    channel = int(np.argmax(np.sum(raw*raw, axis=0)))
    return rate, raw[:, channel], raw


def native_alignment(x, expected, rate, phase_span=0):
    first=int(np.flatnonzero(np.abs(x)>np.max(np.abs(x))*.001)[0])
    # Align the *whole grid*, not a single repeated carrier/envelope peak.
    # Native captures sometimes begin with a partial initial render
    # packet; its cause is not established here. Align using an interior window,
    # then report leading partial clicks rather than normalizing each pulse.
    start=rate;length=min(2*rate,len(x)-start-rate//4)
    upper=start+phase_span
    corr=signal.correlate(expected[:upper+length],x[start:start+length],mode='valid',method='fft')
    lower=max(0,start-first)
    position=lower+int(np.argmax(corr[lower:upper+1]))
    return start-position,first


def metrics(x, onsets, rate, accented=False, measurement_end=None):
    # Include the first click and complete tails. Zero-pad short cells to one
    # common 100 ms analysis window; never hide unequal tails in a short prefix.
    length = rate//10+2
    rows=[]; cells=[]
    for i, onset in enumerate(onsets):
        if onset < 0 or onset+length > len(x) or (measurement_end is not None and onset+length>measurement_end): continue
        end = min(onset+length, onsets[i+1] if i+1<len(onsets) else len(x))
        cell = np.zeros(length); cell[:end-onset] = x[onset:end]
        energy=float(cell@cell); peak=float(np.max(np.abs(cell)))
        if energy<=0: raise ValueError('Expected click is silent/missing')
        fft=np.abs(np.fft.rfft(cell))
        freq=np.fft.rfftfreq(length,1/rate)
        centroid=float(np.sum(freq*fft**2)/np.sum(fft**2))
        rows.append({'frame':int(onset),'energy':energy,'peak':peak,'rms':float(np.sqrt(energy/length)),'centroid_hz':centroid})
        cells.append(cell)
    if len(rows)<5: raise ValueError('Insufficient complete measured clicks')
    cells=np.array(cells); energies=np.array([r['energy'] for r in rows]); peaks=np.array([r['peak'] for r in rows])
    ref=cells[0];ref_fft=np.abs(np.fft.rfft(ref));ffts=np.abs(np.fft.rfft(cells,axis=1))
    correlations=(cells@ref)/np.sqrt(energies*(ref@ref))
    # Raw spectral L2 error catches timbre/tail changes without renormalizing levels.
    spectral=np.linalg.norm(ffts-ref_fft,axis=1)/np.linalg.norm(ref_fft)
    intervals=np.diff([row['frame'] for row in rows])
    return {'clicks':len(rows),'energy_spread_db':float(10*np.log10(energies.max()/energies.min())),
            'peak_spread_db':float(20*np.log10(peaks.max()/peaks.min())),
            'max_sample_difference':float(np.max(np.abs(cells-ref))),
            'minimum_correlation':float(correlations.min()),'max_relative_spectral_error':float(spectral.max()),
            'centroid_spread_hz':float(np.ptp([r['centroid_hz'] for r in rows])),
            'interval_range_frames':[int(intervals.min()),int(intervals.max())],
            'per_click':rows},cells


def plot(base,x,onsets,rate,result,cells):
    fig, axes=plt.subplots(2,2,figsize=(13,8),constrained_layout=True)
    show=min(len(x),rate*3);axes[0,0].plot(np.arange(show)/rate,x[:show],lw=.5);axes[0,0].set(title='Rendered/captured waveform (no gain normalization)',xlabel='Seconds',ylabel='Amplitude')
    energies=np.array([r['energy'] for r in result['per_click']]);axes[0,1].plot(10*np.log10(energies/np.median(energies)),'.-',lw=.7);axes[0,1].set(title=f"Every click incl. first: spread {result['energy_spread_db']:.6f} dB",xlabel='Click number',ylabel='Energy relative to median (dB)')
    for cell in cells[:min(len(cells),80)]:axes[1,0].plot(np.arange(len(cell))*1000/rate,cell,lw=.4,alpha=.3)
    axes[1,0].set(title='Onset-aligned whole clicks (incl. tail, no rescaling)',xlabel='Milliseconds',ylabel='Amplitude')
    stft=signal.ShortTimeFFT.from_window('hann',rate,256,192,scale_to='psd')
    power=stft.spectrogram(x[:show]);extent=stft.extent(show)
    im=axes[1,1].imshow(10*np.log10(np.maximum(power,1e-16)),origin='lower',aspect='auto',extent=extent,vmin=-100,vmax=-20,cmap='magma')
    axes[1,1].set(ylim=(0,7000),title='Spectrogram (fixed dB scale)',xlabel='Seconds',ylabel='Hz');fig.colorbar(im,ax=axes[1,1],label='dB/Hz')
    fig.suptitle(base.name+' — digital audit; not a speaker/microphone measurement',fontsize=12)
    fig.savefig(str(base)+'.png',dpi=140);plt.close(fig)


def main():
    p=argparse.ArgumentParser();p.add_argument('folder',type=Path);p.add_argument('--native',action='store_true');p.add_argument('--renderer',type=Path);p.add_argument('--expect-flat',action='store_true');args=p.parse_args()
    results=[];failures=[]
    if args.native:
        segments=args.folder/'segments.csv'
        if segments.exists():
            rate,_,live=read(args.folder/'live')
            status=next(csv.DictReader((args.folder/'live-status.csv').open()))
            fields=['case','bpm','beats','unit','subdivision','poly','sound','volume','accents','frames','discontinuities','position_gaps','timestamp_errors','device_position_available','phase_unanchored']
            with (args.folder/'capture.csv').open('w') as out:
                writer=csv.DictWriter(out,fieldnames=fields);writer.writeheader()
                for row in csv.DictReader(segments.open()):
                    part=live[int(row['start_frame']):int(row['end_frame'])];base=args.folder/row['case'];part.astype('<f4').tofile(str(base)+'.f32');Path(str(base)+'.format.txt').write_text(f'{rate} {live.shape[1]}\n')
                    writer.writerow({**{k:row[k] for k in fields if k in row},**status,'frames':len(part),'phase_unanchored':1})
        cases=list(csv.DictReader((args.folder/'capture.csv').open()))
    else:
        cases=[{'case':b.name.removesuffix('.format.txt')} for b in sorted(args.folder.glob('*.format.txt'))]
    for case in cases:
        base=args.folder/case['case'];rate,x,raw=read(base);offset=0;analysis_end=None
        if args.native:
            refbase=base.with_name(base.name+'-reference')
            subprocess.run([str(args.renderer.resolve()),'--render',str(refbase),*[case[k] for k in ['bpm','beats','unit','subdivision','poly','sound','volume','accents']],str(len(x)/rate+1)],check=True)
            _,expected,_=read(refbase)
            phase_span=round(rate*60*4*int(case['beats'])/(int(case['bpm'])*int(case['unit']))) if case.get('phase_unanchored')=='1' else 0
            offset,actual_first=native_alignment(x,expected,rate,phase_span)
            markerbase=refbase
        else: markerbase=base;seed=0
        markers=list(csv.DictReader(Path(str(markerbase)+'.clicks.csv').open()))
        onsets=sorted(set(int(r['frame']) for r in markers if r['silent']=='0'))
        if args.native:
            onsets=[n+offset for n in onsets if n+offset<len(x)]
            leading_count=0
            expected_rise=int(np.flatnonzero(np.abs(expected[:rate//10])>np.max(np.abs(expected[:rate//10]))*.001)[0])
            while onsets and actual_first-(onsets[0]+expected_rise)>4:
                onsets.pop(0);leading_count+=1
            onset_corrections=[]
            if case.get('phase_unanchored')=='1':
                # A live retime leaves a fractional bar anchor. Rendering a
                # fresh reference at frame zero can differ by one rounded
                # sample. Locate the exact zero-to-attack edge within ±1;
                # record every correction, never rescale or stretch a click.
                refined=[]
                for n in onsets:
                    edges=[q for q in range(max(1,n-1),min(len(x)-1,n+2)) if x[q]==0 and x[q+1]>0]
                    q=min(edges,key=lambda q:abs(q-n)) if edges else n
                    refined.append(q);onset_corrections.append(q-n)
                onsets=refined
            # Ignore scheduler events after the GUI stopped its own stream, not
            # any audible startup or middle clicks. Silence must be >100 ms.
            last=int(np.flatnonzero(np.abs(x)>np.max(np.abs(x))*.001)[-1])
            # Stop can intentionally cut an in-flight click. Keep the first
            # onset, but exclude the final 100 ms stop boundary, not a varying
            # fraction of each click. Interior cells still use their full tail.
            analysis_end=last+1
        control='positive_control' in base.name
        result,cells=metrics(x,onsets,rate,control,analysis_end)
        cells.astype('<f4').tofile(str(base)+'.cells.f32')
        result.update(case=base.name,rate=rate,alignment_offset_frames=offset,
                      stereo_max_difference=float(np.max(np.abs(raw[:,0]-raw[:,-1]))))
        if result['stereo_max_difference']>1e-7:
            failures.append(base.name+': left/right channels differ')
        if args.native:
            result['leading_capture_partial_clicks_excluded']=leading_count
            result['live_fractional_anchor_onset_corrections']=onset_corrections
            position_available=case.get('device_position_available')=='1'
            result['capture_counters']={'discontinuities':int(case['discontinuities']),
                'timestamp_errors':int(case['timestamp_errors']),
                'position_gaps':int(case['position_gaps']) if position_available else None,
                'device_position_available':position_available}
            if int(case['discontinuities']) or int(case['timestamp_errors']) or (position_available and int(case['position_gaps'])):
                failures.append(base.name+': capture discontinuity/timestamp error')
        results.append(result)
        print(f"{base.name}: clicks={result['clicks']} energy={result['energy_spread_db']:.8f} dB peak={result['peak_spread_db']:.8f} dB spectral_error={result['max_relative_spectral_error']:.8g} corr={result['minimum_correlation']:.8f}",flush=True)
        tolerance=.05 if args.native else 1e-10
        if control:
            if result['energy_spread_db']<.5:failures.append(base.name+': positive control missed')
        elif result['energy_spread_db']>tolerance or result['peak_spread_db']>tolerance or result['minimum_correlation']<.999 or result['max_relative_spectral_error']>(.005 if args.native else 1e-12):
            failures.append(base.name+': unequal clicks')
        # Native plain timing must match one fixed sample-clock grid across the
        # entire capture. No per-beat shift is applied by this analyzer.
        if args.native and int(case['poly'])==0 and int(case['subdivision'])==1 and not control:
            period=rate*60*4/(int(case['bpm'])*int(case['unit']))
            actual=[]
            for row in result['per_click']:
                n=row['frame']
                width=min(rate//20,round(period)//2)
                ref=cells[0,:width]
                actual.append(max(range(n-4,n+5),key=lambda q:float(x[q:q+width]@ref)))
            spacing=np.diff(actual);result['measured_interval_range_frames']=[int(spacing.min()),int(spacing.max())]
            if np.max(np.abs(spacing-period))>1.001:failures.append(base.name+': sample-clock jitter/dropout')
        if len(onsets)>5:plot(base,x,onsets,rate,result,cells)
    (args.folder/'analysis.json').write_text(json.dumps({'measurement':'digital full-click energy/peak/RMS/FFT, not acoustic loudness','cases':results,'failures':failures},indent=2)+'\n')
    if failures:
        print('FAILURES: '+'; '.join(failures))
        if args.expect_flat:raise SystemExit(1)
    else:print('PASS all full-click level, waveform, spectrum and timing checks')

if __name__=='__main__':main()
