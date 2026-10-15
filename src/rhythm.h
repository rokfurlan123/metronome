#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace pulse {
struct Bar {
    int beats = 4, unit = 4, repeats = 1, bpm = 0;
    std::vector<int> groups{4};
};
struct Settings {
    int bpm = 120, subdivision = 1, poly = 0, mute = 0, volume = 70, sound = 0;
    bool sequence = false, accents = true;
    Bar simple;
    std::vector<Bar> bars{{4,4,2,0,{4}}, {7,8,2,0,{2,2,3}}, {3,4,2,0,{3}}};
};
inline std::vector<int> parseGroups(const std::string& value, int beats) {
    if (value.empty()) return {beats};
    std::vector<int> groups;
    std::istringstream in(value);
    std::string piece;
    int sum = 0;
    if (value.back() == '+') throw std::runtime_error("Finish the accent grouping after +.");
    while (std::getline(in, piece, '+')) {
        std::istringstream p(piece); int n; std::string extra;
        if (!(p >> n) || (p >> extra) || n < 1 || n > 32) throw std::runtime_error("Use accent groups such as 2+2+3.");
        groups.push_back(n); sum += n;
    }
    if (sum != beats) throw std::runtime_error("Accent groups must add up to the number of beats.");
    return groups;
}
inline std::string groupText(const Bar& b) {
    std::string s;
    for (int n : b.groups) { if (!s.empty()) s += "+"; s += std::to_string(n); }
    return s;
}
inline void validate(const Bar& b) {
    if (b.beats < 1 || b.beats > 32 || (b.unit != 2 && b.unit != 4 && b.unit != 8 && b.unit != 16) || b.repeats < 0 || b.repeats > 9999 || (b.bpm != 0 && (b.bpm < 20 || b.bpm > 300)))
        throw std::runtime_error("Use 1–32 beats, /2 /4 /8 /16, 0 (forever) or 1–9999 repeats, and 20–300 BPM (or 0 to follow the main tempo).");
    if (b.groups.empty() || parseGroups(groupText(b), b.beats) != b.groups) throw std::runtime_error("Invalid accent grouping.");
}
inline void validate(const Settings& s) {
    if (s.bpm < 20 || s.bpm > 300 || s.subdivision < 1 || s.subdivision > 4 || s.poly < 0 || s.poly > 16 || s.mute < 0 || s.mute > 100 || s.volume < 0 || s.volume > 400 || s.sound < 0 || s.sound > 3 || s.bars.empty() || s.bars.size() > 128)
        throw std::runtime_error("Practice settings are outside their supported range.");
    validate(s.simple); for (const auto& b : s.bars) validate(b);
}
inline void save(std::ostream& out, const Settings& s) {
    validate(s);
    out << "PULSE 5\n" << s.bpm << ' ' << s.subdivision << ' ' << s.poly << ' ' << s.mute << ' ' << s.volume << ' ' << s.sound << ' ' << s.sequence << ' ' << s.accents << '\n';
    auto bar = [&](const Bar& b) { out << b.beats << ' ' << b.unit << ' ' << b.repeats << ' ' << b.bpm << ' ' << groupText(b) << '\n'; };
    bar(s.simple); out << s.bars.size() << '\n'; for (const auto& b : s.bars) bar(b);
    if (!out) throw std::runtime_error("Could not write this preset.");
}
inline Settings load(std::istream& in) {
    Settings s; std::string magic; int version, seq;
    if (!(in >> magic >> version) || magic != "PULSE" || (version < 1 || version > 5)) throw std::runtime_error("This is not a supported Pulse preset.");
    if (!(in >> s.bpm >> s.subdivision >> s.poly >> s.mute >> s.volume >> s.sound >> seq) || (seq != 0 && seq != 1)) throw std::runtime_error("Incomplete preset settings.");
    s.sequence = seq;
    if (version >= 3) {
        int accents;
        if (!(in >> accents) || (accents != 0 && accents != 1)) throw std::runtime_error("Invalid accent setting.");
        s.accents = accents != 0;
    }
    auto bar = [&]() { Bar b; std::string groups; if (!(in >> b.beats >> b.unit >> b.repeats >> b.bpm >> groups)) throw std::runtime_error("Incomplete bar in preset."); b.groups = parseGroups(groups, b.beats); validate(b); return b; };
    s.simple = bar(); int count;
    if (!(in >> count) || count < 1 || count > 128) throw std::runtime_error("Presets must contain 1–128 sequence steps.");
    s.bars.clear(); for (int i = 0; i < count; ++i) s.bars.push_back(bar());
    std::string extra; if (in >> extra) throw std::runtime_error("Unexpected data at end of preset.");
    validate(s); return s;
}
struct Event { int64_t frame; int beat, accent; bool poly, silent; int step, repeat; long double beatPosition = 0; };
// Absolute fractional sample positions carry across bars, avoiding rounding drift.
class Timeline {
    Settings settings;
    int rate, step = 0, repeat = 0, lastStep = 0;
    long double start = 0, barAnchorFrame = 0, barAnchorBeat = 0, barBeatFrames = 0;
    uint32_t random = 0x12345678;
public:
    explicit Timeline(Settings s, int sampleRate = 48000) : settings(std::move(s)), rate(sampleRate) { validate(settings); }
    const Settings& config() const { return settings; }
    long double nextBarFrame() const { return start; }
    void advanceStep() { if (settings.sequence) { step = (lastStep + 1) % settings.bars.size(); repeat = 0; } }
    // Apply a main-tempo change at an already scheduled, unrendered beat.
    // Preserve its exact boundary and all musical positions within this bar.
    void changeTempo(int bpm, std::vector<Event>& events, size_t boundary) {
        if (bpm < 20 || bpm > 300) throw std::runtime_error("Tempo must be between 20 and 300 BPM.");
        if (boundary >= events.size() || events[boundary].poly || events[boundary].accent == 0)
            throw std::runtime_error("Tempo changes require a main beat boundary.");
        const Bar& b = settings.sequence ? settings.bars[lastStep] : settings.simple;
        settings.bpm = bpm;
        if (b.bpm) return; // Explicit step tempos continue to override the main tempo.
        long double beat = events[boundary].beatPosition;
        barAnchorFrame += (beat - barAnchorBeat) * barBeatFrames;
        barAnchorBeat = beat;
        barBeatFrames = static_cast<long double>(rate) * 60 * 4 / (bpm * b.unit);
        for (size_t i = boundary; i < events.size(); ++i)
            events[i].frame = static_cast<int64_t>(std::llround(barAnchorFrame + (events[i].beatPosition - beat) * barBeatFrames));
        start = barAnchorFrame + (b.beats - beat) * barBeatFrames;
    }
    std::vector<Event> nextBar() {
        lastStep = step;
        const Bar& b = settings.sequence ? settings.bars[step] : settings.simple;
        int bpm = b.bpm ? b.bpm : settings.bpm;
        long double beatFrames = static_cast<long double>(rate) * 60 * 4 / (bpm * b.unit);
        long double length = beatFrames * b.beats;
        barAnchorFrame = start; barAnchorBeat = 0; barBeatFrames = beatFrames;
        std::vector<Event> events;
        for (int beat = 0; beat < b.beats; ++beat) {
            random ^= random << 13; random ^= random >> 17; random ^= random << 5;
            bool silent = random % 100 < static_cast<unsigned>(settings.mute);
            bool accent = false; int pos = 0;
            for (int group : b.groups) { if (pos == beat) accent = true; pos += group; }
            for (int sub = 0; sub < settings.subdivision; ++sub)
                events.push_back({static_cast<int64_t>(std::llround(start + beatFrames * (beat + static_cast<long double>(sub) / settings.subdivision))), beat, sub ? 0 : (accent ? 2 : 1), false, silent, step, repeat, beat + static_cast<long double>(sub) / settings.subdivision});
        }
        for (int p = 0; p < settings.poly; ++p)
            events.push_back({static_cast<int64_t>(std::llround(start + length * p / settings.poly)), p, 1, true, false, step, repeat, static_cast<long double>(b.beats) * p / settings.poly});
        std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b) { return a.frame < b.frame; });
        start += length;
        if (settings.sequence && b.repeats != 0 && ++repeat == b.repeats) { repeat = 0; step = (step + 1) % settings.bars.size(); }
        return events;
    }
};
// Bound a flat click to the next audible onset, including the next bar boundary.
inline int framesUntilNextClick(const std::vector<Event>& events,size_t index,int64_t barEnd) {
    int64_t end=barEnd;
    for(size_t next=index+1;next<events.size();++next) {
        if(events[next].frame>events[index].frame && !events[next].silent) { end=events[next].frame; break; }
    }
    return static_cast<int>(std::clamp<int64_t>(end-events[index].frame,1,2147483647));
}
class Synth {
    struct Voice { int age, sound; double hz, gain; int duration; bool shortened; };
    std::vector<Voice> voices;
    int rate;
public:
    explicit Synth(int sampleRate = 48000) : rate(sampleRate) { voices.reserve(64); }
    void trigger(const Event& e, const Settings& s, int untilNext=-1) {
        if(e.silent) return;
        // Flat clicks never stack. Simultaneous main/second pulses become one click,
        // and old accented tails cannot carry into a newly unaccented click.
        if(!s.accents) voices.clear();
        if(voices.size() >= 64) return;
        double hz = !s.accents ? 1174 : e.poly ? 620 : (e.accent == 2 ? 1760 : e.accent == 1 ? 1174 : 830);
        double gain = (!s.accents ? .42 : e.poly ? .28 : e.accent == 2 ? .65 : e.accent == 1 ? .42 : .18) * s.volume / 100.;
        int natural=rate/(s.sound==3?10:20)+1;
        int duration=!s.accents&&untilNext>0?std::min(natural,untilNext):natural;
        voices.push_back({0, s.sound, hz, gain,duration,duration<natural});
    }
    float sample() {
        double result = 0;
        for (auto& v : voices) {
            double t = double(v.age++) / rate;
            double f = v.hz * (v.sound == 1 ? .65 : v.sound == 2 ? 1.4 : v.sound == 3 ? .75 : 1.0);
            double wave = std::sin(6.283185307179586 * f * t);
            if (v.sound == 1) wave += .35 * std::sin(6.283185307179586 * f * 2.76 * t);
            if (v.sound == 2) wave = std::tanh(wave * 3);
            if (v.sound == 3) {
                // A fuller tone with harmonic body and a 100 ms tail.
                double phase=6.283185307179586*f*t;
                wave=.72*std::sin(phase)+.20*std::sin(2*phase)+.08*std::sin(3*phase);
            }
            double decay=v.sound == 3 ? 60 : v.sound == 1 ? 150 : 230;
            double tail=v.sound == 3 ? std::clamp((.1-t)/.02,0.,1.) : 1.;
            // Fade shortened tails to zero before the next onset to avoid a hard cut.
            if(v.shortened) {
                int fade=std::max(1,std::min(rate/500,v.duration/4));
                tail*=std::clamp(double(v.duration-v.age)/fade,0.,1.);
            }
            result += wave * std::exp(-t * decay) * std::min(1.0, t * 2000) * tail * v.gain;
        }
        voices.erase(std::remove_if(voices.begin(), voices.end(), [&](const Voice& v) { return v.age >= v.duration; }), voices.end());
        // Preserve normal levels; smoothly limit summed peaks instead of hard clipping.
        double magnitude=std::abs(result);
        if(magnitude>.8) result=std::copysign(.8+.15*std::tanh((magnitude-.8)/.15),result);
        return static_cast<float>(std::clamp(result, -.95, .95));
    }
};
}
