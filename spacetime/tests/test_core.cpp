#include "../Source/ReverbCore.h"
#include "../Source/Timing.h"
#include <cstdio>
#include <random>
#include <vector>
using namespace spacetime;

static double rt60(const std::vector<float>& ir, double sr)
{
    std::vector<double> e(ir.size()); double acc = 0;
    for (int i = (int) ir.size() - 1; i >= 0; --i) { acc += (double) ir[i] * ir[i]; e[i] = acc; }
    const double e0 = e[0]; for (auto& v : e) v = 10.0 * std::log10(v / e0 + 1e-30);
    int i5 = -1, i25 = -1;
    for (size_t i = 0; i < e.size(); ++i) { if (i5 < 0 && e[i] <= -5) i5 = (int) i; if (i25 < 0 && e[i] <= -25) { i25 = (int) i; break; } }
    if (i5 < 0 || i25 < 0) return -1;
    return 3.0 * (i25 - i5) / sr;
}

int main()
{
    int fails = 0;
    // 1. Тайминги при 120 BPM
    struct E { int idx; float pre, dec, tot; } exp[] = { {5,62.5f,3937.5f,4000}, {4,31.25f,1968.75f,2000}, {3,15.625f,984.375f,1000}, {2,3.906f,496.094f,500} };
    for (auto& e : exp) { auto t = calculate(120, 4, e.idx);
        bool ok = std::fabs(t.preMs-e.pre)<0.01 && std::fabs(t.decayMs-e.dec)<0.02 && std::fabs(t.totalMs-e.tot)<0.01;
        std::printf("timing size %d: pre %.3f dec %.3f tot %.3f %s\n", e.idx, t.preMs, t.decayMs, t.totalMs, ok?"OK":"FAIL"); fails += !ok; }
    { auto t = calculate(120, 3, 4); std::printf("3/4 bar @120: tot %.1f (expect 1500)\n", t.totalMs); fails += std::fabs(t.totalMs-1500)>0.01; }

    // 2. RT60 импульса
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    for (float dec : { 500.0f, 1000.0f, 4000.0f })
    {
        ReverbCore c; c.prepare(sr); c.setParameters(0, dec, 0.0f, 1.0f, 1.0f, 4);
        for (int k = 0; k < 200; ++k) { float a[64] = {0}; c.process(a, a, 64); } // дать сгладиться
        int N = (int) (sr * (dec / 1000.0 * 2.5 + 1)); std::vector<float> L(N, 0), R(N, 0); L[0] = R[0] = 1;
        c.process(L.data(), R.data(), N);
        double r = rt60(L, sr);
        bool ok = r > dec * 0.001 * 0.8 && r < dec * 0.001 * 1.25;
        std::printf("RT60 sr=%.0f target=%.2fs measured=%.2fs %s\n", sr, dec/1000.0, r, ok?"OK":"CHECK"); fails += !ok;
    }
    // 3. Стабильность: громкий шум 60 с, макс. damping/decay
    {
        ReverbCore c; c.prepare(48000); std::mt19937 g(1); std::uniform_real_distribution<float> d(-1, 1);
        float peak = 0; bool bad = false;
        for (int blk = 0; blk < 48000 * 60 / 512; ++blk) {
            if (blk % 300 == 0) c.setParameters((blk%600)?200.f:0.f, (blk%600)?30000.f:50.f, (blk%900)?1.f:0.f, 1, 1, blk/300%6);
            float L[512], R[512]; for (int i = 0; i < 512; ++i) { L[i] = d(g); R[i] = d(g); }
            c.process(L, R, 512);
            for (int i = 0; i < 512; ++i) { if (!std::isfinite(L[i])||!std::isfinite(R[i])) bad = true; peak = std::max(peak, std::max(std::fabs(L[i]), std::fabs(R[i]))); }
        }
        std::printf("stability: peak=%.2f finite=%s %s\n", peak, bad?"NO":"yes", (!bad && peak < 30)?"OK":"FAIL"); fails += (bad || peak >= 30);
    }
    // 4. Уровень wet на шуме (RMS вх = 0.1) для разных decay
    for (float dec : { 125.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f })
    {
        ReverbCore c; c.prepare(48000); c.setParameters(10, dec, 0.35f, 1, 1.0f, 3);
        std::mt19937 g(2); std::normal_distribution<float> d(0, 0.1f); double ein = 0, eout = 0; long cnt = 0;
        int total = 48000 * 40;
        for (int blk = 0; blk < total / 512; ++blk) {
            float L[512], R[512]; for (int i = 0; i < 512; ++i) { L[i] = d(g); R[i] = d(g); }
            float inL[512]; std::copy(L, L+512, inL);
            c.process(L, R, 512);
            if (blk * 512 > total / 2) for (int i = 0; i < 512; ++i) { ein += inL[i]*inL[i]; eout += L[i]*L[i]; cnt++; }
        }
        std::printf("level decay=%5.0fms  wet/dry RMS = %+.1f dB\n", dec, 10*std::log10(eout/ein));
    }
    // 5. Щелчки при скачках параметров: макс. разность соседних сэмплов на синусе
    {
        ReverbCore c; c.prepare(48000); c.setParameters(10, 1000, .4f, 1, .5f, 3);
        double ph = 0; float prev = 0, maxd = 0; 
        for (int blk = 0; blk < 48000 * 6 / 256; ++blk) {
            if (blk % 100 == 50) c.setParameters(blk%200? 400.f:5.f, blk%200?200.f:3000.f, blk%200?1.f:0.f, 1, blk%200?1.f:0.f, (blk/100)%6);
            float L[256], R[256]; for (int i = 0; i < 256; ++i) { L[i] = R[i] = 0.2f*std::sin(ph); ph += 2*3.14159265*220/48000; }
            c.process(L, R, 256);
            for (int i = 0; i < 256; ++i) { if (blk > 5) maxd = std::max(maxd, std::fabs(L[i]-prev)); prev = L[i]; }
        }
        std::printf("max sample step with param jumps: %.3f (220 Hz sine @0.2 normal step ~0.006) %s\n", maxd, maxd < 0.15 ? "OK" : "CHECK");
    }
    std::printf("\n%s\n", fails ? "SOME CHECKS FAILED" : "ALL OK");
    return fails;
}
