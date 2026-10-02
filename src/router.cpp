// Dynamo - routage interne (WASAPI) : loopback de la sortie "source" -> DSP -> sortie "cible".
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <mmsystem.h>
#include <xmmintrin.h>
#include <math.h>
#include <string.h>
#include <vector>
#include <algorithm>

#include "router.h"
#include "dsp.h"

static const GUID kSubFloat = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };

static bool IsFloat32(const WAVEFORMATEX* w) {
    if (w->wBitsPerSample != 32) return false;
    if (w->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
    if (w->wFormatTag == WAVE_FORMAT_EXTENSIBLE) return IsEqualGUID(((const WAVEFORMATEXTENSIBLE*)w)->SubFormat, kSubFloat);
    return false;
}

// IAudioEndpointVolume (declaration locale)
struct IDynEndpointVolume : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE RegisterControlChangeNotify(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnregisterControlChangeNotify(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelCount(UINT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMasterVolumeLevel(float, LPCGUID) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMasterVolumeLevelScalar(float, LPCGUID) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMasterVolumeLevel(float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMasterVolumeLevelScalar(float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetChannelVolumeLevel(UINT, float, LPCGUID) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetChannelVolumeLevelScalar(UINT, float, LPCGUID) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelVolumeLevel(UINT, float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetChannelVolumeLevelScalar(UINT, float*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMute(BOOL, LPCGUID) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetMute(BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVolumeStepInfo(UINT*, UINT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE VolumeStepUp(LPCGUID) = 0;
    virtual HRESULT STDMETHODCALLTYPE VolumeStepDown(LPCGUID) = 0;
    virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVolumeRange(float*, float*, float*) = 0;
};
static const IID IID_DynEndpointVolume = { 0x5CDF2C82, 0x841E, 0x4546, { 0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A } };

template <class T> static void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

bool Router::Start(const std::wstring& sourceId, const std::wstring& targetId, DynShared* sh) {
    Stop();
    src_ = sourceId; dst_ = targetId; sh_ = sh;
    stop_ = 0; lastErrorCode = 0; lastError[0] = 0;
    DWORD tid;
    thread_ = CreateThread(nullptr, 0, ThreadProc, this, 0, &tid);
    return thread_ != nullptr;
}

void Router::Stop() {
    if (!thread_) return;
    InterlockedExchange(&stop_, 1);
    WaitForSingleObject((HANDLE)thread_, 3000);
    CloseHandle((HANDLE)thread_);
    thread_ = nullptr;
    streaming = false;
}

unsigned long __stdcall Router::ThreadProc(void* self) {
    ((Router*)self)->Run();
    return 0;
}

void Router::Run() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    timeBeginPeriod(1);
    DWORD taskIdx = 0;
    HANDLE mm = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIdx);
    while (!stop_) {
        if (!Session()) {
            streaming = false;
            for (int i = 0; i < 10 && !stop_; ++i) Sleep(100);  // on reessaie (peripherique debranche, format change...)
        }
    }
    if (mm) AvRevertMmThreadCharacteristics(mm);
    timeEndPeriod(1);
    CoUninitialize();
}

bool Router::Session() {
    IMMDeviceEnumerator* en = nullptr;
    IMMDevice* dsrc = nullptr; IMMDevice* ddst = nullptr;
    IAudioClient* cap = nullptr; IAudioClient* ren = nullptr;
    IAudioCaptureClient* capC = nullptr; IAudioRenderClient* renC = nullptr;
    WAVEFORMATEX* wfC = nullptr; WAVEFORMATEX* wfR = nullptr;
    bool ok = false;
    auto fail = [&](long code, const wchar_t* msg) { wcsncpy(lastError, msg, 255); lastError[255] = 0; lastErrorCode = code; };

    do {
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en))) { fail(1, L"Service audio indisponible."); break; }
        if (FAILED(en->GetDevice(src_.c_str(), &dsrc))) { fail(2, L"Sortie source introuvable (débranchée ?)."); break; }
        if (FAILED(en->GetDevice(dst_.c_str(), &ddst))) { fail(3, L"Sortie de destination introuvable (débranchée ?)."); break; }
        if (FAILED(dsrc->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&cap)) || FAILED(cap->GetMixFormat(&wfC))) { fail(4, L"Impossible d'ouvrir la source."); break; }
        if (FAILED(ddst->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&ren)) || FAILED(ren->GetMixFormat(&wfR))) { fail(5, L"Impossible d'ouvrir la destination."); break; }
        if (!IsFloat32(wfC) || !IsFloat32(wfR)) { fail(6, L"Format audio inattendu (32 bits flottant requis)."); break; }
        if (wfC->nSamplesPerSec != wfR->nSamplesPerSec) {
            fail(7, L"Les deux sorties n'ont pas la même fréquence d'échantillonnage : règle-les sur la même valeur (Paramètres son → Propriétés → Format).");
            break;
        }
        // loopback de la source : tampon de 40 ms
        HRESULT hr = cap->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 400000, 0, wfC, nullptr);
        if (FAILED(hr)) { fail(8, L"Capture de la source refusée par Windows."); break; }
        // rendu : tampon de 40 ms, rempli au fil de l'eau
        hr = ren->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 400000, 0, wfR, nullptr);
        if (FAILED(hr)) { fail(9, L"Lecture sur la destination refusée par Windows (utilisée en mode exclusif ?)."); break; }
        if (FAILED(cap->GetService(__uuidof(IAudioCaptureClient), (void**)&capC))) { fail(10, L"Capture indisponible."); break; }
        if (FAILED(ren->GetService(__uuidof(IAudioRenderClient), (void**)&renC))) { fail(11, L"Lecture indisponible."); break; }
        ok = true;
    } while (0);

    if (ok) {
        // volume Windows : si la source a un volume materiel, la capture ne le subit pas -> on l'applique nous-memes
        IDynEndpointVolume* vol = nullptr;
        bool followVol = false;
        if (SUCCEEDED(dsrc->Activate(IID_DynEndpointVolume, CLSCTX_ALL, nullptr, (void**)&vol)) && vol) {
            DWORD hw = 0; vol->QueryHardwareSupport(&hw);
            followVol = (hw & 1) != 0;
        }
        float volTarget = 1.f, volCur = 1.f;
        int volTick = 25;
        const unsigned fs = wfR->nSamplesPerSec;
        const int chC = wfC->nChannels, chR = wfR->nChannels;
        UINT32 renBuf = 0; ren->GetBufferSize(&renBuf);
        dyn::Engine* eng = new dyn::Engine();
        DynParams p; int32_t lastSeq = -1;
        auto fetch = [&]() {
            if (!sh_) return;
            int32_t s1 = sh_->paramSeq;
            if ((s1 & 1) || s1 == lastSeq) return;
            MemoryBarrier(); memcpy(&p, (const void*)&sh_->params, sizeof(p)); MemoryBarrier();
            if (sh_->paramSeq == s1) { eng->applyParams(p); lastSeq = s1; }
        };
        fetch();
        eng->prepare(fs, chR);
        unsigned specPos = sh_ ? sh_->specWrite : 0;

        // anneau (en canaux de destination)
        const size_t ringFrames = fs; // 1 s
        std::vector<float> ring(ringFrames * chR);
        size_t rHead = 0, rCount = 0;
        const UINT32 prime = fs * 14 / 1000;      // 14 ms de reserve : absorbe les paquets de 10 ms du loopback
        const UINT32 maxLatency = fs * 50 / 1000; // au-dela, on rattrape
        bool primed = false;
        double latAvg = 0;

        cap->Start(); ren->Start();
        streaming = true; lastErrorCode = 0; lastError[0] = 0;
        std::vector<float> tmp;
        while (!stop_) {
            // 1. vider la capture
            UINT32 pkt = 0;
            HRESULT hr = capC->GetNextPacketSize(&pkt);
            if (FAILED(hr)) break;
            while (pkt > 0) {
                BYTE* data; UINT32 n; DWORD flags;
                if (FAILED(capC->GetBuffer(&data, &n, &flags, nullptr, nullptr))) break;
                const float* in = (const float*)data;
                for (UINT32 f = 0; f < n; ++f) {
                    size_t pos = (rHead + rCount) % ringFrames;
                    float* o = &ring[pos * chR];
                    for (int c = 0; c < chR; ++c) {
                        float v = 0.f;
                        if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT)) {
                            if (c < chC) v = in[f * chC + c];
                            else if (chC == 1 && c < 2) v = in[f];   // mono -> stereo
                        }
                        o[c] = v;
                    }
                    if (rCount < ringFrames) ++rCount; else rHead = (rHead + 1) % ringFrames;
                }
                capC->ReleaseBuffer(n);
                if (FAILED(capC->GetNextPacketSize(&pkt))) { pkt = 0; break; }
            }
            if (followVol && ++volTick >= 25) {
                volTick = 0;
                float db = 0; BOOL mute = FALSE;
                vol->GetMasterVolumeLevel(&db); vol->GetMute(&mute);
                volTarget = mute ? 0.f : powf(10.f, db / 20.f);
            }
            // 2. rattrapage de latence
            UINT32 pad = 0;
            if (FAILED(ren->GetCurrentPadding(&pad))) break;
            if (rCount + pad > maxLatency && rCount > prime) {
                size_t drop = std::min<size_t>(rCount - prime, rCount + pad - prime * 2);
                rHead = (rHead + drop) % ringFrames; rCount -= drop;
                ++xruns;
            }
            // 3. remplir la destination
            if (pad == 0) primed = false;
            if (!primed && rCount >= prime) primed = true;
            if (primed && rCount > 0) {
                UINT32 space = renBuf - pad;
                UINT32 n = (UINT32)std::min<size_t>(space, rCount);
                if (n > 0) {
                    BYTE* out;
                    if (SUCCEEDED(renC->GetBuffer(n, &out))) {
                        float* o = (float*)out;
                        const float step = (volTarget - volCur) / (float)n;
                        for (UINT32 f = 0; f < n; ++f) {
                            const float* s = &ring[((rHead + f) % ringFrames) * chR];
                            volCur += step;
                            for (int c = 0; c < chR; ++c) o[(size_t)f * chR + c] = s[c] * volCur;
                        }
                        volCur = volTarget;
                        rHead = (rHead + n) % ringFrames; rCount -= n;
                        fetch();
                        const unsigned csr = _mm_getcsr(); _mm_setcsr(csr | 0x8040);
                        dyn::Engine::BlockMeters m;
                        eng->process(o, o, n, m, sh_ ? sh_->spec : nullptr, DYN_SPEC_SIZE - 1, specPos);
                        _mm_setcsr(csr);
                        renC->ReleaseBuffer(n, 0);
                        if (sh_) {
                            DynMeters& M = sh_->meters;
                            for (int i = 0; i < 2; ++i) {
                                if (m.inPeak[i] > M.inPeak[i]) M.inPeak[i] = m.inPeak[i];
                                if (m.outPeak[i] > M.outPeak[i]) M.outPeak[i] = m.outPeak[i];
                            }
                            for (int b = 0; b < DYN_COMP_BANDS; ++b) if (m.gr[b] > M.grComp[b]) M.grComp[b] = m.gr[b];
                            if (m.grLim > M.grLimiter) M.grLimiter = m.grLim;
                            M.compInLevel = m.level;
                            sh_->specWrite = specPos;
                            sh_->sampleRate = fs; sh_->channels = chR; sh_->blockFrames = n;
                            sh_->heartbeat = sh_->heartbeat + 1;
                        }
                        // latence estimee : periode du moteur Windows (10 ms) + anneau + tampon de sortie
                        double lat = 10.0 + (rCount + pad + n) * 1000.0 / fs;
                        latAvg = latAvg == 0 ? lat : latAvg * 0.98 + lat * 0.02;
                        latencyMs = (float)latAvg;
                    }
                }
            }
            Sleep(2);
        }
        cap->Stop(); ren->Stop();
        delete eng;
        if (vol) vol->Release();
        ok = stop_ != 0;   // sortie normale = arret demande ; sinon erreur (peripherique perdu) -> on reessaie
        if (!ok) { wcsncpy(lastError, L"Flux audio interrompu (périphérique modifié) : reconnexion…", 255); lastErrorCode = 12; }
    }
    SafeRelease(capC); SafeRelease(renC); SafeRelease(cap); SafeRelease(ren);
    if (wfC) CoTaskMemFree(wfC);
    if (wfR) CoTaskMemFree(wfR);
    SafeRelease(dsrc); SafeRelease(ddst); SafeRelease(en);
    streaming = false;
    return ok;
}
