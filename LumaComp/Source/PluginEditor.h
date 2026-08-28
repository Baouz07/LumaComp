// PluginEditor.h — LumaComp GUI (graphical redesign: needle GR meters,
// separate saturation color selectors, clean layout).
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

#include "PluginProcessor.h"

// ============================================================================
// Shared look & feel: rotary knobs, small pill toggles.
// ============================================================================
class LumaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LumaLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    juce::Colour accent, textColour, dimTextColour, trackColour, fillColour;

    static const juce::Colour bandColour(int band);
    static const juce::Colour bandColourDim(int band);
};

// ============================================================================
// Spectrum analyzer panel (pre/post spectra, band fills, crossover markers).
// ============================================================================
class AnalyzerPanel : public juce::Component
{
public:
    AnalyzerPanel();

    void paint(juce::Graphics&) override;

    float* getPreDb() { return preDb_; }
    float* getPostDb() { return postDb_; }
    int getNumBins() const { return numBins_; }
    void setAnalysis(int numBins, float sampleRate, int fftSize)
    {
        numBins_ = numBins;
        sampleRate_ = sampleRate;
        fftSize_ = fftSize;
    }
    void setSplitMode(bool s) { split_ = s; }
    void setCrossovers(float f1, float f2) { xover1_ = f1; xover2_ = f2; }

private:
    static float freqToX(float freq, int width);
    int binToX(int bin, int width) const
    {
        const float f = (float)bin * sampleRate_ / (float)fftSize_;
        return (int)freqToX(f, width);
    }

    float preDb_[1024];
    float postDb_[1024];
    int numBins_ = 512;
    float sampleRate_ = 48000.0f;
    int fftSize_ = 2048;
    bool split_ = true;
    float xover1_ = 200.0f, xover2_ = 4000.0f;
};

// ============================================================================
// Horizontal gain-reduction meter with scale ticks (0 dB at left).
// ============================================================================
class GrMeter : public juce::Component
{
public:
    GrMeter();

    // grDb is negative; 0 = no reduction.
    void setGr(float grDb);
    void tick();   // UI-side smoothing, call from timer
    float getDisplayValue() const { return value_; }  // used by the meter test

    void paint(juce::Graphics&) override;

private:
    float target_ = 0.0f;
    float value_ = 0.0f;   // smoothed display value (negative dB)
};

// ============================================================================
// One band's control panel: needle GR meter + knob grid + solo/mute/bypass.
// ============================================================================
class BandPanel : public juce::Component
{
public:
    BandPanel(LumaCompAudioProcessor& proc, int bandIndex);

    void resized() override;
    void paint(juce::Graphics&) override;
    void refresh();
    float getGrDisplay() const { return gr_.getDisplayValue(); }  // used by the meter test

private:
    LumaCompAudioProcessor& proc_;
    int band_;

    juce::Label title_;
    juce::ToggleButton solo_, mute_, bypass_;
    GrMeter gr_;

    juce::Slider th_, rat_, att_, rel_, mk_, schp_, sclp_;
    juce::Label thName_, ratName_, attName_, relName_, mkName_, scHpName_, scLpName_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attTh_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRat_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRel_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attMk_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attScHp_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attScLp_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attSolo_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attMute_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attBypass_;

    LumaLookAndFeel lf_;
};

// ============================================================================
// Main editor.
// ============================================================================
class LumaCompAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    LumaCompAudioProcessorEditor(LumaCompAudioProcessor&);
    ~LumaCompAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;  // public so the meter test can drive it directly
    float getDebugGrDisplay(int band) const { return bandPanels_[band]->getGrDisplay(); }  // meter test

private:
    void setColorParam(int index);

    LumaCompAudioProcessor& proc_;
    LumaLookAndFeel lf_;

    // header
    juce::Label title_, subtitle_;
    juce::ToggleButton modeWide_, modeSplit_, bypass_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attBypass_;

    // analyzer
    std::unique_ptr<AnalyzerPanel> analyzer_;

    // band panels
    std::unique_ptr<BandPanel> bandPanels_[3];

    // --- saturation section ------------------------------------------------
    juce::Label satTitle_;
    juce::ToggleButton colorSoft_, colorTube_, colorBright_;
    juce::Slider drive_, toneF_, toneG_, satMix_;
    juce::Label driveName_, toneFName_, toneGName_, satMixName_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attDrive_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attToneF_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attToneG_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attSatMix_;

    // --- output section ------------------------------------------------------
    juce::Label outTitle_;
    juce::Slider in_, lookahead_, mix_, out_;
    juce::Label inName_, lookName_, mixName_, outName_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attIn_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attLook_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attMix_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attOut_;

    int lastMode_ = -1;
    int lastColor_ = -1;
};
