// Dynamo - moteur audio (Audio Processing Object) charge par Windows dans audiodg.exe.
// Insere comme effet de sortie (EFX) sur le peripherique choisi : zero latence ajoutee.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <mmreg.h>
#include <sddl.h>
#include <xmmintrin.h>
#include <stdio.h>
#include <new>

#include "shared.h"
#include "dsp.h"

#include "apo_iface.h"

static const CLSID CLSID_Dynamo = DYN_CLSID_INIT;

static HMODULE g_module = nullptr;
static volatile LONG g_objects = 0;

// ---------------------------------------------------------------- journal (hors thread temps reel)
static void Log(const char* fmt, ...) {
    HANDLE f = CreateFileW(DYN_LOG_FILE, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER sz;
    if (GetFileSizeEx(f, &sz) && sz.QuadPart > 512 * 1024) {
        CloseHandle(f);
        f = CreateFileW(DYN_LOG_FILE, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) return;
    }
    char line[768];
    SYSTEMTIME st; GetLocalTime(&st);
    int n = snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d:%02d.%03d [pid %lu] ", st.wYear, st.wMonth, st.wDay,
                     st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, GetCurrentProcessId());
    va_list ap; va_start(ap, fmt);
    n += vsnprintf(line + n, sizeof(line) - n - 3, fmt, ap);
    va_end(ap);
    if (n > (int)sizeof(line) - 3) n = sizeof(line) - 3;
    line[n++] = '\r'; line[n++] = '\n';
    DWORD w; WriteFile(f, line, n, &w, nullptr);
    CloseHandle(f);
}

// ---------------------------------------------------------------- lien avec l'interface (memoire partagee globale)
namespace link {

static DynShared* g_sh = nullptr;
static HANDLE g_map = nullptr;
static bool g_fileMode = false;
static volatile LONG g_initState = 0; // 0 = non, 1 = en cours, 2 = pret
static HANDLE g_thread = nullptr;
static volatile LONG g_stop = 0;

static bool ReadStateFile(DynParams& out, FILETIME* ft) {
    HANDLE f = CreateFileW(DYN_STATE_FILE, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DynStateFile sf; DWORD r = 0;
    BOOL ok = ReadFile(f, &sf, sizeof(sf), &r, nullptr);
    if (ft) GetFileTime(f, nullptr, nullptr, ft);
    CloseHandle(f);
    if (!ok || r != sizeof(sf) || sf.magic != DYN_MAGIC || sf.layout != DYN_LAYOUT || sf.size != sizeof(DynParams)) return false;
    out = sf.params;
    DynSanitize(out);
    return true;
}

static void WriteParams(const DynParams& p) {
    InterlockedIncrement((LONG*)&g_sh->paramSeq);
    MemoryBarrier();
    memcpy((void*)&g_sh->params, &p, sizeof(p));
    MemoryBarrier();
    InterlockedIncrement((LONG*)&g_sh->paramSeq);
}

// Thread de surveillance du fichier d'etat (mode secours si la memoire partagee n'est pas creable)
static DWORD WINAPI Watcher(void*) {
    FILETIME last = {};
    while (!g_stop) {
        Sleep(200);
        WIN32_FILE_ATTRIBUTE_DATA a;
        if (!GetFileAttributesExW(DYN_STATE_FILE, GetFileExInfoStandard, &a)) continue;
        if (CompareFileTime(&a.ftLastWriteTime, &last) == 0) continue;
        DynParams p;
        if (ReadStateFile(p, nullptr)) { WriteParams(p); last = a.ftLastWriteTime; }
    }
    return 0;
}

// Appele hors temps reel (LockForProcess)
static void Init() {
    if (InterlockedCompareExchange(&g_initState, 1, 0) != 0) {
        while (g_initState == 1) Sleep(1);
        return;
    }
    CreateDirectoryW(DYN_DATA_DIR, nullptr);

    HANDLE sharedFile = nullptr;
    g_sh = DynOpenShared(&sharedFile, &g_map);
    bool existed = g_sh && g_sh->magic == DYN_MAGIC && g_sh->layout == DYN_LAYOUT;
    if (!g_sh) Log("Ouverture de shared.bin impossible (%lu) : mode fichier", GetLastError());

    if (!g_sh) {
        g_fileMode = true;
        g_sh = (DynShared*)VirtualAlloc(nullptr, sizeof(DynShared), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        existed = false;
    }
    if (g_sh && (!existed || g_sh->magic != DYN_MAGIC || g_sh->layout != DYN_LAYOUT)) {
        DynParams p;
        if (!ReadStateFile(p, nullptr)) DynDefaultParams(p);
        g_sh->layout = DYN_LAYOUT;
        g_sh->paramSeq = 0;
        WriteParams(p);
        MemoryBarrier();
        g_sh->magic = DYN_MAGIC;
    }
    // en mode fichier uniquement : surveille state.bin (sinon l'interface ecrit directement en memoire partagee)
    if (g_sh && g_fileMode) g_thread = CreateThread(nullptr, 0, Watcher, nullptr, 0, nullptr);
    Log("Lien initialise : %s, memoire %s", g_fileMode ? "mode fichier" : "memoire partagee", existed ? "existante" : "creee");
    InterlockedExchange(&g_initState, 2);
}

} // namespace link

// ---------------------------------------------------------------- type de media (suggestion de format)
class MediaType : public IAudioMediaType {
public:
    explicit MediaType(const UNCOMPRESSEDAUDIOFORMAT& u) : ref_(1), u_(u) {
        memset(&wfx_, 0, sizeof(wfx_));
        wfx_.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfx_.Format.nChannels = (WORD)u.dwSamplesPerFrame;
        wfx_.Format.nSamplesPerSec = (DWORD)u.fFramesPerSecond;
        wfx_.Format.wBitsPerSample = (WORD)(u.dwBytesPerSampleContainer * 8);
        wfx_.Format.nBlockAlign = (WORD)(u.dwBytesPerSampleContainer * u.dwSamplesPerFrame);
        wfx_.Format.nAvgBytesPerSec = wfx_.Format.nBlockAlign * wfx_.Format.nSamplesPerSec;
        wfx_.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wfx_.Samples.wValidBitsPerSample = (WORD)u.dwValidBitsPerSample;
        wfx_.dwChannelMask = u.dwChannelMask;
        wfx_.SubFormat = u.guidFormatType;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnk) || IsEqualIID(riid, IID_AMT)) { *ppv = static_cast<IAudioMediaType*>(this); AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&ref_); if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE IsCompressedFormat(BOOL* p) override { if (!p) return E_POINTER; *p = FALSE; return S_OK; }
    HRESULT STDMETHODCALLTYPE IsEqual(IAudioMediaType* other, DWORD* flags) override {
        if (!other || !flags) return E_POINTER;
        UNCOMPRESSEDAUDIOFORMAT o;
        HRESULT hr = other->GetUncompressedAudioFormat(&o);
        *flags = 0;
        if (FAILED(hr)) return S_FALSE;
        if (IsEqualGUID(o.guidFormatType, u_.guidFormatType)) *flags |= AUDIOMEDIATYPE_EQUAL_FORMAT_TYPES;
        if (o.dwSamplesPerFrame == u_.dwSamplesPerFrame && o.dwBytesPerSampleContainer == u_.dwBytesPerSampleContainer &&
            o.dwValidBitsPerSample == u_.dwValidBitsPerSample && o.fFramesPerSecond == u_.fFramesPerSecond)
            *flags |= AUDIOMEDIATYPE_EQUAL_FORMAT_DATA | AUDIOMEDIATYPE_EQUAL_FORMAT_USER_DATA;
        return (*flags == (AUDIOMEDIATYPE_EQUAL_FORMAT_TYPES | AUDIOMEDIATYPE_EQUAL_FORMAT_DATA | AUDIOMEDIATYPE_EQUAL_FORMAT_USER_DATA)) ? S_OK : S_FALSE;
    }
    const WAVEFORMATEX* STDMETHODCALLTYPE GetAudioFormat() override { return &wfx_.Format; }
    HRESULT STDMETHODCALLTYPE GetUncompressedAudioFormat(UNCOMPRESSEDAUDIOFORMAT* p) override { if (!p) return E_POINTER; *p = u_; return S_OK; }
private:
    ~MediaType() = default;
    LONG ref_;
    UNCOMPRESSEDAUDIOFORMAT u_;
    WAVEFORMATEXTENSIBLE wfx_;
};

static bool IsFloat32(const UNCOMPRESSEDAUDIOFORMAT& u) {
    return IsEqualGUID(u.guidFormatType, SUBTYPE_FLOAT) && u.dwBytesPerSampleContainer == 4 && u.dwValidBitsPerSample == 32;
}

// ---------------------------------------------------------------- l'APO
class DynamoApo : public IAudioProcessingObject,
                  public IAudioProcessingObjectRT,
                  public IAudioProcessingObjectConfiguration,
                  public IAudioSystemEffects2 {
public:
    explicit DynamoApo(IUnknown* outer) : ref_(1) {
        inner_.self = this;
        outer_ = outer ? outer : static_cast<IUnknown*>(&inner_);
        InterlockedIncrement(&g_objects);
    }

    IUnknown* Inner() { return &inner_; }

    // IUnknown delegue a l'objet exterieur (agregation COM)
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override { return outer_->QueryInterface(riid, ppv); }
    ULONG STDMETHODCALLTYPE AddRef() override { return outer_->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() override { return outer_->Release(); }

    // IAudioProcessingObject
    HRESULT STDMETHODCALLTYPE Reset() override { resetPending_ = 1; return S_OK; }
    HRESULT STDMETHODCALLTYPE GetLatency(HNSTIME* t) override { if (!t) return E_POINTER; *t = 0; return S_OK; }

    HRESULT STDMETHODCALLTYPE GetRegistrationProperties(APO_REG_PROPERTIES** pp) override {
        if (!pp) return E_POINTER;
        APO_REG_PROPERTIES* r = (APO_REG_PROPERTIES*)CoTaskMemAlloc(sizeof(APO_REG_PROPERTIES));
        if (!r) return E_OUTOFMEMORY;
        FillRegProps(*r);
        *pp = r;
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Initialize(UINT32 cb, BYTE* data) override {
        (void)data;
        Log("Initialize (taille %u)", cb);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE IsInputFormatSupported(IAudioMediaType* opposite, IAudioMediaType* req, IAudioMediaType** sup) override {
        return CheckFormat(opposite, req, sup);
    }
    HRESULT STDMETHODCALLTYPE IsOutputFormatSupported(IAudioMediaType* opposite, IAudioMediaType* req, IAudioMediaType** sup) override {
        return CheckFormat(opposite, req, sup);
    }
    HRESULT STDMETHODCALLTYPE GetInputChannelCount(UINT32* n) override {
        if (!n) return E_POINTER;
        *n = channels_;
        return S_OK;
    }

    // IAudioProcessingObjectConfiguration
    HRESULT STDMETHODCALLTYPE LockForProcess(UINT32 nIn, APO_CONNECTION_DESCRIPTOR** in, UINT32 nOut, APO_CONNECTION_DESCRIPTOR** out) override {
        if (locked_) return APOERR_APO_LOCKED;
        if (nIn != 1 || nOut != 1 || !in || !out || !in[0] || !out[0]) return APOERR_NUM_CONNECTIONS_INVALID;
        if (!in[0]->pFormat || !out[0]->pFormat) return E_POINTER;
        UNCOMPRESSEDAUDIOFORMAT fi, fo;
        HRESULT hr = in[0]->pFormat->GetUncompressedAudioFormat(&fi);
        if (FAILED(hr)) return hr;
        hr = out[0]->pFormat->GetUncompressedAudioFormat(&fo);
        if (FAILED(hr)) return hr;
        if (!IsFloat32(fi) || !IsFloat32(fo) || fi.dwSamplesPerFrame != fo.dwSamplesPerFrame ||
            fi.fFramesPerSecond != fo.fFramesPerSecond || fi.dwSamplesPerFrame == 0) {
            Log("LockForProcess : format refuse (%lu canaux, %.0f Hz)", fi.dwSamplesPerFrame, fi.fFramesPerSecond);
            return APOERR_INVALID_CONNECTION_FORMAT;
        }
        link::Init();
        channels_ = fi.dwSamplesPerFrame;
        rate_ = fi.fFramesPerSecond;
        maxFrames_ = in[0]->u32MaxFrameCount;
        if (!engine_) engine_ = new (std::nothrow) dyn::Engine();
        if (!engine_) return E_OUTOFMEMORY;
        if (link::g_sh) {
            DynParams p;
            if (FetchParams(p)) engine_->applyParams(p);
            specPos_ = link::g_sh->specWrite;
            InterlockedIncrement((LONG*)&link::g_sh->engineCount);
        }
        engine_->prepare(rate_, (int)channels_);
        locked_ = true;
        Log("LockForProcess : %u canaux, %.0f Hz, %u trames max", channels_, rate_, maxFrames_);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE UnlockForProcess() override {
        if (!locked_) return APOERR_ALREADY_UNLOCKED;
        locked_ = false;
        if (link::g_sh) InterlockedDecrement((LONG*)&link::g_sh->engineCount);
        Log("UnlockForProcess");
        return S_OK;
    }

    // IAudioProcessingObjectRT (thread temps reel : aucune allocation, aucun verrou, aucune E/S)
    void STDMETHODCALLTYPE APOProcess(UINT32 nIn, APO_CONNECTION_PROPERTY** in, UINT32 nOut, APO_CONNECTION_PROPERTY** out) override {
        if (nIn < 1 || nOut < 1 || !in || !out || !in[0] || !out[0]) return;
        APO_CONNECTION_PROPERTY* ci = in[0];
        APO_CONNECTION_PROPERTY* co = out[0];
        const UINT32 frames = ci->u32ValidFrameCount;
        float* src = reinterpret_cast<float*>(ci->pBuffer);
        float* dst = reinterpret_cast<float*>(co->pBuffer);

        if (ci->u32BufferFlags != BUFFER_VALID && ci->u32BufferFlags != BUFFER_SILENT) return;
        if (!engine_ || !locked_ || !src || !dst) {
            if (src && dst && src != dst) memcpy(dst, src, (size_t)frames * channels_ * sizeof(float));
            co->u32ValidFrameCount = frames;
            co->u32BufferFlags = ci->u32BufferFlags;
            return;
        }

        const unsigned int oldCsr = _mm_getcsr();
        _mm_setcsr(oldCsr | 0x8040); // flush-to-zero + denormals-are-zero

        const bool silentIn = (ci->u32BufferFlags == BUFFER_SILENT);
        if (silentIn) memset(src, 0, (size_t)frames * channels_ * sizeof(float));

        if (resetPending_) { resetPending_ = 0; engine_->reset(); }

        DynShared* sh = link::g_sh;
        if (sh) {
            DynParams p;
            if (FetchParams(p)) engine_->applyParams(p);
        }

        dyn::Engine::BlockMeters m;
        engine_->process(src, dst, frames, m, sh ? sh->spec : nullptr, DYN_SPEC_SIZE - 1, specPos_);

        co->u32ValidFrameCount = frames;
        if (silentIn) {
            // ne signaler du son que s'il y en a reellement (queues de filtres)
            bool silent = true;
            const size_t n = (size_t)frames * channels_;
            for (size_t i = 0; i < n; ++i) if (fabsf(dst[i]) > 1e-9f) { silent = false; break; }
            co->u32BufferFlags = silent ? BUFFER_SILENT : BUFFER_VALID;
        } else {
            co->u32BufferFlags = BUFFER_VALID;
        }

        if (sh) {
            DynMeters& M = sh->meters;
            for (int i = 0; i < 2; ++i) {
                if (m.inPeak[i] > M.inPeak[i]) M.inPeak[i] = m.inPeak[i];
                if (m.outPeak[i] > M.outPeak[i]) M.outPeak[i] = m.outPeak[i];
            }
            for (int b = 0; b < DYN_COMP_BANDS; ++b) if (m.gr[b] > M.grComp[b]) M.grComp[b] = m.gr[b];
            if (m.grLim > M.grLimiter) M.grLimiter = m.grLim;
            M.compInLevel = m.level;
            sh->specWrite = specPos_;
            sh->sampleRate = (uint32_t)rate_;
            sh->channels = channels_;
            sh->blockFrames = frames;
            sh->heartbeat = sh->heartbeat + 1;
        }
        _mm_setcsr(oldCsr);
    }

    UINT32 STDMETHODCALLTYPE CalcInputFrames(UINT32 n) override { return n; }
    UINT32 STDMETHODCALLTYPE CalcOutputFrames(UINT32 n) override { return n; }

    // IAudioSystemEffects2
    HRESULT STDMETHODCALLTYPE GetEffectsList(LPGUID* ids, UINT* count, HANDLE) override {
        if (!ids || !count) return E_POINTER;
        *ids = nullptr; *count = 0;
        return S_OK;
    }

    static void FillRegProps(APO_REG_PROPERTIES& r) {
        memset(&r, 0, sizeof(r));
        r.clsid = CLSID_Dynamo;
        r.Flags = (APO_FLAG)(APO_FLAG_INPLACE | APO_FLAG_SAMPLESPERFRAME_MUST_MATCH | APO_FLAG_FRAMESPERSECOND_MUST_MATCH | APO_FLAG_BITSPERSAMPLE_MUST_MATCH);
        wcscpy(r.szFriendlyName, L"Dynamo");
        wcscpy(r.szCopyrightInfo, L"Dynamo");
        r.u32MajorVersion = 1;
        r.u32MinorVersion = 0;
        r.u32MinInputConnections = 1;
        r.u32MaxInputConnections = 1;
        r.u32MinOutputConnections = 1;
        r.u32MaxOutputConnections = 1;
        r.u32MaxInstances = 0xFFFFFFFF;
        r.u32NumAPOInterfaces = 1;
        r.iidAPOInterfaceList[0] = IID_APO;
    }

private:
    struct InnerUnknown : public IUnknown {
        DynamoApo* self = nullptr;
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
            if (!ppv) return E_POINTER;
            *ppv = nullptr;
            if (IsEqualIID(riid, IID_IUnk))          *ppv = static_cast<IUnknown*>(this);
            else if (IsEqualIID(riid, IID_APO))      *ppv = static_cast<IAudioProcessingObject*>(self);
            else if (IsEqualIID(riid, IID_APO_RT))   *ppv = static_cast<IAudioProcessingObjectRT*>(self);
            else if (IsEqualIID(riid, IID_APO_CFG))  *ppv = static_cast<IAudioProcessingObjectConfiguration*>(self);
            else if (IsEqualIID(riid, IID_SFX2))     *ppv = static_cast<IAudioSystemEffects2*>(self);
            else if (IsEqualIID(riid, IID_SFX))      *ppv = static_cast<IAudioSystemEffects*>(static_cast<IAudioSystemEffects2*>(self));
            else return E_NOINTERFACE;
            reinterpret_cast<IUnknown*>(*ppv)->AddRef();
            return S_OK;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&self->ref_); }
        ULONG STDMETHODCALLTYPE Release() override {
            LONG r = InterlockedDecrement(&self->ref_);
            if (r == 0) delete self;
            return r;
        }
    };

    ~DynamoApo() {
        if (locked_ && link::g_sh) InterlockedDecrement((LONG*)&link::g_sh->engineCount);
        delete engine_;
        InterlockedDecrement(&g_objects);
    }

    bool FetchParams(DynParams& p) {
        DynShared* sh = link::g_sh;
        if (!sh || sh->magic != DYN_MAGIC) return false;
        const int32_t s1 = sh->paramSeq;
        if ((s1 & 1) || s1 == lastSeq_) return false;
        MemoryBarrier();
        memcpy(&p, (const void*)&sh->params, sizeof(p));
        MemoryBarrier();
        if (sh->paramSeq != s1) return false;
        lastSeq_ = s1;
        return true;
    }

    HRESULT CheckFormat(IAudioMediaType* opposite, IAudioMediaType* req, IAudioMediaType** sup) {
        if (!req || !sup) return E_POINTER;
        *sup = nullptr;
        UNCOMPRESSEDAUDIOFORMAT r;
        HRESULT hr = req->GetUncompressedAudioFormat(&r);
        if (FAILED(hr)) return hr;
        UNCOMPRESSEDAUDIOFORMAT want = r;
        if (opposite) {
            UNCOMPRESSEDAUDIOFORMAT o;
            if (SUCCEEDED(opposite->GetUncompressedAudioFormat(&o))) {
                want.dwSamplesPerFrame = o.dwSamplesPerFrame;
                want.fFramesPerSecond = o.fFramesPerSecond;
                if (o.dwChannelMask) want.dwChannelMask = o.dwChannelMask;
            }
        }
        want.guidFormatType = SUBTYPE_FLOAT;
        want.dwBytesPerSampleContainer = 4;
        want.dwValidBitsPerSample = 32;
        if (IsFloat32(r) && r.dwSamplesPerFrame == want.dwSamplesPerFrame && r.fFramesPerSecond == want.fFramesPerSecond) {
            req->AddRef();
            *sup = req;
            return S_OK;
        }
        MediaType* mt = new (std::nothrow) MediaType(want);
        if (!mt) return E_OUTOFMEMORY;
        *sup = mt;
        return S_FALSE;
    }

    InnerUnknown inner_;
    IUnknown* outer_;
    LONG ref_;
    dyn::Engine* engine_ = nullptr;
    bool locked_ = false;
    UINT32 channels_ = 2;
    float rate_ = 48000.f;
    UINT32 maxFrames_ = 0;
    int32_t lastSeq_ = -1;
    unsigned specPos_ = 0;
    volatile LONG resetPending_ = 0;
};

// ---------------------------------------------------------------- fabrique COM
class Factory : public IClassFactory {
public:
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        if (IsEqualIID(riid, IID_IUnk) || IsEqualIID(riid, IID_ICF)) { *ppv = static_cast<IClassFactory*>(this); AddRef(); return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&ref_); }
    ULONG STDMETHODCALLTYPE Release() override { LONG r = InterlockedDecrement(&ref_); if (!r) delete this; return r; }
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (outer && !IsEqualIID(riid, IID_IUnk)) return CLASS_E_NOAGGREGATION;
        DynamoApo* apo = new (std::nothrow) DynamoApo(outer);
        if (!apo) return E_OUTOFMEMORY;
        HRESULT hr = apo->Inner()->QueryInterface(riid, ppv);
        apo->Inner()->Release();
        return hr;
    }
    HRESULT STDMETHODCALLTYPE LockServer(BOOL) override { return S_OK; }
private:
    LONG ref_ = 1;
};

extern "C" {

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { g_module = h; DisableThreadLibraryCalls(h); }
    else if (reason == DLL_PROCESS_DETACH) { link::g_stop = 1; }
    return TRUE;
}

HRESULT __stdcall DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (!IsEqualCLSID(clsid, CLSID_Dynamo)) return CLASS_E_CLASSNOTAVAILABLE;
    Factory* f = new (std::nothrow) Factory();
    if (!f) return E_OUTOFMEMORY;
    HRESULT hr = f->QueryInterface(riid, ppv);
    f->Release();
    return hr;
}

// Le module reste charge : un thread de surveillance peut tourner.
HRESULT __stdcall DllCanUnloadNow() { return S_FALSE; }

// Aide au diagnostic : proprietes d'enregistrement (utilise par l'installateur)
__declspec(dllexport) void __stdcall DynamoGetRegProps(APO_REG_PROPERTIES* r) { DynamoApo::FillRegProps(*r); }

}
