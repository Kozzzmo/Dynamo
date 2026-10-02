// Simule audiodg.exe : charge le moteur et lui fait traiter un signal musical en temps reel.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <vector>
#include "../src/apo_iface.h"
#include "../src/shared.h"
struct MT : IAudioMediaType {
    UNCOMPRESSEDAUDIOFORMAT u{ SUBTYPE_FLOAT, 2, 4, 32, 48000.f, 3 }; WAVEFORMATEX w{};
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void** p) override { *p = this; return S_OK; }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; } ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE IsCompressedFormat(BOOL* b) override { *b = 0; return S_OK; }
    HRESULT STDMETHODCALLTYPE IsEqual(IAudioMediaType*, DWORD* f) override { *f = 0; return S_FALSE; }
    const WAVEFORMATEX* STDMETHODCALLTYPE GetAudioFormat() override { return &w; }
    HRESULT STDMETHODCALLTYPE GetUncompressedAudioFormat(UNCOMPRESSEDAUDIOFORMAT* p) override { *p = u; return S_OK; }
};
int main(int argc, char** argv) {
    int secs = argc > 1 ? atoi(argv[1]) : 30;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    HMODULE dll = LoadLibraryW(L"DynamoAPO.dll");
    auto gco = (HRESULT(__stdcall*)(REFCLSID, REFIID, void**))GetProcAddress(dll, "DllGetClassObject");
    CLSID cl = DYN_CLSID_INIT; IClassFactory* cf; gco(cl, IID_ICF, (void**)&cf);
    IAudioProcessingObject* apo; cf->CreateInstance(nullptr, IID_APO, (void**)&apo);
    IAudioProcessingObjectRT* rt; apo->QueryInterface(IID_APO_RT, (void**)&rt);
    IAudioProcessingObjectConfiguration* cfg; apo->QueryInterface(IID_APO_CFG, (void**)&cfg);
    MT fmt; const UINT32 FR = 480; std::vector<float> buf(FR * 2);
    APO_CONNECTION_DESCRIPTOR d = { APO_CONNECTION_BUFFER_TYPE_EXTERNAL, (UINT_PTR)buf.data(), FR, &fmt, 0 };
    APO_CONNECTION_DESCRIPTOR* pd = &d; cfg->LockForProcess(1, &pd, 1, &pd);
    APO_CONNECTION_PROPERTY c = { (UINT_PTR)buf.data(), FR, BUFFER_VALID, 0 }; APO_CONNECTION_PROPERTY* pc = &c;
    double t = 0; unsigned seed = 1; double lp = 0, bass = 0;
    for (int blk = 0; blk < secs * 100; ++blk) {
        for (UINT32 i = 0; i < FR; ++i, t += 1.0 / 48000) {
            double beat = fmod(t, 0.5); double env = exp(-beat * 9);
            seed = seed * 1664525u + 1013904223u; double n = ((seed >> 8) / 16777216.0 - 0.5);
            lp += 0.05 * (n - lp);
            bass = 0.55 * env * sin(2 * 3.14159 * 55 * t) + 0.15 * sin(2 * 3.14159 * 110 * t);
            double mid = 0.12 * sin(2 * 3.14159 * 440 * t + sin(t * 3)) + 0.06 * sin(2 * 3.14159 * 880 * t);
            double hat = (fmod(t, 0.25) < 0.03 ? 0.15 : 0.02) * n;
            double v = (bass + mid + hat + 0.3 * lp) * (0.6 + 0.4 * sin(t * 0.7));
            buf[i * 2] = (float)v; buf[i * 2 + 1] = (float)(v * 0.93);
        }
        c.u32BufferFlags = BUFFER_VALID; c.u32ValidFrameCount = FR;
        rt->APOProcess(1, &pc, 1, &pc);
        Sleep(10);
    }
    return 0;
}
