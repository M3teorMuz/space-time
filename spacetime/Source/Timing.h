#pragma once
#include <algorithm>
#include <cmath>

// Тайминги по логике калькулятора 2Rock / anotherproducer (при 120 BPM):
//   Hall (2 bars)       pre 62.5   decay 3937.5   total 4000
//   Large Room (1 bar)  pre 31.25  decay 1968.75  total 2000
//   Small Room (1/2)    pre 15.63  decay 984.38   total 1000
//   Tight Ambience(1/4) pre 3.91   decay 496.09   total 500
// Slap Verb (1/16) и Short Reverb (1/8) есть на 2Rock, но их формулы не видны
// на странице -> продолжены по той же схеме (pre = total/128).
namespace spacetime
{
struct SizeInfo
{
    const char* name;      // для списка параметров
    const char* shortName; // для таблицы в интерфейсе
    const char* note;
    double beats;          // длительность в четвертях; <0 => такты (|значение| тактов)
    double preDivisor;     // pre-delay = total / preDivisor
};

inline const SizeInfo* sizeTable()
{
    static const SizeInfo t[6] = {
        { "Slap Verb 1/16",      "SLAP VERB",     "1/16",   0.25, 128.0 },
        { "Short Reverb 1/8",    "SHORT REVERB",  "1/8",    0.5,  128.0 },
        { "Tight Ambience 1/4",  "TIGHT AMBIENCE","1/4",    1.0,  128.0 },
        { "Small Room 1/2",      "SMALL ROOM",    "1/2",    2.0,   64.0 },
        { "Large Room 1 Bar",    "LARGE ROOM",    "1 BAR", -1.0,   64.0 },
        { "Hall 2 Bars",         "HALL",          "2 BARS",-2.0,   64.0 } };
    return t;
}

struct Times { float preMs, decayMs, totalMs; };

inline double clampBpm (double bpm) { return std::min(300.0, std::max(20.0, bpm)); }

inline Times calculate (double bpm, double beatsPerBar, int sizeIndex)
{
    const auto& s = sizeTable()[std::min(5, std::max(0, sizeIndex))];
    const double beatMs = 60000.0 / clampBpm(bpm);
    const double beats = s.beats > 0 ? s.beats : -s.beats * beatsPerBar;
    const double total = beatMs * beats;
    const double pre = total / s.preDivisor;
    return { (float) pre, (float) (total - pre), (float) total };
}

// Итоговые значения: авто (из BPM) или ручные (Pre-Delay и Reverb Time).
inline Times resolve (bool autoTiming, double bpm, double beatsPerBar, int sizeIndex,
                      float manualPreMs, float manualTotalMs)
{
    if (autoTiming)
        return calculate(bpm, beatsPerBar, sizeIndex);
    const float pre = std::max(0.0f, manualPreMs);
    const float decay = std::max(10.0f, manualTotalMs - pre);
    return { pre, decay, pre + decay };
}
} // namespace spacetime
