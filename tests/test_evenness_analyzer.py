"""Validate the measurement tool with known good/bad signals, not app output."""
import importlib.util
from pathlib import Path
import unittest
import numpy as np
spec=importlib.util.spec_from_file_location('analyzer',Path(__file__).parents[1]/'scripts/analyze-evenness.py')
a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)

class AnalyzerTests(unittest.TestCase):
    rate=48000
    def stream(self, transform=lambda x,i:x, irregular=False):
        t=np.arange(2400)/self.rate
        pulse=np.sin(2*np.pi*1174*t)*np.exp(-90*t)*np.minimum(1,t*2000)
        onsets=np.cumsum([0]+[6000+(i%3)*3000 if irregular else 12000 for i in range(9)])
        x=np.zeros(onsets[-1]+self.rate//4)
        for i,n in enumerate(onsets):
            part=transform(pulse.copy(),i);x[n:n+len(part)]=part
        return a.metrics(x,list(onsets),self.rate)[0]
    def test_identical(self):
        r=self.stream();self.assertEqual(r['energy_spread_db'],0);self.assertEqual(r['max_sample_difference'],0);self.assertEqual(r['max_relative_spectral_error'],0)
    def test_first_click_not_discarded(self):
        r=self.stream(lambda p,i:p*2 if i==0 else p);self.assertAlmostEqual(r['energy_spread_db'],20*np.log10(2),places=10)
    def test_gain_change(self):
        self.assertGreater(self.stream(lambda p,i:p*.7 if i%2 else p)['energy_spread_db'],3)
    def test_unequal_tail_not_hidden(self):
        def tail(p,i):
            if i%2:p[500:]=0
            return p
        r=self.stream(tail);self.assertGreater(r['energy_spread_db'],.1);self.assertGreater(r['max_relative_spectral_error'],.01)
    def test_polarity_not_hidden_by_energy(self):
        r=self.stream(lambda p,i:-p if i%2 else p);self.assertEqual(r['energy_spread_db'],0);self.assertLess(r['minimum_correlation'],-.999)
    def test_frequency_not_hidden(self):
        def pitch(p,i):
            if not i%2:return p
            t=np.arange(len(p))/self.rate
            q=np.sin(2*np.pi*1760*t)*np.exp(-90*t)*np.minimum(1,t*2000)
            return q*np.linalg.norm(p)/np.linalg.norm(q)
        r=self.stream(pitch);self.assertLess(r['energy_spread_db'],1e-10);self.assertGreater(r['max_relative_spectral_error'],.5)
    def test_missing_click_fails(self):
        with self.assertRaises(ValueError):self.stream(lambda p,i:p*0 if i==4 else p)
    def test_irregular_spacing_does_not_imply_unequal_clicks(self):
        r=self.stream(irregular=True);self.assertEqual(r['energy_spread_db'],0);self.assertEqual(r['max_sample_difference'],0)
    def test_stop_guard_retains_next_onset_as_cell_boundary(self):
        pulse=np.sin(np.arange(600)*.15)*np.hanning(600)
        onsets=list(range(0,24000,1200));x=np.zeros(36000)
        for n in onsets:x[n:n+600]=pulse
        r,cells=a.metrics(x,onsets,self.rate,measurement_end=18000)
        self.assertGreater(r['clicks'],5);self.assertEqual(r['energy_spread_db'],0)
    def test_global_alignment_survives_a_partial_initial_packet(self):
        rate=self.rate;t=np.arange(rate*4)/rate
        # An irregular deterministic train avoids ambiguous single-tone peaks.
        expected=np.zeros(len(t));pulse=np.sin(np.arange(600)*.15)*np.hanning(600)
        frames=np.cumsum([0]+[1800+(i%7)*137 for i in range(78)])
        for n in frames:
            if n+600<len(expected):expected[n:n+600]=pulse
        offset=9876;x=np.r_[np.zeros(offset),expected][:len(expected)]*.15
        x[offset:offset+300]=0
        measured,_=a.native_alignment(x,expected,rate)
        self.assertEqual(measured,offset)

if __name__=='__main__':unittest.main()
