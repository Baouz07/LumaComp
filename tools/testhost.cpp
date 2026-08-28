// testhost.cpp — minimal VST2 host for functional smoke-testing LumaComp.dll
// Build: g++ testhost.cpp -o testhost.exe
// Run:   testhost.exe <path-to-dll>
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <vector>

typedef intptr_t(*dispatcherFn)(void* effect, int32_t opcode, int32_t index,
                                intptr_t value, void* ptr, float opt);
typedef float(*getParameterFn)(void* effect, int32_t index);
typedef void(*setParameterFn)(void* effect, int32_t index, float value);
typedef void(*processFn)(void* effect, float** inputs, float** outputs,
                         int32_t sampleFrames);

struct AEffect
{
    int32_t magic;
    dispatcherFn dispatcher;
    processFn process; // deprecated
    setParameterFn setParameter;
    getParameterFn getParameter;
    int32_t numPrograms, numParams, numInputs, numOutputs;
    int32_t flags;
    void* resvd1;
    void* resvd2;
    int32_t initialDelay;
    int32_t realQualities, offQualities, ioRatio;
    void* object;
    void* user;
    int32_t uniqueID;
    int32_t version;
    processFn processReplacing;
    processFn processDoubleReplacing;
    char future[56];
};

enum
{
    effOpen = 0, effClose = 1, effSetProgram = 4, effGetPlugCategory = 43,
    effGetEffectName = 45, effGetVendorString = 47, effGetProductString = 48,
    effGetVendorVersion = 49, effSetSampleRate = 10, effSetBlockSize = 11,
    effMainsChanged = 12
};

static const int32_t kEffectMagic = 0x56737450; // 'VstP' — the value JUCE 6.x VST2 wrappers ship; accepted by REAPER

// Minimal host callback — real hosts (REAPER etc.) always provide one.
static intptr_t hostCallback(void* effect, int32_t opcode, int32_t index,
                             intptr_t value, void* ptr, float opt)
{
    fprintf(stderr, "[host] opcode=%d index=%d value=%lld opt=%f\n", opcode, index,
            (long long)value, opt);
    switch (opcode)
    {
    case 1:   // audioMasterVersion -> we are a VST 2.4 host
        return 2400;
    case 13:  // audioMasterGetSampleRate
        return 44100;
    case 3:   // audioMasterIdle
        return 1;
    default:
        return 0;
    }
}

static float peakOf(const float* data, int n)
{
    float p = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float a = std::fabs(data[i]);
        if (a > p) p = a;
    }
    return p;
}

int main(int argc, char** argv)
{
    const char* path = argc > 1 ? argv[1] : "libLumaComp.dll";
    const int block = 512;

    HMODULE h = LoadLibraryA(path);
    if (!h)
    {
        printf("FAIL: LoadLibrary(%s) error=%lu\n", path, GetLastError());
        return 1;
    }
    printf("OK: loaded %s\n", path);

    auto entry = (void* (*)(dispatcherFn))GetProcAddress(h, "VSTPluginMain");
    if (!entry)
    {
        printf("FAIL: no VSTPluginMain export\n");
        return 1;
    }

    AEffect* fx = (AEffect*)entry(hostCallback);
    printf("INFO: VSTPluginMain returned %p\n", (void*)fx);
    if (fx != nullptr)
    {
        const unsigned char* b = (const unsigned char*)fx;
        printf("INFO: first 32 bytes:");
        for (int i = 0; i < 32; ++i)
            printf(" %02x", b[i]);
        printf("\n");
    }
    if (fx == nullptr || fx->magic != kEffectMagic)
    {
        printf("FAIL: bad AEffect magic=%08x\n", fx ? fx->magic : 0);
        return 1;
    }
    printf("OK: magic ok, params=%d in=%d out=%d flags=%08x initialDelay=%d\n",
           fx->numParams, fx->numInputs, fx->numOutputs, fx->flags, fx->initialDelay);

    char name[256] = { 0 };
    if (fx->dispatcher(fx, effGetEffectName, 0, 0, name, 0))
        printf("OK: effect name = %s\n", name);
    if (fx->dispatcher(fx, effGetVendorString, 0, 0, name, 0))
        printf("OK: vendor      = %s\n", name);
    if (fx->dispatcher(fx, effGetProductString, 0, 0, name, 0))
        printf("OK: product     = %s\n", name);
    if (fx->dispatcher(fx, effGetPlugCategory, 0, 0, nullptr, 0) == 1)
        printf("OK: category    = kPlugCategEffect\n");
    printf("OK: vendorVersion = %d\n", (int)fx->dispatcher(fx, effGetVendorVersion, 0, 0, nullptr, 0));

    fx->dispatcher(fx, effOpen, 0, 0, nullptr, 0);
    fx->dispatcher(fx, effSetSampleRate, 0, 0, nullptr, 44100.0f);
    fx->dispatcher(fx, effSetBlockSize, 0, block, nullptr, 0);
    fx->dispatcher(fx, effMainsChanged, 0, 1, nullptr, 0);

    // ---- parameter display sanity: read a few params ----
    const int thIdx = 18; // band 1 (MID/Wide) threshold
    printf("INFO: param[%d] default = %.4f\n", thIdx, fx->getParameter(fx, thIdx));

    // ---- audio test: 440 Hz sine, amplitude 0.5 (-6 dBFS), stereo ----
    std::vector<float> inL(block), inR(block), outL(block), outR(block);
    for (int i = 0; i < block; ++i)
    {
        const float v = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * i / 44100.0f);
        inL[i] = v;
        inR[i] = v;
    }
    float* ins[2] = { inL.data(), inR.data() };
    float* outs[2] = { outL.data(), outR.data() };

    fx->processReplacing(fx, ins, outs, block);

    const float inPeak = peakOf(inL.data(), block);
    // warm up: run several blocks so detector envelopes reach steady state
    for (int w = 0; w < 20; ++w)
        fx->processReplacing(fx, ins, outs, block);
    const float outPeak = peakOf(outL.data(), block);
    printf("AUDIO: input peak=%.4f output peak=%.4f (defaults: threshold -12dB, ratio 4:1 -> expect reduction)\n",
           inPeak, outPeak);

    bool pass = (outPeak > 1e-6f) && (outPeak < inPeak * 0.999f);
    printf("%s\n", pass ? "OK: compression engaged, signal passed" : "WARN: unexpected levels");

    // ---- force heavy compression: threshold -40dB, ratio 20:1 ----
    // normalized = (value - start) / range
    fx->setParameter(fx, 18, (-40.0f - -60.0f) / 60.0f);      // threshold band1 = -40 dB
    fx->setParameter(fx, 19, (20.0f - 1.0f) / 19.0f);          // ratio band1 = 20:1
    fx->setParameter(fx, 3, 1.0f);                             // global mix = 100%
    std::fill(outL.begin(), outL.end(), 0.0f);
    std::fill(outR.begin(), outR.end(), 0.0f);
    for (int w = 0; w < 20; ++w)
        fx->processReplacing(fx, ins, outs, block);
    const float outPeak2 = peakOf(outL.data(), block);
    printf("AUDIO: heavy-comp output peak=%.4f (input 0.5000, expect much less)\n", outPeak2);
    printf("%s\n", outPeak2 < outPeak * 0.9f ? "OK: heavier compression reduced gain further" : "FAIL: no extra compression");

    // ---- saturation test: drive 12 dB, tone gain +6, sat mix 100% ----
    fx->setParameter(fx, 18, 0.0f);                            // threshold 0 dB (no comp)
    fx->setParameter(fx, 19, 0.0f);                            // ratio 1:1 (no comp)
    fx->setParameter(fx, 38, 12.0f / 24.0f);                   // drive = 12 dB
    fx->setParameter(fx, 41, (6.0f - -12.0f) / 24.0f);         // tone gain = +6 dB
    fx->setParameter(fx, 42, 1.0f);                            // sat mix = 100%
    std::fill(outL.begin(), outL.end(), 0.0f);
    std::fill(outR.begin(), outR.end(), 0.0f);
    for (int w = 0; w < 20; ++w)
        fx->processReplacing(fx, ins, outs, block);
    const float outPeak3 = peakOf(outL.data(), block);
    printf("AUDIO: saturator output peak=%.4f (input 0.5000)\n", outPeak3);
    printf("%s\n", outPeak3 > inPeak ? "OK: saturation added gain/harmonics" : "WARN: saturation did not boost");

    // ---- bypass test: must be bit-transparent ----
    fx->setParameter(fx, 4, 1.0f);                             // bypass = on
    std::fill(outL.begin(), outL.end(), 0.0f);
    std::fill(outR.begin(), outR.end(), 0.0f);
    fx->processReplacing(fx, ins, outs, block);
    bool bypassOk = true;
    for (int i = 0; i < block; ++i)
        if (std::fabs(outL[i] - inL[i]) > 1e-7f) { bypassOk = false; break; }
    printf("%s\n", bypassOk ? "OK: bypass is bit-transparent" : "FAIL: bypass not transparent");

    fx->dispatcher(fx, effMainsChanged, 0, 0, nullptr, 0);
    fx->dispatcher(fx, effClose, 0, 0, nullptr, 0);
    FreeLibrary(h);
    printf("DONE\n");
    return 0;
}
