// ProcessorMeterTest.cpp — instantiate processor + editor directly and verify
// the FULL meter chain: processBlock -> meters -> timer -> GrMeter display.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstdio>
#include <cmath>
#include <vector>

int main()
{
    juce::MessageManager::getInstance();
    juce::MessageManager::getInstance()->setCurrentThreadAsMessageThread();

    LumaCompAudioProcessor proc;
    proc.prepareToPlay(44100.0, 512);

    // 1) processor-side: drive audio for 30 blocks (fresh input each block)
    std::vector<float> srcL(512), srcR(512);
    for (int i = 0; i < 512; ++i)
    {
        const float v = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * i / 44100.0f);
        srcL[i] = v; srcR[i] = v;
    }
    juce::AudioBuffer<float> buf(2, 512);
    juce::MidiBuffer midi;
    for (int w = 0; w < 30; ++w)
    {
        for (int i = 0; i < 512; ++i)
        {
            buf.setSample(0, i, srcL[i]);
            buf.setSample(1, i, srcR[i]);
        }
        proc.processBlock(buf, midi);
        const float g1 = proc.meters.bandGr[1].load(std::memory_order_relaxed);
        if (w == 0 || w == 4 || w == 9 || w == 14 || w == 29)
            printf("block %2d: bandGr[1]=%.3f outPeak=%.4f\n", w, (double)g1,
                   (double)std::fabs(buf.getSample(0, 200)));
    }
    const float g1 = proc.meters.bandGr[1].load(std::memory_order_relaxed);
    printf("processor: bandGr[1] = %.3f\n", (double)g1);

    // 2) UI-side: create the editor, then drive the same path the timer runs
    std::unique_ptr<juce::AudioProcessorEditor> editor(proc.createEditor());
    if (!editor)
    {
        printf("FAIL: createEditor returned null\n");
        return 1;
    }
    printf("editor created\n");

    auto* e = dynamic_cast<LumaCompAudioProcessorEditor*>(editor.get());
    if (e == nullptr)
    {
        printf("FAIL: editor is not LumaCompAudioProcessorEditor\n");
        return 1;
    }

    // simulate ~15 timer ticks
    for (int i = 0; i < 15; ++i)
        e->timerCallback();

    const float disp = e->getDebugGrDisplay(1);
    printf("editor meter display band[1] = %.3f\n", (double)disp);
    printf("%s\n", (disp < -0.01f) ? "OK: full meter chain works (UI shows reduction)"
                                   : "FAIL: UI meter not driven");
    return (disp < -0.01f) ? 0 : 1;
}
