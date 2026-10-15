#include "../src/rhythm.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <numeric>
struct Measurement { double dbSpread=0,maxDifference=0; int clicks=0,duplicates=0; };
Measurement audit(pulse::Settings s,int rate) {
    pulse::Timeline timeline(s,rate); std::vector<pulse::Event> events;
    // Multiple complete bars, including several group boundaries and loop transitions.
    for(int bar=0;bar<4;++bar) { auto e=timeline.nextBar(); events.insert(events.end(),e.begin(),e.end()); }
    pulse::Synth synth(rate); size_t next=0; int64_t end=events.back().frame+rate/8;
    std::vector<float> samples(static_cast<size_t>(end));
    for(int64_t frame=0;frame<end;++frame) {
        while(next<events.size()&&events[next].frame<=frame) { auto index=next++; synth.trigger(events[index],s,pulse::framesUntilNextClick(events,index,end)); }
        samples[frame]=synth.sample();
        if(!std::isfinite(samples[frame])||std::abs(samples[frame])>.950001f) throw std::runtime_error("Invalid rendered output");
    }
    Measurement m; double lo=1e100,hi=0; std::vector<float> reference;
    int64_t shortest=rate/8;
    for(size_t i=1;i<events.size();++i) if(events[i].frame>events[i-1].frame) shortest=std::min(shortest,events[i].frame-events[i-1].frame);
    for(size_t i=0;i<events.size();++i) {
        auto frame=events[i].frame;
        if(i&&frame==events[i-1].frame) { ++m.duplicates; continue; }
        if(frame<rate/3 || i+1==events.size()) continue; // exclude startup overlap transient
        double energy=0;
        for(int64_t k=0;k<shortest;++k) energy+=double(samples[frame+k])*samples[frame+k];
        if(reference.empty()) reference.assign(samples.begin()+frame,samples.begin()+frame+shortest);
        for(int64_t k=0;k<shortest;++k) m.maxDifference=std::max(m.maxDifference,double(std::abs(samples[frame+k]-reference[k])));
        lo=std::min(lo,energy); hi=std::max(hi,energy); ++m.clicks;
    }
    if(lo>0&&hi>0) m.dbSpread=10*std::log10(hi/lo);
    return m;
}
int main(int argc,char** argv) {
 try {
    std::ofstream csv(argc>1?argv[1]:"build/accent-audit.csv");
    csv<<"kind,rate,bpm,beats,unit,subdivision,poly,sound,volume,clicks,duplicates,energy_spread_db,max_sample_difference\n";
    int cases=0; double worstRegular=0; int failures=0;
    auto run=[&](const char* kind,pulse::Settings s,int rate,bool expectUniform) {
        auto m=audit(s,rate); ++cases;
        csv<<kind<<','<<rate<<','<<s.bpm<<','<<s.simple.beats<<','<<s.simple.unit<<','<<s.subdivision<<','<<s.poly<<','<<s.sound<<','<<s.volume<<','<<m.clicks<<','<<m.duplicates<<','<<std::setprecision(10)<<m.dbSpread<<','<<m.maxDifference<<'\n';
        if(expectUniform) { worstRegular=std::max(worstRegular,m.dbSpread); if(m.dbSpread>.01) ++failures; }
    };
    for(int sound=0;sound<4;++sound) for(int volume:{70,100,200,400}) for(int bpm:{60,137,160,199,300}) for(int unit:{4,8,16}) for(int subdivision:{1,2,3,4}) {
        pulse::Settings s; s.accents=false; s.sound=sound; s.volume=volume; s.bpm=bpm; s.subdivision=subdivision; s.simple={7,unit,1,0,{2,2,3}};
        run("regular",s,48000,true);
    }
    for(int sound=0;sound<4;++sound) for(int rate:{44100,48000,96000}) {
        pulse::Settings s; s.accents=false; s.sound=sound; s.volume=400; s.bpm=160;
        run("reported_setup",s,rate,true);
        s.sequence=true; s.bars={{4,4,1,0,{4}},{7,4,1,0,{2,2,3}},{3,4,1,0,{1,2}}};
        run("mixed_meter",s,rate,true);
        s.sequence=false; s.accents=true; run("positive_accent_control",s,rate,false);
        s.accents=false; s.poly=3; run("second_pulse",s,rate,true);
    }
    for(int sound=0;sound<4;++sound) for(int bpm:{20,300}) for(int unit:{2,16}) for(int sub:{1,4}) {
        pulse::Settings s; s.accents=false; s.sound=sound; s.volume=400; s.bpm=bpm; s.subdivision=sub; s.simple={1,unit,1,0,{1}};
        run("range_boundary",s,48000,true);
    }
    for(int sound=0;sound<4;++sound) {
        pulse::Settings s; s.accents=false; s.sound=sound; s.volume=400; s.bpm=300; s.simple={32,4,1,0,{7,9,16}};
        run("32_beat_meter",s,48000,true);
    }
    std::cout<<"Rendered "<<cases<<" complete-stream cases. Worst regular unaccented energy spread: "<<worstRegular<<" dB. Cases over 0.01 dB: "<<failures<<". Detailed CSV written.\n";
    return failures?1:0;
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n'; return 2;}
}
