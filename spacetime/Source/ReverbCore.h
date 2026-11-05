#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// 8-line Feedback Delay Network (Householder), входная диффузия (allpass),
// модуляция линий, затухание по Jot (RT60 на НЧ + демпфирование ВЧ).
// Без зависимостей от JUCE. Вся память выделяется в prepare().
namespace spacetime
{
class ReverbCore
{
public:
    static constexpr int N = 8;
    static constexpr int blockSize = 32;

    void prepare (double sampleRate)
    {
        sr = std::max(8000.0, sampleRate);
        const int preMax = (int) std::ceil(0.5 * sr) + 8;
        for (auto& l : pre) l.init(preMax);

        for (int k = 0; k < 4; ++k)
        {
            ap[0][k].init(std::max(2, (int) std::lround(apMsL[k] * sr / 1000.0)), 0.62f);
            ap[1][k].init(std::max(2, (int) std::lround(apMsR[k] * sr / 1000.0)), 0.62f);
        }

        const double maxMs = baseMs[N - 1] * scales[5] + 5.0;
        for (int i = 0; i < N; ++i)
        {
            lines[i].init((int) std::ceil(maxMs * sr / 1000.0) + 8);
            lfoPhase[i] = (float) i / (float) N;
            lfoInc[i] = (float) (lfoHz[i] / sr);
        }
        modDepth = (float) (0.30 * sr / 1000.0);
        lenCoef = (float) (1.0 - std::exp(-1.0 / (sr * 0.20)));
        preCoef = (float) (1.0 - std::exp(-1.0 / (sr * 0.03)));
        mixCoef = (float) (1.0 - std::exp(-1.0 / (sr * 0.02)));
        hpCoef = (float) (1.0 - std::exp(-2.0 * 3.14159265358979 * 30.0 / sr));

        for (int i = 0; i < N; ++i)
            curDelay[i] = tgtDelay[i] = (float) (baseMs[i] * scales[sizeIdx] * sr / 1000.0);
        preCur = preTarget = 15.0f * (float) sr / 1000.0f;
        logDecay = std::log(decayTarget);
        dampCur = dampTarget;
        widthCur = widthTarget;
        mixCur = mixTarget;
        reset();
        updateBlock(blockSize);
    }

    void reset()
    {
        for (auto& l : pre) l.clear();
        for (auto& c : ap) for (auto& a : c) a.clear();
        for (auto& l : lines) l.clear();
        lp.fill(0.0f);
        hp[0] = hp[1] = 0.0f;
    }

    // preDelayMs, decayMs (RT60 на низах), damping 0..1, width 0..1, mix 0..1, sizeIndex 0..5
    void setParameters (float preDelayMs, float decayMs, float damping, float width, float mix, int sizeIndex)
    {
        preTarget = std::min(500.0f, std::max(0.0f, preDelayMs)) * (float) sr / 1000.0f;
        decayTarget = std::min(30.0f, std::max(0.02f, decayMs * 0.001f));
        dampTarget = std::min(1.0f, std::max(0.0f, damping));
        widthTarget = std::min(1.0f, std::max(0.0f, width));
        mixTarget = std::min(1.0f, std::max(0.0f, mix));
        sizeIdx = std::min(5, std::max(0, sizeIndex));
        for (int i = 0; i < N; ++i)
            tgtDelay[i] = (float) (baseMs[i] * scales[sizeIdx] * sr / 1000.0);
    }

    // right может быть nullptr (моно)
    void process (float* left, float* right, int numSamples)
    {
        int pos = 0;
        while (pos < numSamples)
        {
            const int n = std::min(blockSize, numSamples - pos);
            updateBlock(n);

            for (int s = 0; s < n; ++s)
            {
                const float dryL = left[pos + s];
                const float dryR = right != nullptr ? right[pos + s] : dryL;

                preCur += (preTarget - preCur) * preCoef;
                const float pd = std::max(1.0f, preCur);
                float xl = pre[0].readFrac(pd);
                float xr = pre[1].readFrac(pd);
                pre[0].push(dryL);
                pre[1].push(dryR);

                for (int k = 0; k < 4; ++k)
                {
                    xl = ap[0][k].process(xl);
                    xr = ap[1][k].process(xr);
                }

                float y[N];
                float sum = 0.0f;
                for (int i = 0; i < N; ++i)
                {
                    curDelay[i] += (tgtDelay[i] - curDelay[i]) * lenCoef;
                    lfoPhase[i] += lfoInc[i];
                    if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
                    const float d = std::max(2.0f, curDelay[i] + fastSin(lfoPhase[i]) * modDepth);
                    const float v = lines[i].readFrac(d);
                    lp[i] = bCoef[i] * v + aCoef[i] * lp[i];
                    y[i] = lp[i];
                    sum += y[i];
                }
                const float fb = sum * (2.0f / (float) N);
                for (int i = 0; i < N; ++i)
                {
                    const float in = inSign[i] * 0.5f * ((i & 1) ? xr : xl);
                    lines[i].push(in + y[i] - fb);
                }

                float wl = 0.0f, wr = 0.0f;
                for (int i = 0; i < N; ++i)
                {
                    wl += (i & 1) ? -y[i] : y[i];
                    wr += (i & 2) ? -y[i] : y[i];
                }
                wl *= 0.35355339f;
                wr *= 0.35355339f;

                widthCur += (widthTarget - widthCur) * mixCoef;
                const float m = 0.5f * (wl + wr);
                const float sd = 0.5f * (wl - wr) * widthCur;
                wl = m + sd;
                wr = m - sd;

                hp[0] += hpCoef * (wl - hp[0]); wl -= hp[0];
                hp[1] += hpCoef * (wr - hp[1]); wr -= hp[1];

                mixCur += (mixTarget - mixCur) * mixCoef;
                const float ang = mixCur * 1.57079633f;
                const float dg = std::cos(ang);
                const float wg = std::sin(ang) * wetGain;

                if (right != nullptr)
                {
                    left[pos + s]  = dryL * dg + wl * wg;
                    right[pos + s] = dryR * dg + wr * wg;
                }
                else
                    left[pos + s] = dryL * dg + m * wg;
            }
            pos += n;
        }
    }

    float getWetGainForTest() const { return wetGain; }
    float wetLevelConstant = 1.8f; // калибровка уровня
    static constexpr float rtCalibration = 1.16f;

private:
    struct Line
    {
        std::vector<float> buf; int mask = 0; int w = 0;
        void init (int minSize) { int s = 16; while (s < minSize) s <<= 1; buf.assign((size_t) s, 0.0f); mask = s - 1; w = 0; }
        void clear() { std::fill(buf.begin(), buf.end(), 0.0f); w = 0; }
        void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
        // delay >= 1 (в сэмплах): 1 = предыдущий записанный сэмпл
        float readFrac (float delay) const
        {
            const int d = (int) delay;
            const float f = delay - (float) d;
            const int i0 = (w - d) & mask;
            const int i1 = (i0 - 1) & mask;
            return buf[(size_t) i0] * (1.0f - f) + buf[(size_t) i1] * f;
        }
    };
    struct Allpass
    {
        std::vector<float> buf; int idx = 0; float g = 0.6f;
        void init (int len, float gain) { buf.assign((size_t) len, 0.0f); idx = 0; g = gain; }
        void clear() { std::fill(buf.begin(), buf.end(), 0.0f); idx = 0; }
        float process (float x)
        {
            const float d = buf[(size_t) idx];
            const float w = x + g * d;
            buf[(size_t) idx] = w;
            if (++idx >= (int) buf.size()) idx = 0;
            return d - g * w;
        }
    };

    static float fastSin (float phase01) // 0..1 -> sin(2*pi*x), параболическое приближение
    {
        float x = phase01 - 0.5f;               // -0.5..0.5
        const float y = 8.0f * x * (1.0f - 2.0f * std::fabs(x)); // ~ -sin(2*pi*phase)
        return -y;
    }

    void updateBlock (int n)
    {
        const float cs = (float) (1.0 - std::exp(-(double) n / (sr * 0.05)));
        logDecay += (std::log(decayTarget) - logDecay) * cs;
        dampCur += (dampTarget - dampCur) * cs;
        const float rtLf = std::exp(logDecay) * rtCalibration; // калибровка по измеренному RT60
        const float rtHf = std::max(0.03f, rtLf * (1.0f - 0.92f * dampCur));

        for (int i = 0; i < N; ++i)
        {
            const float sec = curDelay[i] / (float) sr;
            const float kLow = std::exp(-6.9077553f * sec / rtLf);   // 10^(-3*sec/RT)
            const float kHigh = std::exp(-6.9077553f * sec / rtHf);
            const float r = kLow / kHigh;
            const float a = (r - 1.0f) / (r + 1.0f);
            aCoef[i] = a;
            bCoef[i] = kLow * (1.0f - a);
        }
        // Компенсация громкости: длинный хвост накапливает больше энергии.
        const float comp = std::min(4.0f, std::max(0.3f, std::pow(1.0f / rtLf, 0.45f)));
        wetGain = wetLevelConstant * comp;
    }

    double sr = 44100.0;
    std::array<Line, 2> pre;
    std::array<std::array<Allpass, 4>, 2> ap;
    std::array<Line, N> lines;
    std::array<float, N> lp {}, aCoef {}, bCoef {}, curDelay {}, tgtDelay {}, lfoPhase {}, lfoInc {};
    float hp[2] { 0.0f, 0.0f };

    float preCur = 0, preTarget = 0, logDecay = 0, dampCur = 0.35f, widthCur = 1, mixCur = 0.25f;
    float decayTarget = 1.0f, dampTarget = 0.35f, widthTarget = 1.0f, mixTarget = 0.25f;
    float modDepth = 0, lenCoef = 0, preCoef = 0, mixCoef = 0, hpCoef = 0, wetGain = 1.0f;
    int sizeIdx = 3;

    static constexpr double baseMs[N] { 21.3, 27.7, 33.1, 39.7, 46.3, 53.9, 61.1, 68.9 };
    static constexpr double scales[6] { 0.45, 0.55, 0.70, 0.85, 1.00, 1.25 };
    static constexpr double lfoHz[N] { 0.13, 0.19, 0.23, 0.29, 0.31, 0.37, 0.43, 0.47 };
    static constexpr double apMsL[4] { 4.77, 6.89, 9.97, 13.31 };
    static constexpr double apMsR[4] { 5.13, 7.37, 10.61, 14.07 };
    static constexpr float inSign[N] { 1, 1, -1, 1, -1, -1, 1, -1 };
};
} // namespace spacetime
