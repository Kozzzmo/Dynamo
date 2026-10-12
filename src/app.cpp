// Dynamo - etat de l'application : lien moteur, presets, reglages, vumetres, spectre.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

#include "app.h"
#include "dsp.h"
#include "version.h"

using sys::ToUtf8;
using sys::FromUtf8;

// ---------------------------------------------------------------- serialisation texte des parametres
static std::string SerializeParams(const DynParams& p) {
    char b[256];
    std::string s = "[Dynamo]\n";
    auto kv = [&](const char* k, double v) { snprintf(b, sizeof(b), "%s=%.4g\n", k, v); s += b; };
    kv("bypass", p.bypass); kv("inputGain", p.inputGain); kv("outputGain", p.outputGain);
    kv("eqEnabled", p.eqEnabled); kv("eqPost", p.eqPost);
    for (int i = 0; i < DYN_EQ_BANDS; ++i) {
        const DynEqBand& e = p.eq[i];
        snprintf(b, sizeof(b), "eq%d=%d,%d,%.4g,%.4g,%.4g,%d\n", i, e.enabled, e.type, e.freq, e.gain, e.q, e.slope); s += b;
    }
    kv("compEnabled", p.compEnabled); kv("compMultiband", p.compMultiband);
    for (int i = 0; i < DYN_COMP_BANDS; ++i) {
        const DynCompBand& c = p.comp[i];
        snprintf(b, sizeof(b), "comp%d=%d,%.4g,%.4g,%.4g,%.4g,%.4g,%.4g,%d\n", i, c.enabled, c.threshold, c.ratio, c.attack, c.release, c.knee, c.makeup, c.autoMakeup);
        s += b;
    }
    kv("xoverLow", p.xoverLow); kv("xoverHigh", p.xoverHigh); kv("detectorRms", p.detectorRms);
    kv("scHpf", p.scHpf); kv("mix", p.mix);
    kv("limEnabled", p.limEnabled); kv("limCeiling", p.limCeiling); kv("limRelease", p.limRelease);
    return s;
}

static bool ParseParams(const std::string& text, DynParams& p) {
    DynDefaultParams(p);
    std::istringstream in(text);
    std::string line;
    bool any = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        float f = (float)atof(v.c_str());
        int n = atoi(v.c_str());
        any = true;
        if (k == "bypass") p.bypass = n;
        else if (k == "inputGain") p.inputGain = f;
        else if (k == "outputGain") p.outputGain = f;
        else if (k == "eqEnabled") p.eqEnabled = n;
        else if (k == "eqPost") p.eqPost = n;
        else if (k == "compEnabled") p.compEnabled = n;
        else if (k == "compMultiband") p.compMultiband = n;
        else if (k == "xoverLow") p.xoverLow = f;
        else if (k == "xoverHigh") p.xoverHigh = f;
        else if (k == "detectorRms") p.detectorRms = n;
        else if (k == "scHpf") p.scHpf = f;
        else if (k == "mix") p.mix = f;
        else if (k == "limEnabled") p.limEnabled = n;
        else if (k == "limCeiling") p.limCeiling = f;
        else if (k == "limRelease") p.limRelease = f;
        else if (k.size() == 3 && k.compare(0, 2, "eq") == 0 && isdigit((unsigned char)k[2])) {
            DynEqBand& e = p.eq[k[2] - '0'];
            sscanf(v.c_str(), "%d,%d,%f,%f,%f,%d", &e.enabled, &e.type, &e.freq, &e.gain, &e.q, &e.slope);
        } else if (k.size() == 5 && k.compare(0, 4, "comp") == 0 && k[4] >= '0' && k[4] < '0' + DYN_COMP_BANDS) {
            DynCompBand& c = p.comp[k[4] - '0'];
            sscanf(v.c_str(), "%d,%f,%f,%f,%f,%f,%f,%d", &c.enabled, &c.threshold, &c.ratio, &c.attack, &c.release, &c.knee, &c.makeup, &c.autoMakeup);
        }
    }
    DynSanitize(p);
    return any;
}

static bool ReadTextFile(const std::wstring& path, std::string& out) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz; GetFileSizeEx(f, &sz);
    if (sz.QuadPart > 1 << 20) { CloseHandle(f); return false; }
    out.resize((size_t)sz.QuadPart);
    DWORD r = 0; if (!out.empty()) ReadFile(f, &out[0], (DWORD)out.size(), &r, nullptr);
    CloseHandle(f);
    out.resize(r);
    return true;
}

static bool WriteFileAtomic(const std::wstring& path, const void* data, size_t size) {
    std::wstring tmp = path + L".tmp";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0; BOOL ok = WriteFile(f, data, (DWORD)size, &w, nullptr);
    CloseHandle(f);
    if (!ok || w != size) { DeleteFileW(tmp.c_str()); return false; }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}

// ---------------------------------------------------------------- presets integres
static std::vector<Preset> BuiltinPresets() {
    std::vector<Preset> v;
    auto base = [] { DynParams p; DynDefaultParams(p); return p; };

    { Preset pr; pr.name = "Neutre"; pr.builtin = true; pr.params = base(); pr.params.compEnabled = 0; v.push_back(pr); }

    { Preset pr; pr.name = "Film de nuit"; pr.builtin = true; DynParams p = base();
        p.comp[0].threshold = -32; p.comp[0].ratio = 4; p.comp[0].attack = 5; p.comp[0].release = 250; p.comp[0].knee = 10; p.comp[0].autoMakeup = 1;
        p.scHpf = 90; p.eq[7].gain = 2; p.eq[7].freq = 3000; p.eq[7].q = 0.9f;
        p.limCeiling = -1; pr.params = p; v.push_back(pr); }
    { Preset pr; pr.name = "Musique"; pr.builtin = true; DynParams p = base();
        p.comp[0].threshold = -16; p.comp[0].ratio = 2; p.comp[0].attack = 20; p.comp[0].release = 250; p.comp[0].knee = 12; p.comp[0].autoMakeup = 1;
        pr.params = p; v.push_back(pr); }
    { Preset pr; pr.name = "Jeux vidéo"; pr.builtin = true; DynParams p = base();
        p.compMultiband = 1;
        p.comp[1] = { 1, -24, 3.0f, 20, 200, 8, 0, 1 };
        p.comp[2] = { 1, -22, 2.5f, 8, 150, 8, 0, 1 };
        p.comp[3] = { 1, -24, 2.0f, 3, 80, 8, 0, 1 };
        p.xoverLow = 160; p.xoverHigh = 3500; p.limCeiling = -0.5f;
        pr.params = p; v.push_back(pr); }
    { Preset pr; pr.name = "Voix / podcast"; pr.builtin = true; DynParams p = base();
        p.eq[0] = { 1, EQ_LOWCUT, 80, 0, 0.707f, 2 };
        p.eq[7].gain = 3; p.eq[7].freq = 3000; p.eq[7].q = 1.0f;
        p.comp[0].threshold = -24; p.comp[0].ratio = 4; p.comp[0].attack = 3; p.comp[0].release = 120; p.comp[0].autoMakeup = 1;
        pr.params = p; v.push_back(pr); }
    { Preset pr; pr.name = "Volume constant"; pr.builtin = true; DynParams p = base();
        p.compMultiband = 1;
        p.comp[1] = { 1, -32, 6, 15, 300, 10, 0, 1 };
        p.comp[2] = { 1, -30, 5, 6, 200, 10, 0, 1 };
        p.comp[3] = { 1, -30, 4, 2, 100, 10, 0, 1 };
        p.limCeiling = -1; pr.params = p; v.push_back(pr); }
    return v;
}

// ---------------------------------------------------------------- dossiers
std::wstring App::DataDir() const {
    PWSTR p = nullptr; std::wstring r = L".";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &p))) { r = p; CoTaskMemFree(p); }
    r += L"\\Dynamo";
    CreateDirectoryW(r.c_str(), nullptr);
    CreateDirectoryW((r + L"\\presets").c_str(), nullptr);
    return r;
}

static std::wstring SafeFileName(const std::string& name) {
    std::wstring w = FromUtf8(name);
    for (auto& c : w) if (wcschr(L"\\/:*?\"<>|", c)) c = L'_';
    return w;
}

// ---------------------------------------------------------------- initialisation
void App::Init() {
    DynDefaultParams(params);
    fftRe_.resize(4096); fftIm_.resize(4096); win_.resize(4096);
    for (int i = 0; i < 4096; ++i) win_[i] = 0.5f - 0.5f * cosf(2 * 3.14159265f * i / 4095.f);
    specSmooth_.assign(kSpecPoints, -120.f);
    for (int i = 0; i < kSpecPoints; ++i) spectrum[i] = -120.f;
    grHistory.assign(300, 0.f);

    LoadSettings();
    RefreshPresets();

    std::string cur;
    if (ReadTextFile(DataDir() + L"\\current.ini", cur) && ParseParams(cur, params)) {
        currentPreset = -1;
        for (int i = 0; i < (int)presets.size(); ++i) if (presets[i].name == settings.lastPreset) currentPreset = i;
        modified = currentPreset < 0 || memcmp(&presets[currentPreset].params, &params, sizeof(DynParams)) != 0;
    } else {
        int idx = 0; // "Neutre" par defaut
        for (int i = 0; i < (int)presets.size(); ++i) if (presets[i].name == settings.lastPreset) idx = i;
        LoadPreset(idx);
    }
    params.bypass = 0;
    // reglage de base : les gains d'entree et de sortie repartent toujours de 0 dB
    params.inputGain = 0.f;
    params.outputGain = 0.f;
    RefreshDevices();
    // zone de parametres locale (le routage tourne dans ce processus)
    local_ = (DynShared*)VirtualAlloc(nullptr, sizeof(DynShared), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (local_) { local_->layout = DYN_LAYOUT; local_->magic = DYN_MAGIC; }
    if (settings.routingOn && local_) { sh_ = local_; PushParams(); StartRouting(); }
    else OpenLink();
    legacyPresent = sys::LegacyPresent();
    Updater::CleanupPrevious();
    if (settings.autoUpdate && !demoDevices) updater.Start(20);
    if (settings.routingOn && settings.startWithWindows) sys::SetAutostart(true);   // pointe vers cette version
    statePending_ = true; stateSaveAt_ = 0;
}

void App::Shutdown() {
    updater.Stop();
    SaveState();
    SaveSettings();
    if (settings.routingOn) {
        router.Stop();
        // on rend le son directement a la vraie sortie pour ne pas rester muet sans Dynamo
        // (sauf si Dynamo redemarre aussitot : mise a jour ou nouvelle instance)
        if (!keepRoutingOnExit) sys::SetDefaultDevice(sys::FromUtf8(settings.routeTarget));
    }
    if (sh_ == local_) sh_ = nullptr;
    if (sh_) UnmapViewOfFile(sh_);
    if (map_) CloseHandle((HANDLE)map_);
    sh_ = nullptr; map_ = nullptr;
}

// ---------------------------------------------------------------- reglages
void App::LoadSettings() {
    std::string s;
    if (!ReadTextFile(DataDir() + L"\\settings.ini", s)) { settings.startWithWindows = sys::GetAutostart(); return; }
    std::istringstream in(s); std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('='); if (eq == std::string::npos) continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "closeToTray") settings.closeToTray = v == "1";
        else if (k == "showSpectrum") settings.showSpectrum = v == "1";
        else if (k == "setAsDefault") settings.setAsDefault = v == "1";
        else if (k == "lastPreset") settings.lastPreset = v;
        else if (k == "skin") settings.skin = std::max(0, atoi(v.c_str()));
        else if (k == "autoUpdate") settings.autoUpdate = v == "1";
        else if (k == "routingOn") settings.routingOn = v == "1";
        else if (k == "routeSource") settings.routeSource = v;
        else if (k == "routeTarget") settings.routeTarget = v;
        else if (k == "selectedBand") settings.selectedBand = std::max(0, std::min(DYN_EQ_BANDS - 1, atoi(v.c_str())));
    }
    settings.startWithWindows = sys::GetAutostart();
}

void App::SaveSettings() {
    std::string s;
    s += "closeToTray=" + std::string(settings.closeToTray ? "1" : "0") + "\n";
    s += "showSpectrum=" + std::string(settings.showSpectrum ? "1" : "0") + "\n";
    s += "setAsDefault=" + std::string(settings.setAsDefault ? "1" : "0") + "\n";
    s += "lastPreset=" + settings.lastPreset + "\n";
    s += "selectedBand=" + std::to_string(settings.selectedBand) + "\n";
    s += "skin=" + std::to_string(settings.skin) + "\n";
    s += "autoUpdate=" + std::string(settings.autoUpdate ? "1" : "0") + "\n";
    s += "routingOn=" + std::string(settings.routingOn ? "1" : "0") + "\n";
    s += "routeSource=" + settings.routeSource + "\n";
    s += "routeTarget=" + settings.routeTarget + "\n";
    WriteFileAtomic(DataDir() + L"\\settings.ini", s.data(), s.size());
}

// ---------------------------------------------------------------- presets
void App::RefreshPresets() {
    std::string keep = currentPreset >= 0 && currentPreset < (int)presets.size() ? presets[currentPreset].name : settings.lastPreset;
    presets = BuiltinPresets();
    WIN32_FIND_DATAW fd;
    std::wstring dir = DataDir() + L"\\presets\\";
    HANDLE h = FindFirstFileW((dir + L"*.ini").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            std::string text;
            if (!ReadTextFile(dir + fd.cFileName, text)) continue;
            Preset pr;
            std::wstring n = fd.cFileName; n = n.substr(0, n.size() - 4);
            pr.name = ToUtf8(n);
            if (!ParseParams(text, pr.params)) continue;
            // un preset utilisateur du meme nom remplace le preset integre
            bool replaced = false;
            for (auto& b : presets) if (b.name == pr.name) { b = pr; replaced = true; }
            if (!replaced) presets.push_back(pr);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    currentPreset = -1;
    for (int i = 0; i < (int)presets.size(); ++i) if (presets[i].name == keep) currentPreset = i;
}

void App::LoadPreset(int idx) {
    if (idx < 0 || idx >= (int)presets.size()) return;
    int bypass = params.bypass;
    params = presets[idx].params;
    params.bypass = bypass;
    currentPreset = idx;
    settings.lastPreset = presets[idx].name;
    modified = false;
    PushParams();
    SaveSettings();
}

bool App::SavePresetAs(const std::string& name) {
    if (name.empty()) return false;
    DynParams p = params; p.bypass = 0;
    std::string text = SerializeParams(p);
    if (!WriteFileAtomic(DataDir() + L"\\presets\\" + SafeFileName(name) + L".ini", text.data(), text.size())) return false;
    settings.lastPreset = name;
    currentPreset = -1;
    RefreshPresets();
    for (int i = 0; i < (int)presets.size(); ++i) if (presets[i].name == name) currentPreset = i;
    modified = false;
    SaveSettings();
    return true;
}

bool App::SaveCurrentPreset() {
    if (currentPreset < 0) return false;
    return SavePresetAs(presets[currentPreset].name);
}

bool App::DeletePreset(int idx) {
    if (idx < 0 || idx >= (int)presets.size()) return false;
    std::wstring path = DataDir() + L"\\presets\\" + SafeFileName(presets[idx].name) + L".ini";
    bool ok = DeleteFileW(path.c_str()) != 0;
    RefreshPresets();
    return ok;
}

std::string App::CurrentPresetLabel() const {
    std::string n = currentPreset >= 0 && currentPreset < (int)presets.size() ? presets[currentPreset].name : "Personnalisé";
    if (modified) n += " *";
    return n;
}

// ---------------------------------------------------------------- lien avec le moteur
void App::OpenLink() {
    if (sh_) return;
    CreateDirectoryW(DYN_DATA_DIR, nullptr);
    HANDLE f = nullptr, m = nullptr;
    DynShared* s = DynOpenShared(&f, &m);
    if (!s) return;
    if (s->magic != DYN_MAGIC || s->layout != DYN_LAYOUT) {
        // premier lancement : l'interface prepare la zone, le moteur la reprendra
        s->layout = DYN_LAYOUT; s->paramSeq = 0; s->heartbeat = 0;
        MemoryBarrier();
        s->magic = DYN_MAGIC;
    }
    map_ = m; sh_ = s;
    PushParams();
}

void App::PushParams() {
    DynSanitize(params);
    if (sh_ && sh_->magic == DYN_MAGIC && sh_->layout == DYN_LAYOUT) {
        InterlockedIncrement((LONG*)&sh_->paramSeq);
        MemoryBarrier();
        memcpy((void*)&sh_->params, &params, sizeof(params));
        MemoryBarrier();
        InterlockedIncrement((LONG*)&sh_->paramSeq);
    }
    statePending_ = true;
    stateSaveAt_ = time + 0.4;
}

void App::ParamsChanged() {
    modified = true;
    PushParams();
}

void App::SaveState() {
    statePending_ = false;
    DynStateFile sf = {};
    sf.magic = DYN_MAGIC; sf.layout = DYN_LAYOUT; sf.size = sizeof(DynParams); sf.seq = 0;
    sf.params = params;
    CreateDirectoryW(DYN_DATA_DIR, nullptr);
    WriteFileAtomic(DYN_STATE_FILE, &sf, sizeof(sf));   // lu par le moteur au demarrage de Windows
    std::string text = SerializeParams(params);
    WriteFileAtomic(DataDir() + L"\\current.ini", text.data(), text.size());
}

EngineStatus App::Status() const {
    if (settings.routingOn) return (sh_ && time - lastBeatTime_ < 1.0) ? ENGINE_ACTIVE : ENGINE_WAITING;
    if (!anyInstalled()) return ENGINE_NOT_INSTALLED;
    if (sh_ && time - lastBeatTime_ < 1.0) return ENGINE_ACTIVE;
    return ENGINE_WAITING;
}
uint32_t App::sampleRate() const { return sh_ ? sh_->sampleRate : 0; }
uint32_t App::channels() const { return sh_ ? sh_->channels : 0; }
bool App::anyInstalled() const { for (auto& d : devices) if (d.installed) return true; return false; }

// ---------------------------------------------------------------- peripheriques
void App::RefreshDevices() {
    std::wstring keep = selectedDevice >= 0 && selectedDevice < (int)devices.size() ? devices[selectedDevice].id : L"";
    devices = sys::ListRenderDevices();
    if (demoDevices && devices.empty()) {
        sys::Device a; a.id = L"{0.0.0.00000000}.{11111111-2222-3333-4444-555555555555}"; a.guid = L"{11111111-2222-3333-4444-555555555555}";
        a.name = L"Haut-parleurs (RME Fireface UC)"; a.isDefault = true; a.installed = true; devices.push_back(a);
        sys::Device b; b.id = L"{0.0.0.00000000}.{66666666-2222-3333-4444-555555555555}"; b.guid = L"{66666666-2222-3333-4444-555555555555}";
        b.name = L"Haut-parleurs (Realtek(R) Audio)"; devices.push_back(b);
    }
    selectedDevice = 0;
    bool found = false;
    for (int i = 0; i < (int)devices.size(); ++i) if (devices[i].id == keep) { selectedDevice = i; found = true; }
    if (!found) {
        // par defaut : le peripherique ou le moteur est installe, sinon la sortie Windows par defaut
        for (int i = 0; i < (int)devices.size(); ++i) if (devices[i].installed) { selectedDevice = i; found = true; break; }
        if (!found) for (int i = 0; i < (int)devices.size(); ++i) if (devices[i].isDefault) { selectedDevice = i; break; }
    }
}

bool App::InstallOnSelected(int mode) {
    if (selectedDevice < 0 || selectedDevice >= (int)devices.size()) return false;
    SaveState();
    const sys::Device& d = devices[selectedDevice];
    lastError.clear();
    std::wstring err;
    if (!sys::RequestElevated(L"install " + d.guid + L" " + std::to_wstring(mode), err)) { lastError = err; return false; }
    std::wstring id = d.id, name = d.name;
    if (settings.setAsDefault) sys::SetDefaultDevice(id);
    RefreshDevices();
    lastInfo = "Moteur installé sur « " + ToUtf8(name) + " ». Lancez un son pour vérifier.";
    infoTime = time;
    if (settings.startWithWindows) sys::SetAutostart(true);
    return true;
}

bool App::UninstallFromSelected() {
    if (selectedDevice < 0 || selectedDevice >= (int)devices.size()) return false;
    std::wstring err;
    if (!sys::RequestElevated(L"uninstall " + devices[selectedDevice].guid, err)) { lastError = err; return false; }
    RefreshDevices();
    lastInfo = "Moteur retiré de ce périphérique.";
    infoTime = time;
    return true;
}

bool App::UninstallEverything() {
    std::wstring err;
    if (!sys::RequestElevated(L"uninstall-all", err)) { lastError = err; return false; }
    sys::SetAutostart(false);
    settings.startWithWindows = false;
    RefreshDevices();
    lastInfo = "KDYN-10 a été entièrement désinstallé du système.";
    infoTime = time;
    return true;
}

bool App::RestartAudio() {
    std::wstring err;
    if (!sys::RequestElevated(L"restart-audio", err)) { lastError = err; return false; }
    lastInfo = "Service audio de Windows redémarré.";
    infoTime = time;
    return true;
}

bool App::MakeSelectedDefault() {
    if (selectedDevice < 0 || selectedDevice >= (int)devices.size()) return false;
    bool ok = sys::SetDefaultDevice(devices[selectedDevice].id);
    RefreshDevices();
    return ok;
}

// ---------------------------------------------------------------- boucle
static void FFT(float* re, float* im, int n) {
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (int len = 2; len <= n; len <<= 1) {
        double ang = -2 * 3.14159265358979 / len;
        float wr = (float)cos(ang), wi = (float)sin(ang);
        for (int i = 0; i < n; i += len) {
            float cr = 1, ci = 0;
            for (int k = 0; k < len / 2; ++k) {
                float ur = re[i + k], ui = im[i + k];
                float vr = re[i + k + len / 2] * cr - im[i + k + len / 2] * ci;
                float vi = re[i + k + len / 2] * ci + im[i + k + len / 2] * cr;
                re[i + k] = ur + vr; im[i + k] = ui + vi;
                re[i + k + len / 2] = ur - vr; im[i + k + len / 2] = ui - vi;
                float nr = cr * wr - ci * wi; ci = cr * wi + ci * wr; cr = nr;
            }
        }
    }
}

void App::ComputeSpectrum() {
    const int N = 4096;
    const uint32_t pos = sh_->specWrite;
    for (int i = 0; i < N; ++i) {
        fftRe_[i] = sh_->spec[(pos - N + i) & (DYN_SPEC_SIZE - 1)] * win_[i];
        fftIm_[i] = 0;
    }
    FFT(fftRe_.data(), fftIm_.data(), N);
    const float fs = sh_->sampleRate ? (float)sh_->sampleRate : 48000.f;
    const float norm = 2.0f / (N * 0.5f);
    for (int p = 0; p < kSpecPoints; ++p) {
        float f0 = 20.f * powf(1000.f, (float)p / kSpecPoints);
        float f1 = 20.f * powf(1000.f, (float)(p + 1) / kSpecPoints);
        int b0 = (int)(f0 * N / fs), b1 = (int)(f1 * N / fs);
        if (b0 < 1) b0 = 1;
        if (b1 <= b0) b1 = b0 + 1;
        if (b1 > N / 2) b1 = N / 2;
        float mx = 0;
        for (int b = b0; b < b1; ++b) { float m = fftRe_[b] * fftRe_[b] + fftIm_[b] * fftIm_[b]; if (m > mx) mx = m; }
        float db = 10.f * log10f(mx * norm * norm + 1e-14f);
        float fc = sqrtf(f0 * f1);
        db += 3.0f * log2f(fc / 1000.f);   // inclinaison 3 dB/oct (lecture "rose")
        float& s = specSmooth_[p];
        s += (db - s) * (db > s ? 0.5f : 0.12f);
        spectrum[p] = s;
    }
    spectrumValid = true;
}

void App::Tick(double dt) {
    time += dt;
    if (!sh_ && !settings.routingOn && time >= nextLinkTry_) { OpenLink(); nextLinkTry_ = time + 1.0; }
    if (time >= nextDeviceCheck_) { RefreshDevices(); nextDeviceCheck_ = time + 4.0; }
    if (statePending_ && time >= stateSaveAt_) SaveState();
    if (diagRecheckAt > 0 && time >= diagRecheckAt) { diagRecheckAt = -1; RunDiagnostic(); }

    float inPk[2] = {0, 0}, outPk[2] = {0, 0}, grNow[DYN_COMP_BANDS] = {0}, limNow = 0;
    bool fresh = false;
    if (sh_ && sh_->magic == DYN_MAGIC) {
        if (sh_->layout != DYN_LAYOUT) { /* moteur d'une autre version : on n'ecrit rien */ }
        int32_t beat = sh_->heartbeat;
        if (beat != lastBeat_) { lastBeat_ = beat; lastBeatTime_ = time; fresh = true; }
        DynMeters& M = sh_->meters;
        for (int i = 0; i < 2; ++i) { inPk[i] = M.inPeak[i]; M.inPeak[i] = 0; outPk[i] = M.outPeak[i]; M.outPeak[i] = 0; }
        for (int b = 0; b < DYN_COMP_BANDS; ++b) { grNow[b] = M.grComp[b]; M.grComp[b] = 0; }
        limNow = M.grLimiter; M.grLimiter = 0;
        compLevel = fresh ? M.compInLevel : -120.f;
    }
    const float fall = (float)(dt * 24.0);   // retombee des vumetres : 24 dB/s
    for (int i = 0; i < 2; ++i) {
        float a = 20.f * log10f(inPk[i] + 1e-9f), b = 20.f * log10f(outPk[i] + 1e-9f);
        inLevel[i] = std::max(a, inLevel[i] - fall);
        outLevel[i] = std::max(b, outLevel[i] - fall);
        if (inLevel[i] < -90) inLevel[i] = -90;
        if (outLevel[i] < -90) outLevel[i] = -90;
        inHold[i] = std::max(inLevel[i], inHold[i] - (float)(dt * 8.0));
        outHold[i] = std::max(outLevel[i], outHold[i] - (float)(dt * 8.0));
    }
    const float grFall = (float)exp(-dt / 0.12);
    for (int b = 0; b < DYN_COMP_BANDS; ++b) gr[b] = std::max(grNow[b], gr[b] * grFall);
    grLim = std::max(limNow, grLim * grFall);

    // historique de reduction de gain (30 points par seconde)
    float grMax = params.compMultiband ? std::max(gr[1], std::max(gr[2], gr[3])) : gr[0];
    grPeakSinceSample_ = std::max(grPeakSinceSample_, grMax);
    grAccum_ += dt;
    while (grAccum_ >= 1.0 / 30.0) {
        grAccum_ -= 1.0 / 30.0;
        grHistory[grHistoryPos] = grPeakSinceSample_;
        grHistoryPos = (grHistoryPos + 1) % (int)grHistory.size();
        grPeakSinceSample_ = 0;
    }

    if (settings.showSpectrum && sh_ && Status() == ENGINE_ACTIVE) ComputeSpectrum();
    else if (spectrumValid) {
        for (int p = 0; p < kSpecPoints; ++p) { specSmooth_[p] -= (float)(dt * 60.0); spectrum[p] = specSmooth_[p]; }
        if (specSmooth_[kSpecPoints / 2] < -130) spectrumValid = false;
    }
}

// ---------------------------------------------------------------- diagnostic
void App::RunDiagnostic() {
    RefreshDevices();
    diag.clear();
    auto add = [&](const std::string& l, int st, const std::string& d) { diag.push_back({ l, st, d }); };
    const sys::Device* sel = devices.empty() ? nullptr : &devices[selectedDevice];
    const sys::Device* def = nullptr;
    for (auto& d : devices) if (d.isDefault) def = &d;

    if (!sel) { add("Sortie audio", 2, "Aucune sortie audio active trouvée."); }
    else add("Sortie traitée", 0, ToUtf8(sel->name));

    bool fx = sys::ProcessRunning(L"fxsound.exe") || sys::ProcessRunning(L"FxSound.exe");
    add("FxSound fermé", fx ? 2 : 0, fx ? "FxSound tourne encore : il détourne le son vers sa sortie virtuelle." : "OK");

    if (sel) {
        bool same = def && def->id == sel->id;
        add("Sortie Windows par défaut", same ? 0 : 2,
            same ? "OK" : "La sortie par défaut est « " + (def ? ToUtf8(def->name) : std::string("?")) + " » au lieu de la sortie traitée.");
        add("Moteur inscrit sur la sortie", sel->installed ? 0 : 2, sel->installed ? std::string("OK (mode ") + sys::ModeName(sel->mode) + ")" : "Pas encore installé sur cette sortie.");
        add("Améliorations audio Windows", sel->enhancementsOff ? 2 : 0, sel->enhancementsOff ? "Désactivées pour cette sortie : Windows bloque les effets." : "OK");
    }
    if (sel) {
        float pk = sys::EndpointPeak(sel->id);
        if (pk >= 0) add("Du son arrive sur la sortie", pk > 0.0005f ? 0 : 1, pk > 0.0005f ? "OK" : "Aucun son en ce moment sur cette sortie (lance une lecture).");
    }
    bool files = sys::EngineFilesInstalled(), com = sys::ComRegistered();
    add("Fichiers du moteur", files && com ? 0 : 2, files && com ? "OK" : (files ? "DLL présente mais non enregistrée." : "DLL absente de Program Files."));

    std::wstring log = sys::ReadFileTail(DYN_LOG_FILE, 4000);
    std::string logU = ToUtf8(log);
    bool loaded = logU.find("LockForProcess :") != std::string::npos;
    bool fileMode = logU.find("mode fichier") != std::string::npos;
    std::string lastLine;
    { size_t e = logU.find_last_not_of("\r\n"); if (e != std::string::npos) { size_t b = logU.rfind('\n', e); lastLine = logU.substr(b == std::string::npos ? 0 : b + 1, e - (b == std::string::npos ? 0 : b + 1) + 1); } }
    if (log.empty()) add("Moteur chargé par Windows", 2, "Jamais chargé (pas de journal moteur.log).");
    else add("Moteur chargé par Windows", loaded ? 0 : 1, loaded ? "OK — " + lastLine : "Journal présent mais pas de flux audio : " + lastLine);
    add("Liaison interface / moteur", sh_ ? 0 : (fileMode ? 1 : 2), sh_ ? "OK" : (fileMode ? "Le moteur fonctionne en mode fichier (vumètres indisponibles)." : "Mémoire partagée introuvable."));
    bool active = Status() == ENGINE_ACTIVE;
    add("Son traité en ce moment", active ? 0 : 1, active ? "OK" : "Aucun son reçu (lance une lecture pour vérifier).");

    // rapport texte a copier-coller
    diagReport = std::string("=== Diagnostic Kinetik KDYN-10 ") + DYN_VERSION_STR + " ===\n";
    for (auto& d : diag) diagReport += std::string(d.state == 0 ? "[OK] " : d.state == 1 ? "[??] " : "[!!] ") + d.label + " : " + d.detail + "\n";
    diagReport += "\n--- moteur.log ---\n" + ToUtf8(sys::ReadFileTail(DYN_LOG_FILE, 1500));
    diagReport += "\n--- install.log ---\n" + ToUtf8(sys::ReadInstallLogTail(1500));
    diagReport += "\n--- sorties ---\n";
    for (auto& d : devices) diagReport += ToUtf8(d.name) + " " + ToUtf8(d.guid) + (d.isDefault ? " [defaut]" : "") + (d.installed ? " [dynamo]" : "") + (d.enhancementsOff ? " [ameliorations off]" : "") + "\n";
}

void App::RepairAll() {
    lastError.clear();
    if (devices.empty()) return;
    std::wstring selId = devices[selectedDevice].id, guid = devices[selectedDevice].guid, name = devices[selectedDevice].name;
    if (sys::ProcessRunning(L"fxsound.exe")) { sys::KillProcess(L"fxsound.exe"); Sleep(800); }
    sys::SetDefaultDevice(selId);
    SaveState();
    std::wstring err; unsigned long code = 0;
    lastInfo = "Réparation en cours : KDYN-10 teste les modes de branchement (jusqu'à 40 s)…";
    infoTime = time;
    bool ok = sys::RequestElevated(L"autofix " + guid, err, &code);
    sys::SetDefaultDevice(selId);
    RefreshDevices();
    for (int i = 0; i < (int)devices.size(); ++i) if (devices[i].id == selId) selectedDevice = i;
    if (!ok) lastError = err;
    else if (code >= 100) { lastInfo = std::string("Réparé : le moteur tourne sur « ") + ToUtf8(name) + " » (mode " + sys::ModeName((int)code - 100) + ")."; infoTime = time; }
    else if (code == 40) lastError = L"Windows refuse de charger le moteur sur cette sortie, quel que soit le mode. Clique sur « Copier le rapport » et envoie-le.";
    if (settings.startWithWindows) sys::SetAutostart(true);
    diagRecheckAt = time + 2.0;
    RunDiagnostic();
}

// ---------------------------------------------------------------- routage interne
int App::FindDevice(const std::string& id) const {
    for (int i = 0; i < (int)devices.size(); ++i) if (ToUtf8(devices[i].id) == id) return i;
    return -1;
}
std::string App::DeviceName(const std::string& id) const {
    int i = FindDevice(id);
    return i >= 0 ? ToUtf8(devices[i].name) : std::string("(introuvable)");
}

static std::wstring CardName(const std::wstring& n) {   // "Haut-parleurs (RME Fireface UC)" -> "RME Fireface UC"
    size_t a = n.rfind(L'('), b = n.rfind(L')');
    return (a != std::wstring::npos && b != std::wstring::npos && b > a) ? n.substr(a + 1, b - a - 1) : L"";
}

bool App::ConfigureRouting(const std::wstring& targetIn, const std::wstring& sourceIn) {
    // copies : les references peuvent pointer dans 'devices', rafraichi juste apres
    const std::wstring target = targetIn;
    const std::wstring source = sourceIn;
    lastError.clear();
    RefreshDevices();
    int ti = -1;
    for (int i = 0; i < (int)devices.size(); ++i) if (devices[i].id == target) ti = i;
    if (ti < 0) { lastError = L"Sortie de destination introuvable."; return false; }
    std::wstring src = source;
    if (src.empty() || src == target) {
        // choix automatique : une autre sortie de la meme carte, "(3+4)" en priorite ; FxSound en dernier recours
        std::wstring card = CardName(devices[ti].name);
        int best = -1, score = -1;
        for (int i = 0; i < (int)devices.size(); ++i) {
            if (i == ti) continue;
            const std::wstring& n = devices[i].name;
            int s = 1;
            if (n.find(L"FxSound") != std::wstring::npos) s = 0;
            if (!card.empty() && CardName(n) == card) s = 3;
            if (s == 3 && n.find(L"3+4") != std::wstring::npos) s = 4;
            if (s > score) { score = s; best = i; }
        }
        if (best < 0) { lastError = L"Il faut une deuxième sortie audio (ex. « Analog (3+4) ») pour le routage."; return false; }
        src = devices[best].id;
    }
    // le moteur APO inscrit sur la destination n'est plus utile : on le retire (sinon double traitement)
    if (devices[ti].installed) {
        std::wstring err;
        if (!sys::RequestElevated(L"uninstall " + devices[ti].guid, err)) { /* non bloquant */ }
    }
    router.Stop();
    settings.routingOn = true;
    settings.routeSource = ToUtf8(src);
    settings.routeTarget = ToUtf8(target);
    SaveSettings();
    if (sh_ && sh_ != local_) { UnmapViewOfFile(sh_); if (map_) CloseHandle((HANDLE)map_); map_ = nullptr; }
    sh_ = local_;
    PushParams();
    StartRouting();
    sys::SetAutostart(true); settings.startWithWindows = true;
    RefreshDevices();
    lastInfo = "Routage activé : Windows joue sur « " + DeviceName(settings.routeSource) + " », KDYN-10 traite et envoie sur « " + DeviceName(settings.routeTarget) + " ».";
    infoTime = time;
    return true;
}

void App::StartRouting() {
    if (!local_) return;
    std::wstring src = sys::FromUtf8(settings.routeSource), dst = sys::FromUtf8(settings.routeTarget);
    if (src.empty() || dst.empty()) return;
    sys::SetDefaultDevice(src);   // tout le son de Windows part vers la sortie capturee
    router.Start(src, dst, local_);
}

void App::DisableRouting() {
    router.Stop();
    sys::SetDefaultDevice(sys::FromUtf8(settings.routeTarget));
    settings.routingOn = false;
    SaveSettings();
    sh_ = nullptr;
    OpenLink();
    RefreshDevices();
    lastInfo = "Routage désactivé : Windows joue directement sur la sortie.";
    infoTime = time;
}

bool App::CleanupLegacy() {
    lastError.clear();
    std::wstring err;
    bool ok = true;
    if (sys::LegacyPresent()) ok = sys::RequestElevated(L"cleanup-legacy", err);
    if (!ok) { lastError = err; return false; }
    int n = sys::RecycleOldExecutables();
    legacyPresent = sys::LegacyPresent();
    Updater::CleanupPrevious();
    if (settings.autoUpdate && !demoDevices) updater.Start(20);
    if (settings.startWithWindows) sys::SetAutostart(true);
    RefreshDevices();
    lastInfo = "Anciennes versions supprimées" + std::string(n ? " (" + std::to_string(n) + " ancien(s) fichier(s) mis à la corbeille)" : "") + ".";
    infoTime = time;
    return true;
}
