#include "rhythm.h"
#include <iostream>
#include <functional>
#include <limits>

int checks = 0;
void require(bool result, const char* message) { ++checks; if (!result) throw std::runtime_error(message); }
void rejects(const std::function<void()>& fn) { bool threw=false; try { fn(); } catch(const std::exception&) { threw=true; } require(threw,"Invalid input was accepted"); }
int main() {
    try {
        pulse::Settings s;
        for(int sound=0;sound<4;++sound) {
            pulse::Settings flat; flat.sound=sound; flat.accents=false;
            std::stringstream preset; pulse::save(preset,flat);
            require(!pulse::load(preset).accents,"Accent preference survives save/load");
            for(auto event:std::vector<pulse::Event>{{0,0,2,false,false,0,0},{0,0,0,false,false,0,0},{0,0,1,true,false,0,0}}) {
                pulse::Synth reference, actual;
                reference.trigger({0,0,1,false,false,0,0},flat); actual.trigger(event,flat);
                for(int sample=0;sample<5200;++sample) require(reference.sample()==actual.sample(),"No accents produces identical waveform for every click type and sound");
            }
        }
        for(int version:{1,2}) {
            std::stringstream old("PULSE "+std::to_string(version)+"\n120 1 0 0 70 0 0\n4 4 1 0 4\n1\n4 4 2 0 4\n");
            require(pulse::load(old).accents,"Legacy presets retain accents");
        }
        { std::stringstream bad("PULSE 3\n120 1 0 0 70 0 0 2\n"); rejects([&]{pulse::load(bad);}); }

        pulse::Timeline standard(s);
        auto bar=standard.nextBar();
        require(bar.size()==4,"4/4 click count");
        for(int i=0;i<4;++i) require(bar[i].frame==24000*i,"120 BPM quarter-note timing");
        require(standard.nextBar()[0].frame==96000,"4/4 loop boundary");
        s.sequence=true; s.bars={{4,4,1,0,{4}},{7,8,1,0,{2,2,3}},{3,4,1,60,{3}}};
        pulse::Timeline mixed(s); mixed.nextBar(); bar=mixed.nextBar();
        require(bar.size()==7&&bar[0].frame==96000&&bar[6].frame==168000,"7/8 boundary and eighth-note duration");
        for(int i=0;i<7;++i) require(bar[i].accent==((i==0||i==2||i==4)?2:1),"2+2+3 accents");
        bar=mixed.nextBar(); require(bar[0].frame==180000&&bar[1].frame==228000,"Step tempo changes");
        require(mixed.nextBar()[0].frame==324000,"Sequence loops without gaps");
        s.bars={{3,4,2,0,{3}},{5,8,3,0,{2,3}}}; pulse::Timeline repeated(s);
        const int steps[]{0,0,1,1,1,0}; const int repeats[]{0,1,0,1,2,0};
        for(int i=0;i<6;++i) { auto e=repeated.nextBar()[0]; require(e.step==steps[i]&&e.repeat==repeats[i],"Repeat ordering"); }
        s.sequence=true; s.bars={{7,8,0,0,{2,2,3}},{3,4,2,0,{3}},{4,4,1,0,{4}}};
        pulse::Timeline forever(s);
        for(int i=0;i<12000;++i) {
            auto e=forever.nextBar()[0];
            require(e.step==0 && e.repeat==0,"Forever stays on selected step without counter growth");
            require(e.frame==84000LL*i,"Forever maintains exact bar boundaries");
        }
        forever.advanceStep(); auto next=forever.nextBar()[0];
        require(next.step==1 && next.repeat==0 && next.frame==84000LL*12000,"Manual advance exits Forever at next bar");
        require(forever.nextBar()[0].step==1,"Finite step repeats after Forever");
        forever.advanceStep(); require(forever.nextBar()[0].step==2,"Manual advance on last finite bar does not skip a step");
        require(forever.nextBar()[0].step==0,"Sequence wraps back to Forever");
        std::stringstream unlimited; pulse::save(unlimited,s); auto unlimitedLoaded=pulse::load(unlimited);
        require(unlimitedLoaded.bars[0].repeats==0,"Forever survives preset save/load");
        std::stringstream legacy("PULSE 1\n120 1 0 0 70 0 0\n4 4 1 0 4\n1\n4 4 2 0 4\n");
        require(pulse::load(legacy).bars[0].repeats==2,"Old presets still load");
        {
            pulse::Settings liveSettings; pulse::Timeline live(liveSettings);
            auto events=live.nextBar(); live.changeTempo(60,events,1);
            require(events[0].frame==0 && events[1].frame==24000,"Live change preserves already scheduled beat and elapsed events");
            require(events[2].frame==72000 && events[3].frame==120000,"Live slowdown changes remaining beats without restart");
            require(std::llround(live.nextBarFrame())==168000,"Live slowdown adjusts bar boundary");
            live.changeTempo(240,events,2);
            require(events[2].frame==72000 && events[3].frame==84000,"Second live change preserves current beat phase");
            require(live.nextBar()[0].frame==96000,"Next bar continues with updated tempo and no gap");
            rejects([&]{live.changeTempo(0,events,1);});
        }
        {
            pulse::Settings layers; layers.subdivision=3; layers.poly=3; layers.mute=100;
            pulse::Timeline live(layers); auto events=live.nextBar(); auto before=events;
            size_t boundary=0; while(events[boundary].poly || events[boundary].beatPosition!=1 || events[boundary].accent==0) ++boundary;
            live.changeTempo(60,events,boundary);
            for(size_t i=0;i<events.size();++i) {
                require(events[i].frame==(i<boundary?before[i].frame:std::llround(24000+(events[i].beatPosition-1)*48000)),"Live tempo scales subdivisions and overlay with the main pulse");
                require(events[i].beat==before[i].beat && events[i].silent==before[i].silent && events[i].step==before[i].step,"Live tempo preserves beat identity, mute decisions and step");
            }
        }
        {
            pulse::Settings programmed; programmed.sequence=true; programmed.bars={{3,4,1,90,{3}},{7,8,0,0,{2,2,3}}};
            pulse::Timeline live(programmed); auto events=live.nextBar(); live.changeTempo(60,events,1);
            require(events[2].frame==64000,"Explicit step BPM is preserved");
            events=live.nextBar(); require(events[0].frame==96000 && events[1].frame==120000 && events[0].step==1,"Next mixed-meter step inherits live main BPM");
        }
        {
            pulse::Settings fractional; fractional.bpm=137; pulse::Timeline live(fractional); long double expected=0;
            for(int i=0;i<1000;++i) {
                int old=live.config().bpm,newBpm=old==137?199:137;
                auto events=live.nextBar(); require(std::abs(events[0].frame-expected)<=.5001L,"Repeated live changes do not accumulate rounding drift");
                live.changeTempo(newBpm,events,2);
                expected+=48000.L*60*2/old+48000.L*60*2/newBpm;
                require(std::abs(live.nextBarFrame()-expected)<.0001L,"Live changes preserve fractional-sample bar timing");
            }
        }
        s=pulse::Settings{}; s.subdivision=3; s.poly=3; pulse::Timeline poly(s); bar=poly.nextBar();
        require(bar.size()==15,"Subdivisions plus polyrhythm");
        std::vector<int64_t> polyFrames; for(auto e:bar) if(e.poly) polyFrames.push_back(e.frame);
        require(polyFrames==std::vector<int64_t>({0,32000,64000}),"3 against 4 timing");
        s.poly=0; s.mute=100; pulse::Timeline silent(s); for(auto e:silent.nextBar()) require(e.silent,"Silent subdivisions must follow muted beat");
        s.mute=0; s.bpm=137; s.simple={7,8,1,0,{2,2,3}}; s.subdivision=1;
        pulse::Timeline precise(s); const long double duration=48000.L*60*4*7/(137*8);
        for(int i=0;i<10000;++i) { auto e=precise.nextBar()[0]; require(std::abs(e.frame-i*duration)<=.5001L,"Long-running timing drift"); }
        require(pulse::parseGroups(" 2 + 2 + 3 ",7)==std::vector<int>({2,2,3}),"Whitespace in groups");
        for(const auto* bad:{"2+3","0+7","-1+8","2++5","7+","3.5+3.5","7garbage","99999999999999"}) rejects([&]{pulse::parseGroups(bad,7);});
        s.sequence=true; std::stringstream saved; pulse::save(saved,s); auto loaded=pulse::load(saved); std::stringstream resaved; pulse::save(resaved,loaded); require(saved.str()==resaved.str(),"Preset round trip");
        for(const auto* bad:{"", "PULSE 9", "PULSE 1\n120 1 0 0 70 0 4", "PULSE 1\n120 1 0 0 70 0 1\n4 4 1 0 4\n999999999"}) rejects([&]{std::stringstream in(bad);pulse::load(in);});
        { std::stringstream in(saved.str()+"unexpected"); rejects([&]{pulse::load(in);}); }
        s=pulse::Settings{}; pulse::Synth synth; auto e=pulse::Timeline(s).nextBar()[0]; synth.trigger(e,s); double energy=0;
        for(int i=0;i<4800;++i) { float sample=synth.sample(); require(std::isfinite(sample)&&std::abs(sample)<=.95f,"Invalid audio output"); energy+=sample*sample; if(i>2400) require(sample==0,"Click tail did not stop"); }
        require(energy>1,"Click is inaudible"); e.silent=true; synth.trigger(e,s); require(synth.sample()==0,"Muted click emitted audio");
        for(int sound=0;sound<4;++sound) {
            pulse::Settings normal, boosted; normal.sound=sound; normal.volume=100; boosted=normal; boosted.volume=200;
            pulse::Synth a,b; pulse::Event click{0,0,2,false,false,0,0}; a.trigger(click,normal); b.trigger(click,boosted);
            double normalEnergy=0,boostEnergy=0;
            for(int i=0;i<2600;++i) { double x=a.sample(), y=b.sample(); normalEnergy+=x*x; boostEnergy+=y*y; require(std::isfinite(y)&&std::abs(y)<=.950001,"Boosted sound remains bounded"); }
            require(boostEnergy>normalEnergy*1.5,"200 percent audibly increases energy for every sound");
            std::stringstream preset; pulse::save(preset,boosted); require(pulse::load(preset).volume==200,"Boost survives preset round trip");
            boosted.volume=401; rejects([&]{pulse::validate(boosted);}); boosted.volume=-1; rejects([&]{pulse::validate(boosted);});
            boosted.volume=200; pulse::Synth layered;
            for(int i=0;i<64;++i) layered.trigger(click,boosted);
            for(int i=0;i<2600;++i) require(std::abs(layered.sample())<=.950001f,"Limiter bounds overlapping boosted voices");
            boosted.volume=0; pulse::Synth zero; zero.trigger(click,boosted);
            for(int i=0;i<2600;++i) require(zero.sample()==0,"Zero volume remains silent");
        }
        for(int version:{3,4}) {
            std::stringstream old("PULSE "+std::to_string(version)+"\n120 1 0 0 100 2 0 0\n4 4 1 0 4\n1\n4 4 2 0 4\n");
            auto loaded=pulse::load(old);
            require(loaded.volume==100 && loaded.sound==2 && !loaded.accents,"Previous boost/accents presets remain compatible");
        }
        double cleanEnergy=0, fullEnergy=0;
        for(int sound=0;sound<4;++sound) {
            pulse::Settings low; low.sound=sound; low.volume=200;
            auto high=low; high.volume=400;
            pulse::Synth a,b; pulse::Event click{0,0,1,false,false,0,0}; a.trigger(click,low); b.trigger(click,high);
            double lowEnergy=0,highEnergy=0,tailEnergy=0;
            for(int i=0;i<6000;++i) {
                double x=a.sample(),y=b.sample(); lowEnergy+=x*x; highEnergy+=y*y;
                require(std::isfinite(y)&&std::abs(y)<=.950001,"400 percent output remains finite and limited");
                if(i>=2400&&i<4800) tailEnergy+=x*x;
                if(i>=4800) require(x==0&&y==0,"Full click ends cleanly at 100ms");
            }
            require(highEnergy>lowEnergy*1.2,"400 percent increases energy over 200 despite limiting");
            if(sound==0) cleanEnergy=lowEnergy;
            if(sound==3) { fullEnergy=lowEnergy; require(tailEnergy>.001,"Full click sustains beyond the old 50ms tail"); }
            std::stringstream preset; pulse::save(preset,high); auto restored=pulse::load(preset);
            require(restored.volume==400 && restored.sound==sound,"400 percent and sound selection round-trip");
            pulse::Synth layered;
            for(int i=0;i<64;++i) layered.trigger(click,high);
            for(int i=0;i<6000;++i) require(std::abs(layered.sample())<=.950001f,"400 percent overlapping clicks remain bounded");
            std::cout<<"Sound "<<sound<<": energy at 200%="<<lowEnergy<<", 400%="<<highEnergy<<"\n";
        }
        require(fullEnergy>cleanEnergy*1.5,"Full click provides more sustained energy than clean tone at equal volume");
        { pulse::Settings bad; bad.sound=4; rejects([&]{pulse::validate(bad);}); }
        // Regression: flat clicks must not add energy at coincident onsets.
        for(int sound=0;sound<4;++sound) for(int volume:{70,200,400}) {
            pulse::Settings flat; flat.accents=false; flat.sound=sound; flat.volume=volume;
            pulse::Event main{0,0,2,false,false,0,0}, overlay{0,0,1,true,false,0,0};
            pulse::Synth one,two;
            one.trigger(main,flat,6000); two.trigger(main,flat,6000); two.trigger(overlay,flat,6000);
            for(int i=0;i<6000;++i) require(one.sample()==two.sample(),"Coincident flat clicks must equal one click");
            pulse::Synth tailed,fresh;
            auto accented=flat; accented.accents=true; tailed.trigger(main,accented);
            for(int i=0;i<500;++i) tailed.sample();
            tailed.trigger(overlay,flat,901); fresh.trigger(main,flat,901);
            float last=1;
            for(int i=0;i<901;++i) { last=tailed.sample(); require(last==fresh.sample(),"Disabling accents clears previous voiced tails"); }
            require(last==0 && tailed.sample()==0,"Shortened flat click fades to zero before next onset");
        }
        {
            std::vector<pulse::Event> clicks{{0,0,2,false,false,0,0},{0,0,1,true,false,0,0},{100,0,0,false,true,0,0},{200,1,1,false,false,0,0}};
            require(pulse::framesUntilNextClick(clicks,0,400)==200,"Lookahead skips coincident and silent clicks");
            require(pulse::framesUntilNextClick(clicks,3,400)==200,"Lookahead covers the next bar boundary");
        }
        std::cout << "Passed " << checks << " checks: mixed meters, repeats, accents, tempo, polyrhythm, silence, 10,000-bar drift, preset validation, and audio synthesis.\n";
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
