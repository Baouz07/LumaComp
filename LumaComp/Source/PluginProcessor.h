// PluginProcessor.h — LumaComp AudioProcessor (JUCE glue).
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/Dsp.h"

// ============================================================================
class LumaCompAudioProcessor : public juce::AudioProcessor
{
public:
    LumaCompAudioProcessor();
    ~LumaCompAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "LumaComp"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    static constexpr int numBands = 3;
    static constexpr int maxBlockSize = 8192;

    // --- shared state consumed by the UI -----------------------------------
    struct Meters
    {
        std::atomic<float> inLevel{ 0.0f };
        std::atomic<float> outLevel{ 0.0f };
        std::atomic<float> bandGr[numBands]{ {0.0f}, {0.0f}, {0.0f} };
    } meters;

    lumacomp::SpectrumAnalyzer analyzerPre, analyzerPost;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // parameter pointers (atomic<float> — real-time safe)
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pInput = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pBypass = nullptr;
    std::atomic<float>* pLookahead = nullptr;
    std::atomic<float>* pXov1 = nullptr;
    std::atomic<float>* pXov2 = nullptr;
    std::atomic<float>* pTh[numBands] = {};
    std::atomic<float>* pRat[numBands] = {};
    std::atomic<float>* pAtt[numBands] = {};
    std::atomic<float>* pRel[numBands] = {};
    std::atomic<float>* pMk[numBands] = {};
    std::atomic<float>* pScHp[numBands] = {};
    std::atomic<float>* pScLp[numBands] = {};
    std::atomic<float>* pBandBypass[numBands] = {};
    std::atomic<float>* pSolo[numBands] = {};
    std::atomic<float>* pMute[numBands] = {};
    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pColor = nullptr;
    std::atomic<float>* pToneFreq = nullptr;
    std::atomic<float>* pToneGain = nullptr;
    std::atomic<float>* pSatMix = nullptr;

    // DSP
    lumacomp::ThreeWaySplit splitL_, splitR_;          // per-channel crossover
    lumacomp::BandCompressor bands_[numBands];
    lumacomp::SaturationStage sat_;
    lumacomp::SoftLimiter limiterL_, limiterR_;
    lumacomp::LevelMeter inMeter_, outMeter_;

    float sampleRate_ = 48000.0f;
    int lookaheadMaxSamples_ = 0;

    // scratch for analyzer taps (pre/post), fixed-size
    std::vector<float> preBuf_[2], postBuf_[2];
    bool isPrepared_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LumaCompAudioProcessor)
};
