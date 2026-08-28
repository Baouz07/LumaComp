// vst3test.cpp — minimal VST3 host (SDK hosting API) to functionally verify
// LumaComp's VST3 build exactly as a real host (REAPER) would load it.
//
// Build (from tools/):
//   g++ -O2 -static vst3test.cpp \
//       ../third_party/vst3sdk/vst3sdk-3.7.8_build_34/public.sdk/source/vst/hosting/module.cpp \
//       ../third_party/vst3sdk/vst3sdk-3.7.8_build_34/public.sdk/source/vst/hosting/module_win32.cpp \
//       ../third_party/vst3sdk/vst3sdk-3.7.8_build_34/public.sdk/source/vst/hosting/hostclasses.cpp \
//       ../third_party/vst3sdk/vst3sdk-3.7.8_build_34/public.sdk/source/vst/hosting/pluginterfacesupport.cpp \
//       ../third_party/vst3sdk/vst3sdk-3.7.8_build_34/base/source/fstring.cpp \
//       ../third_party/vst3sdk/vst3sdk-3.7.8_build_34/base/source/fstreamer.cpp \
//       ../third_party/vst3sdk/vst3sdk-3.7.8_build_34/base/source/updatehandler.cpp \
//       -I ../third_party/vst3sdk/vst3sdk-3.7.8_build_34 \
//       -o vst3test.exe
// Run: vst3test.exe <path-to-inner-vst3-dll>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#include <windows.h>

#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/base/ipluginbase.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
using VST3::Hosting::Module;
using VST3::Hosting::ClassInfo;

// MinGW-w64 does not ship these two FOLDERID constants; define them manually
// (values from the Windows SDK shlobj_core.h).
extern "C" {
    __declspec(selectany) GUID FOLDERID_ProgramFilesCommon =
        { 0xF7F1ED05, 0x9F6D, 0x47A2, { 0xAA, 0xAA, 0x29, 0xDE, 0xE3, 0x17, 0xC5, 0xD9 } };
    __declspec(selectany) GUID FOLDERID_UserProgramFilesCommon =
        { 0xbcbd3057, 0xca5c, 0x4622, { 0xb4, 0x2d, 0xbc, 0x56, 0xdb, 0x0a, 0xe5, 0x93 } };
}

static constexpr int kBlock = 512;
static constexpr float kSr = 44100.0f;

static std::string u16toAscii(const TChar* s)
{
    std::string r;
    while (*s != 0) { r += (char)(*s++ & 0x7f); }
    return r;
}

static float peakOf(const float* d, int n)
{
    float p = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float a = std::fabs(d[i]);
        if (a > p) p = a;
    }
    return p;
}

// ---------------------------------------------------------------------------
// Minimal IParameterChanges / IParameterValueQueue (automation path)
// ---------------------------------------------------------------------------
class TestValueQueue : public IParamValueQueue
{
public:
    TestValueQueue() = default;
    void set(ParamID id, int32 sampleOffset, ParamValue value)
    {
        id_ = id; offset_ = sampleOffset; value_ = value; has_ = true;
    }
    bool has() const { return has_; }

    ParamID PLUGIN_API getParameterId() override { return id_; }
    int32 PLUGIN_API getPointCount() override { return has_ ? 1 : 0; }
    tresult PLUGIN_API getPoint(int32, int32& sampleOffset, ParamValue& value) override
    {
        if (!has_) return kResultFalse;
        sampleOffset = offset_; value = value_; return kResultTrue;
    }
    tresult PLUGIN_API addPoint(int32 sampleOffset, ParamValue value, int32& index) override
    {
        set(0, sampleOffset, value); index = 0; return kResultTrue;
    }
    DECLARE_FUNKNOWN_METHODS

private:
    ParamID id_ = 0;
    int32 offset_ = 0;
    ParamValue value_ = 0.0;
    bool has_ = false;
};

IMPLEMENT_FUNKNOWN_METHODS(TestValueQueue, IParamValueQueue, IParamValueQueue::iid)

class TestParameterChanges : public IParameterChanges
{
public:
    void add(ParamID id, ParamValue value)
    {
        queues_[0].set(id, 0, value);
        count_ = 1;
    }

    int32 PLUGIN_API getParameterCount() override { return count_; }
    IParamValueQueue* PLUGIN_API getParameterData(int32 index) override
    {
        return index == 0 ? &queues_[0] : nullptr;
    }
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID& id, int32& index) override
    {
        index = 0; return &queues_[0];
    }
    DECLARE_FUNKNOWN_METHODS

private:
    TestValueQueue queues_[1];
    int32 count_ = 0;
};

IMPLEMENT_FUNKNOWN_METHODS(TestParameterChanges, IParameterChanges, IParameterChanges::iid)

// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1]
        : "E:/AI/VST/dist/LumaComp.vst3/Contents/x86_64-win/LumaComp.vst3";

    std::string error;
    auto module = Module::create(path, error);
    if (!module)
    {
        printf("FAIL: Module::create(%s) error=%s\n", path, error.c_str());
        return 1;
    }
    printf("OK: module loaded\n");

    const auto& factory = module->getFactory();
    auto infos = factory.classInfos();
    printf("INFO: factory classes=%u\n", (unsigned)infos.size());

    ClassInfo* componentInfo = nullptr;
    ClassInfo* controllerInfo = nullptr;
    for (auto& ci : infos)
    {
        printf("INFO: class '%s' cat='%s'\n", ci.name().c_str(), ci.category().c_str());
        if (ci.category() == "Audio Module Class" && componentInfo == nullptr)
            componentInfo = &ci;
        if (ci.category() == "Component Controller Class" && controllerInfo == nullptr)
            controllerInfo = &ci;
    }
    if (!componentInfo)
    {
        printf("FAIL: no audio module class\n");
        return 1;
    }

    HostApplication hostApp;
    factory.setHostContext(&hostApp);

    auto component = factory.createInstance<IComponent>(componentInfo->get().classID);
    if (!component)
    {
        printf("FAIL: createInstance(IComponent)\n");
        return 1;
    }
    printf("OK: component created\n");

    if (component->initialize(&hostApp) != kResultOk)
    {
        printf("FAIL: initialize\n");
        return 1;
    }
    printf("OK: component initialized\n");

    // controller (for parameter metadata)
    IPtr<IEditController> controller;
    if (controllerInfo)
    {
        auto c = factory.createInstance<IEditController>(controllerInfo->get().classID);
        controller = c;
        if (controller)
        {
            controller->initialize(&hostApp);
            printf("OK: controller created\n");
        }
    }

    FUnknownPtr<IAudioProcessor> processor(component);
    if (!processor)
    {
        printf("FAIL: not an audio processor\n");
        return 1;
    }

    const int32 inBuses = component->getBusCount(kAudio, kInput);
    const int32 outBuses = component->getBusCount(kAudio, kOutput);
    printf("INFO: input buses=%d output buses=%d\n", (int)inBuses, (int)outBuses);
    if (inBuses < 1 || outBuses < 1)
    {
        printf("FAIL: missing buses\n");
        return 1;
    }
    for (int32 i = 0; i < inBuses; ++i)
        component->activateBus(kAudio, kInput, i, true);
    for (int32 i = 0; i < outBuses; ++i)
        component->activateBus(kAudio, kOutput, i, true);

    ProcessSetup setup;
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample32;
    setup.maxSamplesPerBlock = kBlock;
    setup.sampleRate = kSr;
    if (processor->setupProcessing(setup) != kResultOk)
    {
        printf("FAIL: setupProcessing\n");
        return 1;
    }
    printf("OK: setupProcessing\n");

    if (component->setActive(true) != kResultOk)
    {
        printf("FAIL: setActive\n");
        return 1;
    }
    printf("OK: setActive(true)\n");

    // ---- parameters ----
    int32 numParams = 0;
    int32 thIdx = -1, ratIdx = -1;
    if (controller)
    {
        for (int32 i = 0;; ++i)
        {
            ParameterInfo info;
            if (controller->getParameterInfo(i, info) != kResultOk) break;
            numParams = i + 1;
            const std::string title = u16toAscii(info.title);
            if (i < 5 || i > numParams - 4)
                printf("INFO: param[%d] title='%s'\n", i, title.c_str());
            if (thIdx < 0 && title.find("Threshold") != std::string::npos) thIdx = i;
            if (ratIdx < 0 && title.find("Ratio") != std::string::npos) ratIdx = i;
        }
    }
    printf("INFO: total params=%d  threshold idx=%d  ratio idx=%d\n",
           (int)numParams, (int)thIdx, (int)ratIdx);

    // ---- audio processing ----
    std::vector<float> inL(kBlock), inR(kBlock), outL(kBlock), outR(kBlock);
    for (int i = 0; i < kBlock; ++i)
    {
        const float v = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * i / kSr);
        inL[i] = v; inR[i] = v;
    }

    float* inputBuffers[2] = { inL.data(), inR.data() };
    float* outputBuffers[2] = { outL.data(), outR.data() };

    AudioBusBuffers inBuf, outBuf;
    memset(&inBuf, 0, sizeof(inBuf));
    memset(&outBuf, 0, sizeof(outBuf));
    inBuf.numChannels = 2;
    inBuf.channelBuffers32 = inputBuffers;
    outBuf.numChannels = 2;
    outBuf.channelBuffers32 = outputBuffers;

    TestParameterChanges changes;

    ProcessData pd;
    memset(&pd, 0, sizeof(pd));
    pd.processMode = kRealtime;
    pd.symbolicSampleSize = kSample32;
    pd.numSamples = kBlock;
    pd.numInputs = 1;
    pd.numOutputs = 1;
    pd.inputs = &inBuf;
    pd.outputs = &outBuf;
    pd.inputParameterChanges = &changes;
    pd.outputParameterChanges = nullptr;

    // 20 warm-up blocks (envelope settle), then measure
    for (int w = 0; w < 20; ++w)
        processor->process(pd);

    const float inPeak = peakOf(inL.data(), kBlock);
    const float outPeak = peakOf(outL.data(), kBlock);
    printf("AUDIO: defaults input peak=%.4f output peak=%.4f\n", inPeak, outPeak);
    printf("%s\n", (outPeak > 1e-6f && outPeak < inPeak * 0.999f)
               ? "OK: VST3 compression engaged"
               : "FAIL: VST3 no compression");

    // force heavy compression via the automation path (IParameterChanges)
    if (thIdx >= 0 && ratIdx >= 0)
    {
        changes.add((ParamID)thIdx, 20.0 / 60.0);   // threshold -40 dB
        changes.add((ParamID)ratIdx, 19.0 / 19.0);  // ratio 20:1
        for (int w = 0; w < 20; ++w)
            processor->process(pd);
        const float outPeak2 = peakOf(outL.data(), kBlock);
        printf("AUDIO: heavy-comp output peak=%.4f (input 0.5000)\n", outPeak2);
        printf("%s\n", outPeak2 < outPeak * 0.9f
                   ? "OK: VST3 heavier compression"
                   : "FAIL: VST3 no extra compression");
    }

    component->setActive(false);
    component->terminate();
    if (controller) controller->terminate();
    printf("DONE\n");
    return 0;
}
