// PluginEditor.cpp — LumaComp GUI implementation (graphical redesign).
#include "PluginEditor.h"

#include <cmath>

namespace
{
const juce::Colour kBg(0xFF15181D);
const juce::Colour kPanel(0xFF20242B);
const juce::Colour kTrack(0xFF343B46);
const juce::Colour kText(0xFFE8EBF0);
const juce::Colour kDim(0xFF8A919E);
const juce::Colour kAccent(0xFFF0A84C);
const juce::Colour kCyan(0xFF57D5DE);

const juce::Colour kBandCols[3] = {
    juce::Colour(0xFFF0A84C),   // low   — orange
    juce::Colour(0xFF46C9A6),   // mid   — teal
    juce::Colour(0xFF9A85F0)    // high  — violet
};

void setSliderRangeFromParam(juce::Slider& s, juce::AudioProcessorValueTreeState& vts,
                             const juce::String& id)
{
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(vts.getParameter(id)))
        s.setRange(p->range.start, p->range.end, p->range.interval);
}

void paintPanel(juce::Graphics& g, const juce::Rectangle<int>& bounds,
                const juce::Colour& accentColour, bool fillAccent)
{
    g.setColour(kPanel);
    g.fillRoundedRectangle(bounds.toFloat(), 7.0f);
    if (fillAccent)
    {
        g.setColour(accentColour.withAlpha(0.07f));
        g.fillRoundedRectangle(bounds.toFloat(), 7.0f);
    }
    // top accent strip
    g.setColour(accentColour.withAlpha(0.55f));
    g.fillRoundedRectangle(juce::Rectangle<float>((float)bounds.getX() + 8.0f,
                                                  (float)bounds.getY() + 1.0f,
                                                  (float)bounds.getWidth() - 16.0f, 2.5f), 1.25f);
    g.setColour(juce::Colour(0xFF323842));
    g.drawRoundedRectangle(bounds.toFloat(), 7.0f, 1.0f);
}

juce::Rectangle<int> panelInner(const juce::Rectangle<int>& b, int margin = 8)
{
    return b.reduced(margin);
}
} // namespace

// ============================================================================
// Look & Feel
// ============================================================================
LumaLookAndFeel::LumaLookAndFeel()
    : accent(kAccent), textColour(kText), dimTextColour(kDim), trackColour(kTrack), fillColour(kPanel)
{
}

const juce::Colour LumaLookAndFeel::bandColour(int band)
{
    return kBandCols[band < 0 ? 0 : (band > 2 ? 2 : band)];
}

const juce::Colour LumaLookAndFeel::bandColourDim(int band)
{
    return bandColour(band).withAlpha(0.45f);
}

void LumaLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                                       float pos, float start, float end, juce::Slider& slider)
{
    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f - 2.0f;
    const float radius = juce::jmin(w, h) * 0.5f - 5.0f;
    const juce::Colour col = accent;

    // track
    juce::Path track;
    track.addCentredArc(cx, cy, radius, radius, 0.0f, start, end, true);
    g.setColour(trackColour.withAlpha(0.9f));
    g.strokePath(track, juce::PathStrokeType(3.2f));

    // value arc (with soft glow)
    const float angle = start + pos * (end - start);
    juce::Path value;
    value.addCentredArc(cx, cy, radius, radius, 0.0f, start, angle, true);
    g.setColour(col.withAlpha(0.28f));
    g.strokePath(value, juce::PathStrokeType(6.0f));
    g.setColour(col);
    g.strokePath(value, juce::PathStrokeType(3.2f));

    // indicator dot
    const float ix = cx + radius * std::cos(angle);
    const float iy = cy + radius * std::sin(angle);
    g.setColour(col.brighter(0.4f));
    g.fillEllipse(ix - 3.0f, iy - 3.0f, 6.0f, 6.0f);

    // center cap
    g.setColour(juce::Colour(0xFF2A2F38));
    g.fillEllipse(cx - 5.5f, cy - 5.5f, 11.0f, 11.0f);
    g.setColour(col.withAlpha(0.9f));
    g.drawEllipse(cx - 5.5f, cy - 5.5f, 11.0f, 11.0f, 1.2f);

    // value text
    juce::String txt = slider.getTextFromValue(slider.getValue());
    g.setColour(textColour);
    g.setFont(juce::Font(9.5f).boldened());
    g.drawText(txt, x, y + h - 16, w, 12, juce::Justification::centred, false);
}

void LumaLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
                                       bool, bool)
{
    const bool on = b.getToggleState();
    const auto area = b.getLocalBounds().reduced(1, 2).toFloat();
    g.setColour(on ? accent : trackColour);
    g.fillRoundedRectangle(area, area.getHeight() * 0.5f);
    g.setColour(juce::Colour(0x22000000));
    g.drawRoundedRectangle(area, area.getHeight() * 0.5f, 1.0f);
    g.setColour(on ? juce::Colours::white : dimTextColour);
    g.setFont(juce::Font(juce::jmin(9.0f, b.getHeight() * 0.42f)).boldened());
    g.drawText(b.getButtonText(), area, juce::Justification::centred, false);
}

void LumaLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b,
                                           const juce::Colour&, bool highlighted,
                                           bool down)
{
    const auto area = b.getLocalBounds().toFloat();
    juce::Colour col = b.getToggleState() ? accent : trackColour;
    if (highlighted) col = col.brighter(0.15f);
    if (down) col = col.darker(0.15f);
    g.setColour(col);
    g.fillRoundedRectangle(area, area.getHeight() * 0.5f);
}

// ============================================================================
// AnalyzerPanel
// ============================================================================
AnalyzerPanel::AnalyzerPanel()
{
    setOpaque(true);
    std::fill(std::begin(preDb_), std::end(preDb_), -80.0f);
    std::fill(std::begin(postDb_), std::end(postDb_), -80.0f);
}

float AnalyzerPanel::freqToX(float freq, int width)
{
    const float lf = std::log10(std::max(freq, 20.0f));
    const float lo = std::log10(20.0f);
    const float hi = std::log10(20000.0f);
    return (lf - lo) / (hi - lo) * (float)width;
}

void AnalyzerPanel::paint(juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0) return;

    g.fillAll(juce::Colour(0xFF14171B));

    // grid
    g.setColour(juce::Colour(0xFF232830));
    const auto yForDb = [h](float db) { return (float)h * (1.0f - (db + 60.0f) / 60.0f); };
    for (float db : { -12.0f, -24.0f, -36.0f, -48.0f, -60.0f })
        g.drawHorizontalLine((int)yForDb(db), 0.0f, (float)w);

    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f })
    {
        const int x = (int)freqToX(f, w);
        g.drawVerticalLine(x, 0.0f, (float)h);
        g.setColour(kDim.withAlpha(0.7f));
        g.setFont(juce::Font(8.5f));
        juce::String label = f >= 1000.0f ? juce::String((int)(f / 1000.0f)) + "k"
                                          : juce::String((int)f);
        g.drawText(label, x + 3, h - 14, 30, 10, juce::Justification::centredLeft, false);
        g.setColour(juce::Colour(0xFF232830));
    }

    const auto yForDbVal = [yForDb](float db) { return yForDb(std::max(db, -70.0f)); };

    // band fills under the post spectrum (split mode)
    if (split_)
    {
        for (int b = 0; b < 3; ++b)
        {
            float f0 = b == 0 ? 20.0f : (b == 1 ? xover1_ : xover2_);
            float f1 = b == 0 ? xover1_ : (b == 1 ? xover2_ : 20000.0f);
            const int x0 = (int)freqToX(f0, w);
            const int x1 = (int)freqToX(f1, w);
            juce::Path fill;
            fill.startNewSubPath((float)x0, yForDbVal(postDb_[0]));
            for (int i = 0; i < numBins_; ++i)
            {
                const int x = binToX(i, w);
                if (x < x0) continue;
                if (x > x1) break;
                fill.lineTo((float)x, yForDbVal(postDb_[i]));
            }
            fill.lineTo((float)x1, (float)h);
            fill.lineTo((float)x0, (float)h);
            fill.closeSubPath();
            g.setColour(kBandCols[b].withAlpha(0.16f));
            g.fillPath(fill);
        }
    }

    // pre spectrum (grey) and post spectrum (cyan)
    auto drawLine = [&](float* db, const juce::Colour& col, bool fill)
    {
        juce::Path path;
        path.startNewSubPath(0.0f, yForDbVal(db[0]));
        for (int b = 1; b < numBins_; ++b)
        {
            const int x = binToX(b, w);
            if (x >= w) break;
            path.lineTo((float)x, yForDbVal(db[b]));
        }
        if (fill)
        {
            juce::Path fp(path);
            fp.lineTo((float)w, (float)h);
            fp.lineTo(0.0f, (float)h);
            fp.closeSubPath();
            g.setColour(col.withAlpha(0.18f));
            g.fillPath(fp);
        }
        g.setColour(col);
        g.strokePath(path, juce::PathStrokeType(1.4f));
    };
    drawLine(preDb_, juce::Colour(0xFF9AA2AC), true);
    drawLine(postDb_, kCyan, false);

    // crossover markers
    if (split_)
    {
        for (float f : { xover1_, xover2_ })
        {
            const int x = (int)freqToX(f, w);
            juce::Path dash;
            dash.startNewSubPath((float)x, 0.0f);
            dash.lineTo((float)x, (float)h);
            const float dashLengths[] = { 4.0f, 4.0f };
            juce::Path dashed;
            juce::PathStrokeType(1.0f).createDashedStroke(dashed, dash, dashLengths, 2);
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.strokePath(dashed, juce::PathStrokeType(1.0f));
        }
    }

    // border
    g.setColour(juce::Colour(0xFF2E333C));
    g.drawRect(getLocalBounds(), 1);
}

// ============================================================================
// GrMeter — horizontal gain-reduction meter with scale ticks
// ============================================================================
GrMeter::GrMeter()
{
    setOpaque(false);
}

void GrMeter::setGr(float grDb)
{
    target_ = grDb;
}

void GrMeter::tick()
{
    // UI-side exponential smoothing for a damped readout
    const float diff = target_ - value_;
    value_ += diff * 0.35f;
    if (std::fabs(diff) < 0.02f) value_ = target_;
}

void GrMeter::paint(juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0) return;

    const float gr = juce::jlimit(0.0f, 24.0f, -value_);
    const float frac = gr / 24.0f;

    // track
    g.setColour(juce::Colour(0xFF14161A));
    g.fillRoundedRectangle(1.0f, 2.0f, (float)w - 2.0f, (float)h - 14.0f, 3.0f);

    // filled bar (sweeps left -> right as reduction grows)
    const int barH = h - 14;
    if (frac > 0.001f)
    {
        const float bw = (float)(w - 4) * frac;
        juce::Colour col = gr < 6.0f ? kCyan : (gr < 12.0f ? kAccent : juce::Colour(0xFFE05A4E));
        g.setColour(col.withAlpha(0.95f));
        g.fillRoundedRectangle(3.0f, 3.0f, bw, (float)(barH - 2), 2.0f);
    }

    // scale ticks (0 dB at left)
    g.setColour(juce::Colour(0xFF4A525E));
    const float tickPositions[] = { 3.0f, 6.0f, 10.0f, 15.0f, 20.0f, 24.0f };
    for (float t : tickPositions)
    {
        const int x = 2 + (int)((float)(w - 4) * t / 24.0f);
        g.drawVerticalLine(x, 3.0f, (float)(barH - 1));
    }

    // scale labels
    g.setColour(kDim);
    g.setFont(juce::Font(7.5f));
    const float labels[] = { 0.0f, 6.0f, 12.0f, 24.0f };
    for (float l : labels)
    {
        const int x = 2 + (int)((float)(w - 4) * l / 24.0f);
        juce::String txt = l == 0.0f ? "0" : juce::String((int)l);
        g.drawText(txt, x - 8, barH + 2, 16, 9, juce::Justification::centred, false);
    }

    // "GR" caption at top-left
    g.setColour(gr > 0.5f ? juce::Colour(0xFFE8B35C) : kDim);
    g.setFont(juce::Font(7.5f).boldened());
    g.drawText("GR", 4, 1, 16, 8, juce::Justification::centredLeft, false);

    // border
    g.setColour(juce::Colour(0xFF2A2F38));
    g.drawRoundedRectangle(1.0f, 2.0f, (float)w - 2.0f, (float)barH, 3.0f, 1.0f);
}

// ============================================================================
// BandPanel
// ============================================================================
BandPanel::BandPanel(LumaCompAudioProcessor& proc, int bandIndex)
    : proc_(proc), band_(bandIndex)
{
    const juce::Colour col = LumaLookAndFeel::bandColour(bandIndex);
    const juce::String s = juce::String(bandIndex);

    title_.setText(bandIndex == 0 ? "LOW" : (bandIndex == 1 ? "MID" : "HIGH"),
                   juce::dontSendNotification);
    title_.setFont(juce::Font(13.0f).boldened());
    title_.setColour(juce::Label::textColourId, col);
    addAndMakeVisible(title_);

    auto style = [this, &col](juce::Slider& sl, juce::Label& name, const juce::String& txt)
    {
        sl.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        sl.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f, false);
        sl.setLookAndFeel(&lf_);
        sl.setOpaque(false);
        addAndMakeVisible(sl);
        name.setText(txt, juce::dontSendNotification);
        name.setFont(juce::Font(8.5f).boldened());
        name.setColour(juce::Label::textColourId, kDim);
        name.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(name);
    };

    lf_.accent = col;

    style(th_, thName_, "THRESH");
    style(rat_, ratName_, "RATIO");
    style(att_, attName_, "ATTACK");
    style(rel_, relName_, "RELEASE");
    style(mk_, mkName_, "MAKEUP");
    style(schp_, scHpName_, "SC HP");
    style(sclp_, scLpName_, "SC LP");

    setSliderRangeFromParam(th_, proc_.apvts, "th" + s);
    setSliderRangeFromParam(rat_, proc_.apvts, "rat" + s);
    setSliderRangeFromParam(att_, proc_.apvts, "att" + s);
    setSliderRangeFromParam(rel_, proc_.apvts, "rel" + s);
    setSliderRangeFromParam(mk_, proc_.apvts, "mk" + s);
    setSliderRangeFromParam(schp_, proc_.apvts, "schp" + s);
    setSliderRangeFromParam(sclp_, proc_.apvts, "sclp" + s);

    attTh_    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "th" + s, th_);
    attRat_   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "rat" + s, rat_);
    attAtt_   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "att" + s, att_);
    attRel_   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "rel" + s, rel_);
    attMk_    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "mk" + s, mk_);
    attScHp_  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "schp" + s, schp_);
    attScLp_  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "sclp" + s, sclp_);

    solo_.setButtonText("SOLO");
    mute_.setButtonText("MUTE");
    bypass_.setButtonText("BYP");
    for (auto* b : { &solo_, &mute_, &bypass_ })
    {
        b->setLookAndFeel(&lf_);
        b->setOpaque(false);
        addAndMakeVisible(b);
    }
    attSolo_   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc_.apvts, "solo" + s, solo_);
    attMute_   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc_.apvts, "mut" + s, mute_);
    attBypass_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc_.apvts, "sb" + s, bypass_);

    addAndMakeVisible(gr_);
    setOpaque(false);
}

void BandPanel::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    title_.setBounds(12, 5, 60, 18);
    bypass_.setBounds(w - 220, 5, 64, 16);
    solo_.setBounds(w - 150, 5, 64, 16);
    mute_.setBounds(w - 80, 5, 64, 16);

    // knob grid (4 columns x 2 rows)
    const int kx = 8;
    const int kw = (w - 16) / 4;
    const int row1Y = 26;
    const int row2Y = 134;
    const int kh = 104;
    th_.setBounds(kx, row1Y, kw, kh);       thName_.setBounds(kx, row1Y + kh - 24, kw, 12);
    rat_.setBounds(kx + kw, row1Y, kw, kh); ratName_.setBounds(kx + kw, row1Y + kh - 24, kw, 12);
    att_.setBounds(kx + kw * 2, row1Y, kw, kh); attName_.setBounds(kx + kw * 2, row1Y + kh - 24, kw, 12);
    rel_.setBounds(kx + kw * 3, row1Y, kw, kh); relName_.setBounds(kx + kw * 3, row1Y + kh - 24, kw, 12);

    mk_.setBounds(kx, row2Y, kw, kh);       mkName_.setBounds(kx, row2Y + kh - 24, kw, 12);
    schp_.setBounds(kx + kw, row2Y, kw, kh); scHpName_.setBounds(kx + kw, row2Y + kh - 24, kw, 12);
    sclp_.setBounds(kx + kw * 2, row2Y, kw, kh); scLpName_.setBounds(kx + kw * 2, row2Y + kh - 24, kw, 12);

    // horizontal GR meter across the bottom
    gr_.setBounds(10, h - 30, w - 20, 22);
}

void BandPanel::paint(juce::Graphics& g)
{
    paintPanel(g, getLocalBounds(), LumaLookAndFeel::bandColour(band_), true);
}

void BandPanel::refresh()
{
    gr_.setGr(proc_.meters.bandGr[band_].load(std::memory_order_relaxed));
    gr_.tick();
}

// ============================================================================
// Editor
// ============================================================================
LumaCompAudioProcessorEditor::LumaCompAudioProcessorEditor(LumaCompAudioProcessor& proc)
    : AudioProcessorEditor(&proc), proc_(proc)
{
    lf_.accent = kAccent;

    title_.setText("LumaComp", juce::dontSendNotification);
    title_.setFont(juce::Font(24.0f).boldened());
    title_.setColour(juce::Label::textColourId, kText);
    addAndMakeVisible(title_);

    subtitle_.setText("multiband compressor  ·  harmonic brightener", juce::dontSendNotification);
    subtitle_.setFont(juce::Font(11.0f));
    subtitle_.setColour(juce::Label::textColourId, kDim);
    addAndMakeVisible(subtitle_);

    modeWide_.setButtonText("WIDE");
    modeSplit_.setButtonText("SPLIT");
    bypass_.setButtonText("BYPASS");
    for (auto* b : { &modeWide_, &modeSplit_, &bypass_ })
    {
        b->setLookAndFeel(&lf_);
        b->setOpaque(false);
        addAndMakeVisible(b);
    }
    modeWide_.onClick = [this] { if (auto* p = proc_.apvts.getParameter("mode")) p->setValueNotifyingHost(0.0f); };
    modeSplit_.onClick = [this] { if (auto* p = proc_.apvts.getParameter("mode")) p->setValueNotifyingHost(1.0f); };
    attBypass_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc_.apvts, "bypass", bypass_);

    analyzer_ = std::make_unique<AnalyzerPanel>();
    addAndMakeVisible(*analyzer_);

    for (int b = 0; b < 3; ++b)
    {
        bandPanels_[b] = std::make_unique<BandPanel>(proc_, b);
        addAndMakeVisible(*bandPanels_[b]);
    }
    // initial visibility from current mode (avoids first-frame flicker)
    {
        const bool split = proc_.apvts.getRawParameterValue("mode")->load() > 0.5f;
        bandPanels_[0]->setVisible(split);
        bandPanels_[2]->setVisible(split);
        lastMode_ = split ? 1 : 0;
    }

    // ---- saturation section ----
    satTitle_.setText("SATURATION · HARMONIC COLOR", juce::dontSendNotification);
    satTitle_.setFont(juce::Font(10.0f).boldened());
    satTitle_.setColour(juce::Label::textColourId, kAccent);
    addAndMakeVisible(satTitle_);

    colorSoft_.setButtonText("SOFT");
    colorTube_.setButtonText("TUBE");
    colorBright_.setButtonText("BRIGHT");
    for (auto* b : { &colorSoft_, &colorTube_, &colorBright_ })
    {
        b->setLookAndFeel(&lf_);
        b->setOpaque(false);
        addAndMakeVisible(b);
    }
    colorSoft_.onClick   = [this] { setColorParam(0); };
    colorTube_.onClick   = [this] { setColorParam(1); };
    colorBright_.onClick = [this] { setColorParam(2); };

    auto styleGlobal = [this](juce::Slider& sl, juce::Label& name, const juce::String& txt)
    {
        sl.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        sl.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        sl.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f, false);
        sl.setLookAndFeel(&lf_);
        sl.setOpaque(false);
        addAndMakeVisible(sl);
        name.setText(txt, juce::dontSendNotification);
        name.setFont(juce::Font(8.5f).boldened());
        name.setColour(juce::Label::textColourId, kDim);
        name.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(name);
    };

    styleGlobal(drive_, driveName_, "DRIVE");
    styleGlobal(toneF_, toneFName_, "TONE FREQ");
    styleGlobal(toneG_, toneGName_, "TONE GAIN");
    styleGlobal(satMix_, satMixName_, "SAT MIX");

    setSliderRangeFromParam(drive_, proc_.apvts, "drive");
    setSliderRangeFromParam(toneF_, proc_.apvts, "tonef");
    setSliderRangeFromParam(toneG_, proc_.apvts, "toneg");
    setSliderRangeFromParam(satMix_, proc_.apvts, "satmix");

    attDrive_  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "drive", drive_);
    attToneF_  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "tonef", toneF_);
    attToneG_  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "toneg", toneG_);
    attSatMix_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "satmix", satMix_);

    // ---- output section ----
    outTitle_.setText("OUTPUT", juce::dontSendNotification);
    outTitle_.setFont(juce::Font(10.0f).boldened());
    outTitle_.setColour(juce::Label::textColourId, kCyan);
    addAndMakeVisible(outTitle_);

    styleGlobal(in_, inName_, "INPUT");
    styleGlobal(lookahead_, lookName_, "LOOKAHEAD");
    styleGlobal(mix_, mixName_, "MIX");
    styleGlobal(out_, outName_, "OUTPUT");

    setSliderRangeFromParam(in_, proc_.apvts, "input");
    setSliderRangeFromParam(lookahead_, proc_.apvts, "lookahead");
    setSliderRangeFromParam(mix_, proc_.apvts, "mix");
    setSliderRangeFromParam(out_, proc_.apvts, "output");

    attIn_   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "input", in_);
    attLook_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "lookahead", lookahead_);
    attMix_  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "mix", mix_);
    attOut_  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc_.apvts, "output", out_);

    setSize(980, 640);
    startTimerHz(30);
}

LumaCompAudioProcessorEditor::~LumaCompAudioProcessorEditor()
{
    stopTimer();
    for (auto* c : getChildren())
        c->setLookAndFeel(nullptr);
}

void LumaCompAudioProcessorEditor::setColorParam(int index)
{
    if (auto* p = proc_.apvts.getParameter("color"))
        p->setValueNotifyingHost(index / 2.0f);
}

void LumaCompAudioProcessorEditor::paint(juce::Graphics& g)
{
    // background with subtle vertical gradient
    g.fillAll(kBg);
    juce::ColourGradient grad(kBg.brighter(0.04f), 0.0f, 0.0f,
                              kBg.darker(0.06f), 0.0f, (float)getHeight(), false);
    g.setGradientFill(grad);
    g.fillRect(getLocalBounds());

    // top accent line
    g.setColour(kAccent.withAlpha(0.8f));
    g.fillRect(0, 0, getWidth(), 2);
}

void LumaCompAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    title_.setBounds(16, 8, 150, 30);
    subtitle_.setBounds(16, 36, 320, 12);
    modeWide_.setBounds(w - 250, 11, 74, 24);
    modeSplit_.setBounds(w - 172, 11, 74, 24);
    bypass_.setBounds(w - 92, 11, 78, 24);

    analyzer_->setBounds(14, 52, w - 28, 168);

    const int bandY = 230;
    const int bandH = 286;
    const int bandW = (w - 28 - 12) / 3;
    for (int b = 0; b < 3; ++b)
        bandPanels_[b]->setBounds(14 + b * (bandW + 6), bandY, bandW, bandH);

    // bottom sections
    const int bottomY = bandY + bandH + 12;
    const int bottomH = h - bottomY - 12;
    const int satW = (int)((w - 28) * 0.60f);
    const int outX = 14 + satW + 8;
    const int outW = w - 14 - outX;

    // saturation panel layout
    satTitle_.setBounds(24, bottomY + 8, 240, 12);
    const int colorY = bottomY + 22;
    colorSoft_.setBounds(24, colorY, 64, 20);
    colorTube_.setBounds(94, colorY, 64, 20);
    colorBright_.setBounds(164, colorY, 72, 20);

    const int knobY = bottomY + 46;
    const int knobH = bottomH - 46;
    const int satKnobW = (satW - 24 - 16) / 4;
    drive_.setBounds(20, knobY, satKnobW, knobH);
    driveName_.setBounds(20, knobY + knobH - 22, satKnobW, 12);
    toneF_.setBounds(20 + satKnobW, knobY, satKnobW, knobH);
    toneFName_.setBounds(20 + satKnobW, knobY + knobH - 22, satKnobW, 12);
    toneG_.setBounds(20 + satKnobW * 2, knobY, satKnobW, knobH);
    toneGName_.setBounds(20 + satKnobW * 2, knobY + knobH - 22, satKnobW, 12);
    satMix_.setBounds(20 + satKnobW * 3, knobY, satKnobW, knobH);
    satMixName_.setBounds(20 + satKnobW * 3, knobY + knobH - 22, satKnobW, 12);

    // output panel layout
    outTitle_.setBounds(outX + 10, bottomY + 8, 120, 12);
    const int outKnobW = (outW - 20 - 16) / 4;
    in_.setBounds(outX + 8, knobY, outKnobW, knobH);
    inName_.setBounds(outX + 8, knobY + knobH - 22, outKnobW, 12);
    lookahead_.setBounds(outX + 8 + outKnobW, knobY, outKnobW, knobH);
    lookName_.setBounds(outX + 8 + outKnobW, knobY + knobH - 22, outKnobW, 12);
    mix_.setBounds(outX + 8 + outKnobW * 2, knobY, outKnobW, knobH);
    mixName_.setBounds(outX + 8 + outKnobW * 2, knobY + knobH - 22, outKnobW, 12);
    out_.setBounds(outX + 8 + outKnobW * 3, knobY, outKnobW, knobH);
    outName_.setBounds(outX + 8 + outKnobW * 3, knobY + knobH - 22, outKnobW, 12);
}

void LumaCompAudioProcessorEditor::timerCallback()
{
    // spectra
    const int bins = juce::jmin(analyzer_->getNumBins(), 1024);
    proc_.analyzerPre.computeSpectrum(analyzer_->getPreDb(), bins);
    proc_.analyzerPost.computeSpectrum(analyzer_->getPostDb(), bins);
    analyzer_->setAnalysis(bins, proc_.getSampleRate() > 0 ? (float)proc_.getSampleRate() : 48000.0f,
                           proc_.analyzerPre.getSize());

    const bool split = proc_.apvts.getRawParameterValue("mode")->load() > 0.5f;
    analyzer_->setSplitMode(split);
    analyzer_->setCrossovers(proc_.apvts.getRawParameterValue("xov1")->load(),
                             proc_.apvts.getRawParameterValue("xov2")->load());
    analyzer_->repaint();

    // mode buttons
    const int mode = split ? 1 : 0;
    if (mode != lastMode_)
    {
        lastMode_ = mode;
        modeWide_.setToggleState(mode == 0, juce::dontSendNotification);
        modeSplit_.setToggleState(mode == 1, juce::dontSendNotification);
        bandPanels_[0]->setVisible(mode == 1);
        bandPanels_[2]->setVisible(mode == 1);
    }

    // color buttons
    const int col = (int)(proc_.apvts.getRawParameterValue("color")->load() + 0.5f);
    if (col != lastColor_)
    {
        lastColor_ = col;
        colorSoft_.setToggleState(col == 0, juce::dontSendNotification);
        colorTube_.setToggleState(col == 1, juce::dontSendNotification);
        colorBright_.setToggleState(col == 2, juce::dontSendNotification);
    }

    // band GR meters
    for (int b = 0; b < 3; ++b)
        bandPanels_[b]->refresh();

    repaint();
}
