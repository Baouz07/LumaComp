// Dsp.h — LumaComp DSP core.
// Pure C++ (no JUCE dependency), real-time safe: no heap allocation, no locks,
// no exceptions inside the audio thread. All state is preallocated members.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

namespace lumacomp {

constexpr float kPi       = 3.14159265358979323846f;
constexpr float kTwoPi    = 6.28318530717958647692f;
constexpr float kMinLevel = 1e-9f;

inline float dbToLin(float db) { return std::pow(10.0f, db * 0.05f); }
inline float linToDb(float lin) { return 20.0f * std::log10(std::max(lin, kMinLevel)); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// One-pole smoothing coefficient for a time constant in seconds.
inline float timeConstCoeff(float seconds, float sr)
{
    return seconds <= 0.0f ? 1.0f : 1.0f - std::exp(-1.0f / (seconds * sr));
}

// ---------------------------------------------------------------------------
// Biquad (RBJ cookbook): LPF / HPF / high-shelf.
// ---------------------------------------------------------------------------
class Biquad
{
public:
    void setLowPass(float freq, float sr, float q = 0.70710678f)
    {
        const float w0 = kTwoPi * freq / sr;
        const float cs = std::cos(w0);
        const float sn = std::sin(w0);
        const float alpha = sn / (2.0f * q);
        const float b0 = (1.0f - cs) * 0.5f, b1 = 1.0f - cs, b2 = (1.0f - cs) * 0.5f;
        const float a0 = 1.0f + alpha, a1 = -2.0f * cs, a2 = 1.0f - alpha;
        setRaw(b0, b1, b2, a1, a2, a0);
    }

    void setHighPass(float freq, float sr, float q = 0.70710678f)
    {
        const float w0 = kTwoPi * freq / sr;
        const float cs = std::cos(w0);
        const float sn = std::sin(w0);
        const float alpha = sn / (2.0f * q);
        const float b0 = (1.0f + cs) * 0.5f, b1 = -(1.0f + cs), b2 = (1.0f + cs) * 0.5f;
        const float a0 = 1.0f + alpha, a1 = -2.0f * cs, a2 = 1.0f - alpha;
        setRaw(b0, b1, b2, a1, a2, a0);
    }

    void setHighShelf(float freq, float sr, float gainDb, float s = 1.0f)
    {
        const float A = std::pow(10.0f, gainDb * 0.025f);   // sqrt(linear gain)
        const float w0 = kTwoPi * freq / sr;
        const float cs = std::cos(w0);
        const float sn = std::sin(w0);
        const float alpha = sn * 0.5f * std::sqrt((A + 1.0f / A) * (1.0f / s - 1.0f) + 2.0f);
        const float twoSqrtA = 2.0f * std::sqrt(A);
        const float b0 = A * ((A + 1.0f) + (A - 1.0f) * cs + twoSqrtA * alpha);
        const float b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cs);
        const float b2 = A * ((A + 1.0f) + (A - 1.0f) * cs - twoSqrtA * alpha);
        const float a0 = (A + 1.0f) - (A - 1.0f) * cs + twoSqrtA * alpha;
        const float a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cs);
        const float a2 = (A + 1.0f) - (A - 1.0f) * cs - twoSqrtA * alpha;
        setRaw(b0, b1, b2, a1, a2, a0);
    }

    void setLowShelf(float freq, float sr, float gainDb, float s = 1.0f)
    {
        const float A = std::pow(10.0f, gainDb * 0.025f);
        const float w0 = kTwoPi * freq / sr;
        const float cs = std::cos(w0);
        const float sn = std::sin(w0);
        const float alpha = sn * 0.5f * std::sqrt((A + 1.0f / A) * (1.0f / s - 1.0f) + 2.0f);
        const float twoSqrtA = 2.0f * std::sqrt(A);
        const float b0 = A * ((A + 1.0f) - (A - 1.0f) * cs + twoSqrtA * alpha);
        const float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cs);
        const float b2 = A * ((A + 1.0f) - (A - 1.0f) * cs - twoSqrtA * alpha);
        const float a0 = (A + 1.0f) + (A - 1.0f) * cs + twoSqrtA * alpha;
        const float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cs);
        const float a2 = (A + 1.0f) + (A - 1.0f) * cs - twoSqrtA * alpha;
        setRaw(b0, b1, b2, a1, a2, a0);
    }

    inline float process(float x)
    {
        const float y = b0_ * x + b1_ * x1_ + b2_ * x2_ - a1_ * y1_ - a2_ * y2_;
        x2_ = x1_; x1_ = x;
        y2_ = y1_; y1_ = y;
        return y;
    }

    void reset() { x1_ = x2_ = y1_ = y2_ = 0.0f; }

private:
    void setRaw(float b0, float b1, float b2, float a1, float a2, float a0)
    {
        const float ia0 = 1.0f / a0;
        b0_ = b0 * ia0; b1_ = b1 * ia0; b2_ = b2 * ia0;
        a1_ = a1 * ia0; a2_ = a2 * ia0;
    }

    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f, a1_ = 0.0f, a2_ = 0.0f;
    float x1_ = 0.0f, x2_ = 0.0f, y1_ = 0.0f, y2_ = 0.0f;
};

// ---------------------------------------------------------------------------
// Linkwitz-Riley 4th order section (cascade of two Butterworth biquads).
// ---------------------------------------------------------------------------
class LR4Section
{
public:
    void setLowPass(float freq, float sr) { a_.setLowPass(freq, sr); b_.setLowPass(freq, sr); }
    void setHighPass(float freq, float sr) { a_.setHighPass(freq, sr); b_.setHighPass(freq, sr); }
    inline float process(float x) { return b_.process(a_.process(x)); }
    void reset() { a_.reset(); b_.reset(); }

private:
    Biquad a_, b_;
};

// 3-way Linkwitz-Riley crossover: low = LP(f1), mid = HP(f1)->LP(f2), high = HP(f2).
class ThreeWaySplit
{
public:
    void setCrossovers(float f1, float f2, float sr)
    {
        low_.setLowPass(f1, sr);
        midHp_.setHighPass(f1, sr);
        midLp_.setLowPass(f2, sr);
        high_.setHighPass(f2, sr);
    }

    inline void process(float x, float& low, float& mid, float& high)
    {
        low = low_.process(x);
        mid = midLp_.process(midHp_.process(x));
        high = high_.process(x);
    }

    void reset() { low_.reset(); midHp_.reset(); midLp_.reset(); high_.reset(); }

private:
    LR4Section low_, midHp_, midLp_, high_;
};

// ---------------------------------------------------------------------------
// Fractional-free circular delay line (used for lookahead).
// ---------------------------------------------------------------------------
class DelayLine
{
public:
    void prepare(int maxSamples)
    {
        if ((int)buf_.size() < maxSamples + 1)
            buf_.assign((size_t)maxSamples + 1, 0.0f);
        reset();
    }

    void reset()
    {
        idx_ = 0;
        delay_ = 0;
        std::fill(buf_.begin(), buf_.end(), 0.0f);
    }

    void setDelaySamples(int n) { delay_ = clampf(n, 0, (int)buf_.size() - 1); }

    inline float process(float x)
    {
        buf_[idx_] = x;
        int read = idx_ - delay_;
        if (read < 0) read += (int)buf_.size();
        const float y = buf_[read];
        if (++idx_ >= (int)buf_.size()) idx_ = 0;
        return y;
    }

private:
    std::vector<float> buf_;
    int idx_ = 0, delay_ = 0;
};

// ---------------------------------------------------------------------------
// Stereo band compressor with sidechain avoidance filters (HP/LP "ducking
// avoidance") and optional lookahead. Detector is mono-linked (L+R)/2.
// ---------------------------------------------------------------------------
struct CompressorParams
{
    float thresholdDb = 0.0f;
    float ratio       = 1.0f;
    float attackMs    = 20.0f;
    float releaseMs   = 200.0f;
    float kneeDb      = 6.0f;
    float makeupDb    = 0.0f;
    float scHpHz      = 20.0f;    // sidechain high-pass  (low-frequency avoidance)
    float scLpHz      = 20000.0f; // sidechain low-pass   (high-frequency avoidance)
    float lookaheadMs = 0.0f;
};

class BandCompressor
{
public:
    void prepare(float sampleRate, int lookaheadMaxSamples)
    {
        sr_ = sampleRate;
        dlL_.prepare(lookaheadMaxSamples);
        dlR_.prepare(lookaheadMaxSamples);
        reset();
    }

    void reset()
    {
        scHp_.reset();
        scLp_.reset();
        dlL_.reset();
        dlR_.reset();
        levelSq_ = 0.0f;
        grDb_ = 0.0f;
    }

    void setParams(const CompressorParams& p)
    {
        p_ = p;
        scHp_.setHighPass(p.scHpHz, sr_);
        scLp_.setLowPass(p.scLpHz, sr_);
        dlL_.setDelaySamples((int)(p.lookaheadMs * 0.001f * sr_ + 0.5f));
        dlR_.setDelaySamples((int)(p.lookaheadMs * 0.001f * sr_ + 0.5f));
        envCoeff_ = timeConstCoeff(0.010f, sr_);          // fixed 10 ms detection envelope
        attCoeff_ = timeConstCoeff(p.attackMs * 0.001f, sr_);
        relCoeff_ = timeConstCoeff(p.releaseMs * 0.001f, sr_);
        makeLin_ = dbToLin(p.makeupDb);
        invRatio_ = 1.0f / std::max(p.ratio, 1.0f);
    }

    // Process a stereo pair. Returns current (smoothed) gain reduction in dB.
    inline float process(float l, float r, float& outL, float& outR)
    {
        // mono-linked detection signal, then the avoidance filters
        const float sc = 0.5f * (l + r);
        const float scf = scLp_.process(scHp_.process(sc));
        const float x2 = scf * scf;
        levelSq_ += (x2 - levelSq_) * envCoeff_;

        const float levelDb = 10.0f * std::log10(levelSq_ + 1e-12f);

        // soft-knee gain computer
        float grTarget = 0.0f;
        const float over = levelDb - p_.thresholdDb;
        if (p_.kneeDb > 0.0001f)
        {
            const float k2 = p_.kneeDb * 0.5f;
            if (over > k2)
                grTarget = (over - k2) * (1.0f - invRatio_);
            else if (over > -k2)
            {
                const float w = (over + k2) / p_.kneeDb;   // 0..1 within knee
                grTarget = p_.kneeDb * 0.5f * w * w * (1.0f - invRatio_);
            }
        }
        else if (over > 0.0f)
        {
            grTarget = over * (1.0f - invRatio_);
        }

        // smooth the gain reduction with attack/release
        grDb_ += (grTarget - grDb_) * (grTarget < grDb_ ? attCoeff_ : relCoeff_);

        const float gain = makeLin_ * dbToLin(-grDb_);
        outL = dlL_.process(l) * gain;
        outR = dlR_.process(r) * gain;
        return grDb_;
    }

    float getGainReductionDb() const { return grDb_; }

private:
    float sr_ = 48000.0f;
    CompressorParams p_;
    Biquad scHp_, scLp_;
    DelayLine dlL_, dlR_;
    float levelSq_ = 0.0f, grDb_ = 0.0f;
    float envCoeff_ = 0.01f, attCoeff_ = 0.1f, relCoeff_ = 0.01f, makeLin_ = 1.0f;
    float invRatio_ = 1.0f;
};

// ---------------------------------------------------------------------------
// Harmonic coloration (saturation) stage: waveshaper + high-shelf "air" +
// DC blocker (tube mode) + wet/dry blend.
// ---------------------------------------------------------------------------
struct SaturationParams
{
    float driveDb   = 0.0f;
    int   mode      = 0;   // 0 Soft, 1 Tube, 2 Bright
    float toneFreqHz = 8000.0f;
    float toneGainDb = 0.0f;
    float mix       = 0.0f; // 0..1
};

class SaturationStage
{
public:
    void prepare(float sr) { sr_ = sr; reset(); }

    void reset()
    {
        shelfL_.reset(); shelfR_.reset();
        dcL_.reset(); dcR_.reset();
    }

    void setParams(const SaturationParams& p)
    {
        p_ = p;
        driveLin_ = dbToLin(p.driveDb);
        shelfL_.setHighShelf(p.toneFreqHz, sr_, p.toneGainDb);
        shelfR_.setHighShelf(p.toneFreqHz, sr_, p.toneGainDb);
        dcL_.setHighPass(25.0f, sr_, 0.70710678f);
        dcR_.setHighPass(25.0f, sr_, 0.70710678f);
    }

    inline void process(float l, float r, float& ol, float& or_)
    {
        const float g = driveLin_;
        float yl, yr;

        switch (p_.mode)
        {
        case 1: // Tube: even harmonics
        {
            const float tl = std::tanh(g * l);
            const float tr = std::tanh(g * r);
            yl = tl + 0.25f * tl * tl;
            yr = tr + 0.25f * tr * tr;
            yl = dcL_.process(yl);
            yr = dcR_.process(yr);
            break;
        }
        case 2: // Bright: extra 3rd harmonics (sparkle)
        {
            const float gl = g * l, gr = g * r;
            yl = std::tanh(gl + 0.30f * gl * gl * gl);
            yr = std::tanh(gr + 0.30f * gr * gr * gr);
            break;
        }
        default: // Soft
            yl = std::tanh(g * l);
            yr = std::tanh(g * r);
            break;
        }

        yl = shelfL_.process(yl);
        yr = shelfR_.process(yr);

        ol = l + p_.mix * (yl - l);
        or_ = r + p_.mix * (yr - r);
    }

private:
    float sr_ = 48000.0f;
    SaturationParams p_;
    float driveLin_ = 1.0f;
    Biquad shelfL_, shelfR_, dcL_, dcR_;
};

// ---------------------------------------------------------------------------
// Safety soft limiter (transparent below -1 dBFS).
// ---------------------------------------------------------------------------
class SoftLimiter
{
public:
    inline float process(float x)
    {
        const float t = 0.9f;
        const float ax = std::fabs(x);
        if (ax <= t) return x;
        const float y = t + (1.0f - t) * std::tanh((ax - t) / (1.0f - t));
        return std::copysign(y, x);
    }
};

// ---------------------------------------------------------------------------
// Level meter (RMS-style envelope + peak hold).
// ---------------------------------------------------------------------------
class LevelMeter
{
public:
    void prepare(float sr, float tauMs = 200.0f)
    {
        coeff_ = timeConstCoeff(tauMs * 0.001f, sr);
        reset();
    }

    void reset() { env_ = 0.0f; peak_ = 0.0f; }

    inline void process(float l, float r)
    {
        const float m = std::max(std::fabs(l), std::fabs(r));
        env_ += (m - env_) * coeff_;
        peak_ = std::max(peak_, m);
    }

    float getEnv() const { return env_; }
    float getPeak() const { return peak_; }
    void clearPeak() { peak_ = 0.0f; }

private:
    float coeff_ = 0.01f, env_ = 0.0f, peak_ = 0.0f;
};

// ---------------------------------------------------------------------------
// Radix-2 FFT (iterative, in-place on separate real/imag arrays).
// ---------------------------------------------------------------------------
class FFT
{
public:
    explicit FFT(int size)
        : n_(size), bits_(0)
    {
        while ((1 << bits_) < n_) ++bits_;
        rev_.resize((size_t)n_);
        for (int i = 0; i < n_; ++i) rev_[i] = reverseBits(i);
    }

    void perform(float* re, float* im)
    {
        for (int i = 0; i < n_; ++i)
        {
            const int j = rev_[i];
            if (j > i)
            {
                std::swap(re[i], re[j]);
                std::swap(im[i], im[j]);
            }
        }
        for (int len = 2; len <= n_; len <<= 1)
        {
            const float ang = -kTwoPi / (float)len;
            const float wpr = std::cos(ang), wpi = std::sin(ang);
            const int half = len >> 1;
            for (int i = 0; i < n_; i += len)
            {
                float wr = 1.0f, wi = 0.0f;
                for (int k = 0; k < half; ++k)
                {
                    const float ur = re[i + k], ui = im[i + k];
                    const float vr = re[i + k + half] * wr - im[i + k + half] * wi;
                    const float vi = re[i + k + half] * wi + im[i + k + half] * wr;
                    re[i + k] = ur + vr;       im[i + k] = ui + vi;
                    re[i + k + half] = ur - vr; im[i + k + half] = ui - vi;
                    const float nwr = wr * wpr - wi * wpi;
                    wi = wr * wpi + wi * wpr;
                    wr = nwr;
                }
            }
        }
    }

private:
    int reverseBits(int x) const
    {
        int r = 0;
        for (int i = 0; i < bits_; ++i) { r = (r << 1) | (x & 1); x >>= 1; }
        return r;
    }

    int n_, bits_;
    std::vector<int> rev_;
};

// ---------------------------------------------------------------------------
// Spectrum analyzer: lock-free single-producer/single-consumer ring per
// channel; FFT computed on the UI thread from a mixed snapshot.
// ---------------------------------------------------------------------------
class SpectrumAnalyzer
{
public:
    void prepare(int numChannels, int fftSize, float sr)
    {
        fftSize_ = fftSize;
        sr_ = sr;
        numChannels_ = std::max(1, std::min(numChannels, 2));
        window_.assign((size_t)fftSize, 0.0f);
        for (int i = 0; i < fftSize; ++i)
            window_[(size_t)i] = 0.5f - 0.5f * std::cos(kTwoPi * (float)i / (float)(fftSize - 1));
        for (auto& c : channels_)
        {
            c.ring.assign((size_t)fftSize, 0.0f);
            c.writePos.store(0, std::memory_order_relaxed);
        }
        fft_ = std::make_unique<FFT>(fftSize);
        re_.assign((size_t)fftSize, 0.0f);
        im_.assign((size_t)fftSize, 0.0f);
    }

    void reset()
    {
        for (int c = 0; c < numChannels_; ++c)
        {
            std::fill(channels_[(size_t)c].ring.begin(), channels_[(size_t)c].ring.end(), 0.0f);
            channels_[(size_t)c].writePos.store(0, std::memory_order_relaxed);
        }
    }

    // ---- audio thread ----
    void push(const float* const* data, int numChannels, int numSamples)
    {
        const int n = std::min(numChannels, numChannels_);
        for (int c = 0; c < n; ++c)
        {
            auto& ch = channels_[(size_t)c];
            int wp = ch.writePos.load(std::memory_order_relaxed);
            const float* src = data[c];
            for (int i = 0; i < numSamples; ++i)
            {
                ch.ring[(size_t)wp] = src[i];
                if (++wp >= fftSize_) wp = 0;
            }
            ch.writePos.store(wp, std::memory_order_relaxed);
        }
    }

    // ---- UI thread ----
    void computeSpectrum(float* outDb, int numBins)
    {
        const int n = numChannels_;
        for (int i = 0; i < fftSize_; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < n; ++c)
            {
                auto& ch = channels_[(size_t)c];
                int idx = ch.writePos.load(std::memory_order_relaxed) + i;
                if (idx >= fftSize_) idx -= fftSize_;
                s += ch.ring[(size_t)idx];
            }
            s /= (float)n;
            re_[(size_t)i] = s * window_[(size_t)i];
            im_[(size_t)i] = 0.0f;
        }
        fft_->perform(re_.data(), im_.data());
        const int bins = std::min(numBins, fftSize_ / 2);
        const float norm = 1.0f / (float)fftSize_;
        for (int b = 0; b < bins; ++b)
        {
            const float mag = std::sqrt(re_[(size_t)b] * re_[(size_t)b] + im_[(size_t)b] * im_[(size_t)b]);
            outDb[b] = linToDb(mag * norm);
        }
    }

    float getFreqForBin(int bin) const { return (float)bin * sr_ / (float)fftSize_; }
    int getSize() const { return fftSize_; }

private:
    struct Channel
    {
        std::vector<float> ring;
        std::atomic<int> writePos{ 0 };
    };

    int fftSize_ = 2048;
    float sr_ = 48000.0f;
    int numChannels_ = 2;
    std::vector<float> window_, re_, im_;
    std::array<Channel, 2> channels_;
    std::unique_ptr<FFT> fft_;
};

} // namespace lumacomp
