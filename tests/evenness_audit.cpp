#ifndef PULSE_RHYTHM_HEADER
#define PULSE_RHYTHM_HEADER "../src/rhythm.h"
#endif
#include PULSE_RHYTHM_HEADER
#include <fstream>
#include <iostream>
#include <iomanip>
#include <random>
#include <filesystem>
#include <map>

struct Click {pulse::Event e; int until;};
struct Result {double energyDb=0,peakDb=0,difference=0,correlation=1; uint64_t clicks=0,samples=0;};
std::vector<Click> schedule(const pulse::Settings& s,int rate,int bars) {
    pulse::Timeline t(s,rate); std::vector<Click> result;
    for(int b=0;b<bars;++b) {
        auto events=t.nextBar(); auto end=static_cast<int64_t>(std::llround(t.nextBarFrame()));
        for(size_t i=0;i<events.size();++i) result.push_back({events[i],pulse::framesUntilNextClick(events,i,end)});
    }
    return result;
}
Result audit(const pulse::Settings& s,int rate,int bars) {
    auto events=schedule(s,rate,bars); pulse::Synth synth(rate);
    events.erase(std::remove_if(events.begin(),events.end(),[](const Click& c){return c.e.silent;}),events.end());
    const int window=rate/10+2;
    std::vector<float> reference,cell(window); Result result;
    double minEnergy=1e300,maxEnergy=0,minPeak=1e300,maxPeak=0;
    for(size_t i=0;i<events.size();) {
        auto frame=events[i].e.frame; bool audible=false;
        do {audible|=!events[i].e.silent; synth.trigger(events[i].e,s,events[i].until);++i;} while(i<events.size()&&events[i].e.frame==frame);
        auto gap=i<events.size()?events[i].e.frame-frame:window;
        const int count=static_cast<int>(std::min<int64_t>(window,gap));
        std::fill(cell.begin(),cell.end(),0.f);
        double energy=0,peak=0;
        for(int k=0;k<count;++k) {
            float x=synth.sample(); if(!std::isfinite(x)||std::abs(x)>.950001f) throw std::runtime_error("Nonfinite or out-of-bounds sample");
            cell[k]=x; energy+=double(x)*x; peak=std::max(peak,double(std::abs(x)));++result.samples;
        }
        if(!audible) continue;
        ++result.clicks;
        if(s.volume==0) {if(energy!=0) throw std::runtime_error("Zero gain is not silent");continue;}
        if(energy<=0) throw std::runtime_error("Unexpected silent audible click");
        minEnergy=std::min(minEnergy,energy);maxEnergy=std::max(maxEnergy,energy);
        minPeak=std::min(minPeak,peak);maxPeak=std::max(maxPeak,peak);
        if(reference.empty()) reference=cell;
        double dot=0,referenceEnergy=0;
        for(int k=0;k<window;++k) {
            result.difference=std::max(result.difference,double(std::abs(cell[k]-reference[k])));
            dot+=double(cell[k])*reference[k];referenceEnergy+=double(reference[k])*reference[k];
        }
        result.correlation=std::min(result.correlation,dot/std::sqrt(energy*referenceEnergy));
    }
    if(maxEnergy>0) {result.energyDb=10*std::log10(maxEnergy/minEnergy);result.peakDb=20*std::log10(maxPeak/minPeak);}
    return result;
}
void artifact(const std::filesystem::path& base,pulse::Settings s,int rate=48000,int bars=8) {
    auto events=schedule(s,rate,bars); int64_t end=events.back().e.frame+rate/4;
    std::vector<float> data(static_cast<size_t>(end));pulse::Synth synth(rate);size_t next=0;
    std::ofstream markers(base.string()+".clicks.csv");markers<<"frame,silent,poly,accent\n";
    for(const auto& c:events) markers<<c.e.frame<<','<<c.e.silent<<','<<c.e.poly<<','<<c.e.accent<<'\n';
    for(int64_t f=0;f<end;++f) {
        while(next<events.size()&&events[next].e.frame<=f) {synth.trigger(events[next].e,s,events[next].until);++next;}
        data[static_cast<size_t>(f)]=synth.sample();
    }
    std::ofstream out(base.string()+".f32",std::ios::binary);out.write(reinterpret_cast<const char*>(data.data()),static_cast<std::streamsize>(data.size()*sizeof(float)));
    std::ofstream meta(base.string()+".format.txt");meta<<rate<<" 1\n";
}
int main(int argc,char** argv) {
 try {
    if(argc==12&&std::string(argv[1])=="--render") {
        pulse::Settings s;s.bpm=std::stoi(argv[3]);s.simple={std::stoi(argv[4]),std::stoi(argv[5]),1,0,{std::stoi(argv[4])}};
        s.subdivision=std::stoi(argv[6]);s.poly=std::stoi(argv[7]);s.sound=std::stoi(argv[8]);s.volume=std::stoi(argv[9]);s.accents=std::stoi(argv[10])!=0;pulse::validate(s);
        double seconds=std::stod(argv[11]);int bars=2+static_cast<int>(std::ceil(seconds/(60.*4*s.simple.beats/(s.bpm*s.simple.unit))));
        artifact(argv[2],s,48000,bars);return 0;
    }
    std::filesystem::path folder=argc>1?argv[1]:"build/evenness-after";std::filesystem::create_directories(folder);
    bool reportOnly=argc>2&&std::string(argv[2])=="--report-only";
    std::ofstream csv(folder/"matrix.csv");csv<<"kind,rate,bpm,beats,unit,subdivision,poly,mute,sound,volume,sequence,clicks,samples,energy_spread_db,peak_spread_db,max_sample_difference,min_correlation\n";
    uint64_t cases=0,failures=0,clicks=0,samples=0;double worst=0;
    auto run=[&](const char* kind,pulse::Settings s,int rate,int bars=3) {
        auto r=audit(s,rate,bars);++cases;clicks+=r.clicks;samples+=r.samples;
        csv<<kind<<','<<rate<<','<<s.bpm<<','<<s.simple.beats<<','<<s.simple.unit<<','<<s.subdivision<<','<<s.poly<<','<<s.mute<<','<<s.sound<<','<<s.volume<<','<<s.sequence<<','<<r.clicks<<','<<r.samples<<','<<std::setprecision(12)<<r.energyDb<<','<<r.peakDb<<','<<r.difference<<','<<r.correlation<<'\n';
        if(!s.accents) {worst=std::max(worst,r.energyDb);if(r.difference!=0||r.energyDb>1e-10||r.peakDb>1e-10)++failures;}
        else if(s.poly==0&&s.mute==0&&r.energyDb<.5) throw std::runtime_error("Positive accent control was not detected");
        if(cases%500==0) std::cout<<"Audited "<<cases<<" cases, failing flat cases "<<failures<<std::endl;
    };
    // Cartesian dense/irregular grid: every second-pulse count, all sounds/gains/subdivisions.
    for(int sound=0;sound<4;++sound) for(int gain:{100,200,400}) for(int sub=1;sub<=4;++sub) for(int poly=0;poly<=16;++poly) for(int bpm:{137,199,300}) {
        pulse::Settings s;s.sound=sound;s.volume=gain;s.subdivision=sub;s.poly=poly;s.bpm=bpm;s.simple={7,16,1,0,{2,2,3}};run("dense_grid",s,48000);
    }
    // Deterministic fuzz: meter/rate/mute/gain extremes, mixed steps and loop boundaries.
    std::mt19937 random(0x50554c53); const int rates[]{44100,48000,96000};const int units[]{2,4,8,16};
    for(int n=0;n<1200;++n) {
        pulse::Settings s;s.bpm=20+random()%281;s.sound=random()%4;s.volume=random()%401;s.subdivision=1+random()%4;s.poly=random()%17;s.mute=(n%4==0)?random()%101:0;
        s.simple={1+int(random()%32),units[random()%4],1,0,{}};s.simple.groups={s.simple.beats};
        if(n%3==0) {s.sequence=true;s.bars.clear();for(int k=0;k<4;++k) {int beats=1+random()%13;s.bars.push_back({beats,units[random()%4],1,k%2?0:20+int(random()%281),{beats}});}}
        run("seeded_fuzz",s,rates[n%3],s.sequence?8:3);
    }
    for(int sound=0;sound<4;++sound) for(int rate:rates) for(int gain:{0,1,70,100,200,275,400}) {
        pulse::Settings s;s.sound=sound;s.volume=gain;s.bpm=160;run("plain_160",s,rate,12);
        s.bpm=120;run("startup_120",s,rate,12);
        if(gain) {s.accents=true;run("positive_control",s,rate,4);}
    }
    for(int poly:{0,3,5,16}) {pulse::Settings s;s.sound=3;s.volume=200;s.bpm=199;s.subdivision=4;s.poly=poly;s.simple={7,16,1,0,{2,2,3}};artifact(folder/("dense_poly"+std::to_string(poly)),s);}
    for(int bpm:{120,160}) {pulse::Settings s;s.bpm=bpm;artifact(folder/("plain_"+std::to_string(bpm)),s);}
    pulse::Settings control;control.accents=true;artifact(folder/"positive_control",control);
    std::ofstream summary(folder/"summary.txt");summary<<"cases="<<cases<<"\nclicks="<<clicks<<"\nsamples="<<samples<<"\nfailing_flat_cases="<<failures<<"\nworst_energy_spread_db="<<std::setprecision(12)<<worst<<'\n';
    std::cout<<"RESULT cases="<<cases<<" clicks="<<clicks<<" samples="<<samples<<" flat_failures="<<failures<<" worst_energy_db="<<worst<<std::endl;
    return failures&&!reportOnly?1:0;
 }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 2;}
}
