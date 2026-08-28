// PluginProcessor.cpp — LumaComp audio processing implementation.
#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cstring>

#include <juce_dsp/juce_dsp.h>

namespace
{
using juce::AudioParameterBool;
using juce::AudioParameterChoice;
using juce::AudioParameterFloat;

juce::NormalisableRange<float> dbRange(float lo, float hi, float step = 0.1f)
{
    return { lo, hi, step };
}

juce::NormalisableRange<float> logRange(float lo, float hi, float skew = 0.3f)
{
    return { lo, hi, 0.01f, skew };
}

juce::StringArray bandNames()
{
    return { "Low", "Mid", "High" };
}
} // namespace

// ============================================================================
LumaCompAudioProcessor::LumaCompAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "LumaComp", createParameterLayout())
{
    pMode       = apvts.getRawParameterValue("mode");
    pInput      = apvts.getRawParameterValue("input");
    pOutput     = apvts.getRawParameterValue("output");
    pMix        = apvts.getRawParameterValue("mix");
    pBypass     = apvts.getRawParameterValue("bypass");
    pLookahead  = apvts.getRawParameterValue("lookahead");
    pXov1       = apvts.getRawParameterValue("xov1");
    pXov2       = apvts.getRawParameterValue("xov2");

    for (int b = 0; b < numBands; ++b)
    {
        const juce::String s = juce::String(b);
        pTh[b]        = apvts.getRawParameterValue("th" + s);
        pRat[b]       = apvts.getRawParameterValue("rat" + s);
        pAtt[b]       = apvts.getRawParameterValue("att" + s);
        pRel[b]       = apvts.getRawParameterValue("rel" + s);
        pMk[b]        = apvts.getRawParameterValue("mk" + s);
        pScHp[b]      = apvts.getRawParameterValue("schp" + s);
        pScLp[b]      = apvts.getRawParameterValue("sclp" + s);
        pBandBypass[b] = apvts.getRawParameterValue("sb" + s);
        pSolo[b]      = apvts.getRawParameterValue("solo" + s);
        pMute[b]      = apvts.getRawParameterValue("mut" + s);
    }

    pDrive     = apvts.getRawParameterValue("drive");
    pColor     = apvts.getRawParameterValue("color");
    pToneFreq  = apvts.getRawParameterValue("tonef");
    pToneGain  = apvts.getRawParameterValue("toneg");
    pSatMix    = apvts.getRawParameterValue("satmix");
}

juce::AudioProcessorValueTreeState::ParameterLayout
LumaCompAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<AudioParameterChoice>("mode", "Mode",
                juce::StringArray{ "Wide", "Split" }, 0));
    layout.add(std::make_unique<AudioParameterFloat>("input", "Input",
                dbRange(-24.0f, 24.0f), 0.0f));
    layout.add(std::make_unique<AudioParameterFloat>("output", "Output",
                dbRange(-24.0f, 24.0f), 0.0f));
    layout.add(std::make_unique<AudioParameterFloat>("mix", "Mix",
                dbRange(0.0f, 100.0f, 1.0f), 100.0f));
    layout.add(std::make_unique<AudioParameterBool>("bypass", "Bypass", false));
    layout.add(std::make_unique<AudioParameterFloat>("lookahead", "Lookahead",
                dbRange(0.0f, 10.0f, 0.1f), 0.0f));

    layout.add(std::make_unique<AudioParameterFloat>("xov1", "X-Over 1",
                logRange(30.0f, 2000.0f), 200.0f));
    layout.add(std::make_unique<AudioParameterFloat>("xov2", "X-Over 2",
                logRange(500.0f, 20000.0f), 4000.0f));

    for (int b = 0; b < numBands; ++b)
    {
        const juce::String s = juce::String(b);
        const juce::String name = bandNames()[b] + " ";

        layout.add(std::make_unique<AudioParameterFloat>("th" + s, name + "Threshold",
                    dbRange(-60.0f, 0.0f), -18.0f));
        layout.add(std::make_unique<AudioParameterFloat>("rat" + s, name + "Ratio",
                    dbRange(1.0f, 20.0f, 0.1f), 4.0f));
        layout.add(std::make_unique<AudioParameterFloat>("att" + s, name + "Attack",
                    logRange(0.1f, 300.0f), 15.0f));
        layout.add(std::make_unique<AudioParameterFloat>("rel" + s, name + "Release",
                    logRange(10.0f, 3000.0f), 150.0f));
        layout.add(std::make_unique<AudioParameterFloat>("mk" + s, name + "Makeup",
                    dbRange(0.0f, 24.0f), 0.0f));
        layout.add(std::make_unique<AudioParameterFloat>("schp" + s, name + "SC High-Pass",
                    logRange(20.0f, 1000.0f), 20.0f));
        layout.add(std::make_unique<AudioParameterFloat>("sclp" + s, name + "SC Low-Pass",
                    logRange(500.0f, 20000.0f), 20000.0f));
        layout.add(std::make_unique<AudioParameterBool>("sb" + s, name + "Bypass", false));
        layout.add(std::make_unique<AudioParameterBool>("solo" + s, name + "Solo", false));
        layout.add(std::make_unique<AudioParameterBool>("mut" + s, name + "Mute", false));
    }

    layout.add(std::make_unique<AudioParameterFloat>("drive", "Drive",
                dbRange(0.0f, 24.0f), 0.0f));
    layout.add(std::make_unique<AudioParameterChoice>("color", "Color",
                juce::StringArray{ "Soft", "Tube", "Bright" }, 0));
    layout.add(std::make_unique<AudioParameterFloat>("tonef", "Tone Freq",
                logRange(1000.0f, 20000.0f), 8000.0f));
    layout.add(std::make_unique<AudioParameterFloat>("toneg", "Tone Gain",
                dbRange(-12.0f, 12.0f), 0.0f));
    layout.add(std::make_unique<AudioParameterFloat>("satmix", "Saturation Mix",
                dbRange(0.0f, 100.0f, 1.0f), 50.0f));

    return layout;
}

// ============================================================================
void LumaCompAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    sampleRate_ = (float)sampleRate;
    lookaheadMaxSamples_ = (int)(0.010 * sampleRate) + 2;   // 10 ms max

    splitL_.setCrossovers(200.0f, 4000.0f, (float)sampleRate);
    splitR_.setCrossovers(200.0f, 4000.0f, (float)sampleRate);

    for (auto& band : bands_)
        band.prepare((float)sampleRate, lookaheadMaxSamples_);

    sat_.prepare((float)sampleRate);
    inMeter_.prepare((float)sampleRate);
    outMeter_.prepare((float)sampleRate);

    const int n = juce::jmin(samplesPerBlock, maxBlockSize);
    for (int c = 0; c < 2; ++c)
    {
        preBuf_[c].assign((size_t)n, 0.0f);
        postBuf_[c].assign((size_t)n, 0.0f);
    }

    analyzerPre.prepare(2, 2048, (float)sampleRate);
    analyzerPost.prepare(2, 2048, (float)sampleRate);

    isPrepared_ = true;
}

void LumaCompAudioProcessor::releaseResources()
{
    isPrepared_ = false;
}

// ============================================================================
void LumaCompAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    if (!isPrepared_ || numChannels == 0 || numSamples == 0)
    {
        if (numSamples > 0 && numChannels > 0)
        {
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                buffer.clear(c, 0, numSamples);
        }
        return;
    }

    // ---- pull parameters (atomic, lock-free) ------------------------------
    const bool bypass  = *pBypass > 0.5f;
    const bool split   = *pMode > 0.5f;
    const float inputGain  = lumacomp::dbToLin(*pInput);
    const float outputGain = lumacomp::dbToLin(*pOutput);
    const float mix        = lumacomp::clampf(*pMix * 0.01f, 0.0f, 1.0f);
    const float lookaheadMs = lumacomp::clampf(*pLookahead, 0.0f, 10.0f);

    lumacomp::CompressorParams cp[numBands];
    bool bandActive[numBands];
    bool solo[numBands], mute[numBands];
    bool soloAny = false;

    for (int b = 0; b < numBands; ++b)
    {
        cp[b].thresholdDb  = *pTh[b];
        cp[b].ratio        = *pRat[b];
        cp[b].attackMs     = *pAtt[b];
        cp[b].releaseMs    = *pRel[b];
        cp[b].makeupDb     = *pMk[b];
        cp[b].scHpHz       = *pScHp[b];
        cp[b].scLpHz       = *pScLp[b];
        cp[b].lookaheadMs  = lookaheadMs;
        cp[b].kneeDb       = 6.0f;
        bandActive[b]      = *pBandBypass[b] < 0.5f;
        solo[b]            = *pSolo[b] > 0.5f;
        mute[b]            = *pMute[b] > 0.5f;
        soloAny |= solo[b];
        bands_[b].setParams(cp[b]);
    }

    lumacomp::SaturationParams sp;
    sp.driveDb    = *pDrive;
    sp.mode       = juce::roundToInt(*pColor * 2.0f);   // choice 0..2
    sp.toneFreqHz = *pToneFreq;
    sp.toneGainDb = *pToneGain;
    sp.mix        = lumacomp::clampf(*pSatMix * 0.01f, 0.0f, 1.0f);
    sat_.setParams(sp);

    const float f1 = lumacomp::clampf(*pXov1, 30.0f, 19900.0f);
    const float f2 = lumacomp::clampf(*pXov2, 100.0f, 20000.0f);
    const float x1 = std::min(f1, f2 - 50.0f);
    const float x2 = std::max(f2, f1 + 50.0f);
    if (split)
    {
        splitL_.setCrossovers(x1, x2, sampleRate_);
        splitR_.setCrossovers(x1, x2, sampleRate_);
    }

    // band routing gains (solo/mute)
    float bandGain[numBands];
    for (int b = 0; b < numBands; ++b)
    {
        float g = 1.0f;
        if (soloAny) g = solo[b] ? 1.0f : 0.0f;
        else if (mute[b]) g = 0.0f;
        bandGain[b] = g;
    }

    const int n = juce::jmin(numSamples, maxBlockSize);
    const float* srcCh[2] = { buffer.getReadPointer(0), numChannels > 1 ? buffer.getReadPointer(1) : buffer.getReadPointer(0) };

    for (int i = 0; i < n; ++i)
    {
        const float l = srcCh[0][i] * inputGain;
        const float r = (numChannels > 1 ? srcCh[1][i] : l) * inputGain;

        float dryL = l, dryR = r;
        float wetL, wetR;

        if (bypass)
        {
            wetL = l; wetR = r;
        }
        else
        {
            if (split)
            {
                float ll, lm, lh, rl, rm, rh;
                splitL_.process(l, ll, lm, lh);
                splitR_.process(r, rl, rm, rh);
                float ol, om, oh, orl, orm, orh;
                bands_[0].process(ll, rl, ol, orl);
                bands_[1].process(lm, rm, om, orm);
                bands_[2].process(lh, rh, oh, orh);
                wetL = ol * bandGain[0] + om * bandGain[1] + oh * bandGain[2];
                wetR = orl * bandGain[0] + orm * bandGain[1] + orh * bandGain[2];
            }
            else
            {
                // Wide mode: single full-band compressor (band 1 controls)
                float ol, or_;
                bands_[1].process(l, r, ol, or_);
                wetL = ol * bandGain[1];
                wetR = or_ * bandGain[1];
            }

            // saturation
            float satL, satR;
            sat_.process(wetL, wetR, satL, satR);

            // dry/wet (parallel)
            wetL = dryL * (1.0f - mix) + satL * mix;
            wetR = dryR * (1.0f - mix) + satR * mix;

            // output gain + safety limiter
            wetL = limiterL_.process(wetL * outputGain);
            wetR = limiterR_.process(wetR * outputGain);
        }

        preBuf_[0][i] = l;   preBuf_[1][i] = r;
        postBuf_[0][i] = wetL; postBuf_[1][i] = wetR;

        inMeter_.process(l, r);
        outMeter_.process(wetL, wetR);
    }

    // write back the processed audio
    for (int c = 0; c < numChannels; ++c)
    {
        const float* post = postBuf_[c].data();
        float* dst = buffer.getWritePointer(c);
        std::memcpy(dst, post, sizeof(float) * (size_t)n);
        if (n < numSamples)
            buffer.clear(c, n, numSamples - n);
    }

    // analyzer taps
    const float* preCh[2] = { preBuf_[0].data(), preBuf_[1].data() };
    const float* postCh[2] = { postBuf_[0].data(), postBuf_[1].data() };
    analyzerPre.push(preCh, numChannels, n);
    analyzerPost.push(postCh, numChannels, n);

    // meters (GR stored as negative dB: -4.5 = 4.5 dB of reduction)
    meters.inLevel.store(inMeter_.getEnv(), std::memory_order_relaxed);
    meters.outLevel.store(outMeter_.getEnv(), std::memory_order_relaxed);
    if (split)
    {
        for (int b = 0; b < numBands; ++b)
            meters.bandGr[b].store(-bands_[b].getGainReductionDb(), std::memory_order_relaxed);
    }
    else
    {
        meters.bandGr[1].store(-bands_[1].getGainReductionDb(), std::memory_order_relaxed);
        meters.bandGr[0].store(0.0f, std::memory_order_relaxed);
        meters.bandGr[2].store(0.0f, std::memory_order_relaxed);
    }

    // latency for lookahead
    setLatencySamples((int)(lookaheadMs * 0.001f * sampleRate_ + 0.5f));
}

// ============================================================================
void LumaCompAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void LumaCompAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

// ============================================================================
juce::AudioProcessorEditor* LumaCompAudioProcessor::createEditor()
{
    return new LumaCompAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LumaCompAudioProcessor();
}
