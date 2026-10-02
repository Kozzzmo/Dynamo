// Banc d'essai : charge DynamoAPO.dll comme le ferait audiodg.exe et fait passer du son.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include "../src/apo_iface.h"
#include "../src/shared.h"

static const GUID SUB_FLOAT  = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID SUB_PCM    = { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const CLSID CLSID_Dyn = DYN_CLSID_INIT;

struct TestMT : IAudioMediaType {
    LONG r = 1; UNCOMPRESSEDAUDIOFORMAT u; WAVEFORMATEX w{};
    TestMT(GUID g, DWORD ch, DWORD bytes, float fs) { u = { g, ch, bytes, bytes * 8, fs, 3 }; }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** p) override { *p = this; AddRef(); return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++r; }
    ULONG STDMETHODCALLTYPE Release() override { return --r; }
    HRESULT STDMETHODCALLTYPE IsCompressedFormat(BOOL* b) override { *b = FALSE; return S_OK; }
    HRESULT STDMETHODCALLTYPE IsEqual(IAudioMediaType*, DWORD* f) override { *f = 0; return S_FALSE; }
    const WAVEFORMATEX* STDMETHODCALLTYPE GetAudioFormat() override { return &w; }
    HRESULT STDMETHODCALLTYPE GetUncompressedAudioFormat(UNCOMPRESSEDAUDIOFORMAT* p) override { *p = u; return S_OK; }
};

// Objet exterieur minimal pour tester l'agregation COM
struct Outer : IUnknown {
    LONG r = 1; IUnknown* inner = nullptr;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** p) override {
        if (IsEqualIID(riid, IID_IUnk)) { *p = this; AddRef(); return S_OK; }
        return inner->QueryInterface(riid, p);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++r; }
    ULONG STDMETHODCALLTYPE Release() override { return --r; }
};

#define CHECK(x, msg) do { if (!(x)) { printf("ECHEC : %s\n", msg); return 1; } else printf("ok   : %s\n", msg); } while (0)

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HMODULE dll = LoadLibraryW(L"DynamoAPO.dll");
    CHECK(dll, "chargement de la DLL");
    typedef HRESULT(__stdcall* GCO)(REFCLSID, REFIID, void**);
    GCO gco = (GCO)GetProcAddress(dll, "DllGetClassObject");
    CHECK(gco, "export DllGetClassObject");
    IClassFactory* cf = nullptr;
    CHECK(SUCCEEDED(gco(CLSID_Dyn, IID_ICF, (void**)&cf)), "fabrique COM");

    // agregation (comme le moteur audio Windows)
    Outer outer;
    IUnknown* innerUnk = nullptr;
    CHECK(SUCCEEDED(cf->CreateInstance(&outer, IID_IUnk, (void**)&innerUnk)), "creation agregee");
    outer.inner = innerUnk;
    IAudioProcessingObject* aggApo = nullptr;
    CHECK(SUCCEEDED(innerUnk->QueryInterface(IID_APO, (void**)&aggApo)), "QI agregee IAudioProcessingObject");
    IUnknown* back = nullptr;
    aggApo->QueryInterface(IID_IUnk, (void**)&back);
    CHECK(back == &outer, "delegation vers l'objet exterieur");
    aggApo->Release(); back->Release();
    innerUnk->Release();

    // instance simple
    IAudioProcessingObject* apo = nullptr;
    CHECK(SUCCEEDED(cf->CreateInstance(nullptr, IID_APO, (void**)&apo)), "creation IAudioProcessingObject");
    IAudioProcessingObjectRT* rt = nullptr; IAudioProcessingObjectConfiguration* cfg = nullptr;
    IAudioSystemEffects* fx = nullptr; IAudioSystemEffects2* fx2 = nullptr;
    CHECK(SUCCEEDED(apo->QueryInterface(IID_APO_RT, (void**)&rt)), "QI IAudioProcessingObjectRT");
    CHECK(SUCCEEDED(apo->QueryInterface(IID_APO_CFG, (void**)&cfg)), "QI IAudioProcessingObjectConfiguration");
    CHECK(SUCCEEDED(apo->QueryInterface(IID_SFX, (void**)&fx)), "QI IAudioSystemEffects");
    CHECK(SUCCEEDED(apo->QueryInterface(IID_SFX2, (void**)&fx2)), "QI IAudioSystemEffects2");
    LPGUID ids; UINT nids = 99;
    CHECK(fx2->GetEffectsList(&ids, &nids, nullptr) == S_OK && nids == 0, "GetEffectsList");

    APO_REG_PROPERTIES* rp = nullptr;
    CHECK(SUCCEEDED(apo->GetRegistrationProperties(&rp)) && IsEqualCLSID(rp->clsid, CLSID_Dyn), "proprietes d'enregistrement");
    CoTaskMemFree(rp);
    CHECK(SUCCEEDED(apo->Initialize(0, nullptr)), "Initialize");
    HNSTIME lat = 123; apo->GetLatency(&lat);
    CHECK(lat == 0, "latence nulle");

    TestMT fOut(SUB_FLOAT, 2, 4, 48000), fIn(SUB_FLOAT, 2, 4, 48000), fPcm(SUB_PCM, 2, 2, 44100);
    IAudioMediaType* sup = nullptr;
    CHECK(apo->IsInputFormatSupported(&fOut, &fIn, &sup) == S_OK && sup == &fIn, "format float32 accepte");
    sup = nullptr;
    HRESULT hr = apo->IsInputFormatSupported(&fOut, &fPcm, &sup);
    UNCOMPRESSEDAUDIOFORMAT su{};
    if (sup) sup->GetUncompressedAudioFormat(&su);
    CHECK(hr == S_FALSE && sup && IsEqualGUID(su.guidFormatType, SUB_FLOAT) && su.fFramesPerSecond == 48000, "PCM16 -> suggestion float32 48k");
    if (sup) sup->Release();

    const UINT32 FR = 480, CH = 2;
    std::vector<float> buf(FR * CH);
    APO_CONNECTION_DESCRIPTOR din = { APO_CONNECTION_BUFFER_TYPE_EXTERNAL, (UINT_PTR)buf.data(), FR, &fIn, 0 };
    APO_CONNECTION_DESCRIPTOR dout = { APO_CONNECTION_BUFFER_TYPE_EXTERNAL, (UINT_PTR)buf.data(), FR, &fOut, 0 };
    APO_CONNECTION_DESCRIPTOR* pin = &din; APO_CONNECTION_DESCRIPTOR* pout = &dout;
    CHECK(SUCCEEDED(cfg->LockForProcess(1, &pin, 1, &pout)), "LockForProcess");

    HANDLE sf, map;
    DynShared* sh = DynOpenShared(&sf, &map);
    CHECK(sh && sh->magic == DYN_MAGIC, "memoire partagee visible par l'interface");

    // parametres : compresseur T-20 R4 sans genou, limiteur off
    DynParams p = sh->params;
    p.compEnabled = 1; p.compMultiband = 0; p.limEnabled = 0; p.scHpf = 0;
    p.comp[0].threshold = -20; p.comp[0].ratio = 4; p.comp[0].knee = 0; p.comp[0].attack = 1; p.comp[0].release = 50; p.comp[0].makeup = 0;
    p.comp[0].autoMakeup = 0;
    for (int i = 0; i < DYN_EQ_BANDS; i++) p.eq[i].gain = 0;
    InterlockedIncrement((LONG*)&sh->paramSeq); memcpy((void*)&sh->params, &p, sizeof(p)); InterlockedIncrement((LONG*)&sh->paramSeq);

    APO_CONNECTION_PROPERTY cin = { (UINT_PTR)buf.data(), FR, BUFFER_VALID, 0 };
    APO_CONNECTION_PROPERTY cout = { (UINT_PTR)buf.data(), 0, BUFFER_INVALID, 0 };
    APO_CONNECTION_PROPERTY* ppin = &cin; APO_CONNECTION_PROPERTY* ppout = &cout;
    double amp = pow(10.0, -8.0 / 20), ph = 0, peak = 0;
    for (int blk = 0; blk < 200; ++blk) {
        for (UINT32 i = 0; i < FR; ++i) { float v = (float)(amp * sin(ph)); ph += 2 * 3.14159265358979 * 1000 / 48000; buf[i * 2] = v; buf[i * 2 + 1] = v; }
        cin.u32ValidFrameCount = FR; cin.u32BufferFlags = BUFFER_VALID;
        rt->APOProcess(1, &ppin, 1, &ppout);
        if (blk > 100) for (float v : buf) if (fabs(v) > peak) peak = fabs(v);
    }
    double outDb = 20 * log10(peak);
    printf("       sortie %.2f dBFS pour -8 dBFS en entree (attendu ~ -17)\n", outDb);
    CHECK(outDb < -15.5 && outDb > -18.0, "compression appliquee dans le moteur");
    CHECK(cout.u32BufferFlags == BUFFER_VALID && cout.u32ValidFrameCount == FR, "drapeaux de sortie");
    CHECK(sh->heartbeat >= 200 && sh->sampleRate == 48000 && sh->channels == 2, "pouls + format publies");
    CHECK(sh->meters.grComp[0] > 2.0f, "vumetre de reduction de gain");
    printf("       GR = %.2f dB, crete entree %.3f, sortie %.3f\n", sh->meters.grComp[0], sh->meters.inPeak[0], sh->meters.outPeak[0]);

    // tampon silencieux
    cin.u32BufferFlags = BUFFER_SILENT;
    for (int blk = 0; blk < 50; ++blk) { cin.u32BufferFlags = BUFFER_SILENT; rt->APOProcess(1, &ppin, 1, &ppout); }
    CHECK(cout.u32BufferFlags == BUFFER_SILENT, "silence propage");

    // contournement
    p.bypass = 1;
    InterlockedIncrement((LONG*)&sh->paramSeq); memcpy((void*)&sh->params, &p, sizeof(p)); InterlockedIncrement((LONG*)&sh->paramSeq);
    peak = 0;
    for (int blk = 0; blk < 20; ++blk) {
        for (UINT32 i = 0; i < FR; ++i) { float v = (float)(amp * sin(ph)); ph += 2 * 3.14159265358979 * 1000 / 48000; buf[i * 2] = v; buf[i * 2 + 1] = v; }
        cin.u32BufferFlags = BUFFER_VALID;
        rt->APOProcess(1, &ppin, 1, &ppout);
        if (blk > 5) for (float v : buf) if (fabs(v) > peak) peak = fabs(v);
    }
    printf("       contournement : %.2f dBFS\n", 20 * log10(peak));
    CHECK(fabs(20 * log10(peak) + 8) < 0.05, "contournement transparent");

    CHECK(SUCCEEDED(cfg->UnlockForProcess()), "UnlockForProcess");
    rt->Release(); cfg->Release(); fx->Release(); fx2->Release();
    ULONG left = apo->Release();
    CHECK(left == 0, "liberation complete de l'objet");
    cf->Release();
    printf("TOUS LES TESTS OK\n");
    return 0;
}
