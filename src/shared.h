// Dynamo - structures partagees entre le moteur audio (APO, dans audiodg.exe)
// et l'interface. Uniquement des types POD, disposition fixe.
#pragma once
#include <stdint.h>

#define DYN_MAGIC        0x314E5944u   // "DYN1"
#define DYN_LAYOUT       3u
#define DYN_EQ_BANDS     10
#define DYN_COMP_BANDS   4             // [0] = mono-bande, [1..3] = grave / medium / aigu
#define DYN_SPEC_SIZE    8192          // tampon circulaire pour l'analyseur de spectre

#define DYN_SHARED_FILE  L"C:\\ProgramData\\Dynamo\\shared.bin"
#define DYN_DATA_DIR     L"C:\\ProgramData\\Dynamo"
#define DYN_STATE_FILE   L"C:\\ProgramData\\Dynamo\\state.bin"
#define DYN_LOG_FILE     L"C:\\ProgramData\\Dynamo\\moteur.log"

// CLSID du moteur : {8B4F2C61-3E7A-4D19-9C55-6A1E0F2D7B93}
#define DYN_CLSID_STR    L"{8B4F2C61-3E7A-4D19-9C55-6A1E0F2D7B93}"
#define DYN_CLSID_INIT   { 0x8b4f2c61, 0x3e7a, 0x4d19, { 0x9c, 0x55, 0x6a, 0x1e, 0x0f, 0x2d, 0x7b, 0x93 } }

enum DynEqType : int32_t {
    EQ_PEAK = 0,
    EQ_LOWSHELF,
    EQ_HIGHSHELF,
    EQ_LOWCUT,     // passe-haut
    EQ_HIGHCUT,    // passe-bas
    EQ_NOTCH,
    EQ_TYPE_COUNT
};

struct DynEqBand {
    int32_t enabled;
    int32_t type;      // DynEqType
    float   freq;      // Hz
    float   gain;      // dB
    float   q;
    int32_t slope;     // coupe-bas/haut : 1=12, 2=24, 3=36, 4=48 dB/oct
};

struct DynCompBand {
    int32_t enabled;
    float   threshold; // dB
    float   ratio;     // 1..20 (>=20 = limiteur)
    float   attack;    // ms
    float   release;   // ms
    float   knee;      // dB
    float   makeup;    // dB
    int32_t autoMakeup;
};

struct DynParams {
    int32_t bypass;
    float   inputGain;       // dB
    float   outputGain;      // dB

    int32_t eqEnabled;
    int32_t eqPost;          // 0 = EQ avant compresseur, 1 = apres
    DynEqBand eq[DYN_EQ_BANDS];

    int32_t compEnabled;
    int32_t compMultiband;
    DynCompBand comp[DYN_COMP_BANDS];
    float   xoverLow;        // Hz (grave/medium)
    float   xoverHigh;       // Hz (medium/aigu)
    int32_t detectorRms;     // 0 = crete, 1 = RMS
    float   scHpf;           // Hz, filtre du detecteur (0 = off)
    float   mix;             // 0..1 compression parallele

    int32_t limEnabled;
    float   limCeiling;      // dBFS
    float   limRelease;      // ms

    float   reserved[24];
};

struct DynMeters {
    volatile float inPeak[2];     // lineaire, max depuis la derniere lecture
    volatile float outPeak[2];
    volatile float grComp[DYN_COMP_BANDS]; // dB positifs
    volatile float grLimiter;
    volatile float compInLevel;   // dB niveau detecte (mono-bande) pour le point sur la courbe
};

struct DynShared {
    uint32_t magic;
    uint32_t layout;
    volatile int32_t paramSeq;    // seqlock : impair = ecriture en cours
    volatile int32_t heartbeat;   // incremente a chaque bloc traite
    volatile uint32_t sampleRate;
    volatile uint32_t channels;
    volatile uint32_t blockFrames;
    volatile uint32_t engineCount;
    DynParams params;
    DynMeters meters;
    volatile uint32_t specWrite;
    float spec[DYN_SPEC_SIZE];
};

// Fichier d'etat persistant : en-tete + parametres
struct DynStateFile {
    uint32_t magic;
    uint32_t layout;
    uint32_t size;
    uint32_t seq;
    DynParams params;
};

inline void DynDefaultParams(DynParams& p) {
    p = DynParams{};
    p.bypass = 0;
    p.inputGain = 0.f;
    p.outputGain = 0.f;
    p.eqEnabled = 1;
    p.eqPost = 0;
    const float freqs[DYN_EQ_BANDS] = { 31.f, 62.f, 125.f, 250.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f, 16000.f };
    for (int i = 0; i < DYN_EQ_BANDS; ++i) {
        p.eq[i].enabled = 1;
        p.eq[i].type = EQ_PEAK;
        p.eq[i].freq = freqs[i];
        p.eq[i].gain = 0.f;
        p.eq[i].q = 1.0f;
        p.eq[i].slope = 1;
    }
    p.eq[0].type = EQ_LOWSHELF;  p.eq[0].q = 0.707f;
    p.eq[9].type = EQ_HIGHSHELF; p.eq[9].q = 0.707f;

    p.compEnabled = 1;
    p.compMultiband = 0;
    for (int b = 0; b < DYN_COMP_BANDS; ++b) {
        DynCompBand& c = p.comp[b];
        c.enabled = 1;
        c.threshold = -18.f;
        c.ratio = 3.f;
        c.attack = 10.f;
        c.release = 150.f;
        c.knee = 6.f;
        c.makeup = 0.f;
        c.autoMakeup = 0;
    }
    p.comp[1].attack = 25.f; p.comp[1].release = 250.f;   // graves plus lents
    p.comp[3].attack = 3.f;  p.comp[3].release = 80.f;    // aigus plus rapides
    p.xoverLow = 150.f;
    p.xoverHigh = 3000.f;
    p.detectorRms = 0;
    p.scHpf = 60.f;
    p.mix = 1.f;

    p.limEnabled = 1;
    p.limCeiling = -0.3f;
    p.limRelease = 60.f;
}

// Validation defensive : le moteur tourne dans audiodg.exe, on ne fait jamais confiance aux valeurs.
inline float DynClampF(float v, float lo, float hi, float def) {
    if (!(v == v)) return def; // NaN
    return v < lo ? lo : (v > hi ? hi : v);
}

inline void DynSanitize(DynParams& p) {
    p.inputGain  = DynClampF(p.inputGain, -24.f, 24.f, 0.f);
    p.outputGain = DynClampF(p.outputGain, -24.f, 24.f, 0.f);
    for (int i = 0; i < DYN_EQ_BANDS; ++i) {
        DynEqBand& e = p.eq[i];
        if (e.type < 0 || e.type >= EQ_TYPE_COUNT) e.type = EQ_PEAK;
        e.freq = DynClampF(e.freq, 10.f, 22000.f, 1000.f);
        e.gain = DynClampF(e.gain, -24.f, 24.f, 0.f);
        e.q    = DynClampF(e.q, 0.1f, 18.f, 1.f);
        if (e.slope < 1 || e.slope > 4) e.slope = 1;
    }
    for (int b = 0; b < DYN_COMP_BANDS; ++b) {
        DynCompBand& c = p.comp[b];
        c.threshold = DynClampF(c.threshold, -60.f, 0.f, -18.f);
        c.ratio     = DynClampF(c.ratio, 1.f, 30.f, 3.f);
        c.attack    = DynClampF(c.attack, 0.05f, 300.f, 10.f);
        c.release   = DynClampF(c.release, 5.f, 3000.f, 150.f);
        c.knee      = DynClampF(c.knee, 0.f, 24.f, 6.f);
        c.makeup    = DynClampF(c.makeup, -12.f, 30.f, 0.f);
    }
    p.xoverLow  = DynClampF(p.xoverLow, 40.f, 1000.f, 150.f);
    p.xoverHigh = DynClampF(p.xoverHigh, 1000.f, 12000.f, 3000.f);
    if (p.xoverHigh < p.xoverLow * 2.f) p.xoverHigh = p.xoverLow * 2.f;
    p.scHpf      = DynClampF(p.scHpf, 0.f, 500.f, 60.f);
    p.mix        = DynClampF(p.mix, 0.f, 1.f, 1.f);
    p.limCeiling = DynClampF(p.limCeiling, -24.f, 0.f, -0.3f);
    p.limRelease = DynClampF(p.limRelease, 1.f, 2000.f, 60.f);
}

#ifdef _WIN32
#include <windows.h>
// Memoire partagee adossee a un fichier : fonctionne meme depuis audiodg.exe (jeton a ecriture restreinte),
// sans espace de noms global ni privilege particulier.
inline DynShared* DynOpenShared(HANDLE* fileOut, HANDLE* mapOut, bool readOnly = false) {
    *fileOut = nullptr; *mapOut = nullptr;
    HANDLE f = CreateFileW(DYN_SHARED_FILE, readOnly ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE),
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           readOnly ? OPEN_EXISTING : OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return nullptr;
    if (readOnly) {
        LARGE_INTEGER sz; GetFileSizeEx(f, &sz);
        if (sz.QuadPart < (LONGLONG)sizeof(DynShared)) { CloseHandle(f); return nullptr; }
    }
    HANDLE m = CreateFileMappingW(f, nullptr, readOnly ? PAGE_READONLY : PAGE_READWRITE, 0, sizeof(DynShared), nullptr);
    if (!m) { CloseHandle(f); return nullptr; }
    DynShared* s = (DynShared*)MapViewOfFile(m, readOnly ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS, 0, 0, sizeof(DynShared));
    if (!s) { CloseHandle(m); CloseHandle(f); return nullptr; }
    *fileOut = f; *mapOut = m;
    return s;
}
#endif
