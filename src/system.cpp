// Dynamo - fonctions systeme : peripheriques, installation du moteur (APO), routage.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <mmdeviceapi.h>
#include <propvarutil.h>
#include <shlobj.h>
#include <shellapi.h>
#include <sddl.h>
#include <aclapi.h>
#include <tlhelp32.h>
#include <audioclient.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <vector>
#include <algorithm>

#include "system.h"
#include "shared.h"
#include "apo_iface.h"

namespace sys {

static const PROPERTYKEY kFriendlyName = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };

static const wchar_t* kMMDevRender = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Render\\";
static const wchar_t* kBackupRoot  = L"SOFTWARE\\Dynamo\\Endpoints\\";
static const wchar_t* kRunKey      = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

// Noms des proprietes FX gerees (sauvegardees puis restaurees a la desinstallation)
static const wchar_t* V_LFX    = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},1";
static const wchar_t* V_GFX    = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},2";
static const wchar_t* V_SFX    = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},5";
static const wchar_t* V_MFX    = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},6";
static const wchar_t* V_EFX    = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},7";
static const wchar_t* V_CSFX   = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},13";
static const wchar_t* V_CMFX   = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},14";
static const wchar_t* V_CEFX   = L"{d04e05a6-594b-4fb6-a80d-01af5eed7d1d},15";
static const wchar_t* V_PM_SFX = L"{d3993a3f-99c2-4402-b5ec-a92a0367664b},5";
static const wchar_t* V_PM_MFX = L"{d3993a3f-99c2-4402-b5ec-a92a0367664b},6";
static const wchar_t* V_PM_EFX = L"{d3993a3f-99c2-4402-b5ec-a92a0367664b},7";
static const wchar_t* V_SYSFX  = L"{1da5d803-d492-4edd-8c23-e0c0ffee7f0e},5";
static const wchar_t* V_TITLE  = L"{b725f130-47ef-101a-a5f1-02608c9eebac},10";
static const wchar_t* kManaged[] = { V_LFX, V_GFX, V_SFX, V_MFX, V_EFX, V_CSFX, V_CMFX, V_CEFX, V_PM_SFX, V_PM_MFX, V_PM_EFX, V_SYSFX, V_TITLE };
static const wchar_t* kDefaultMode = L"{C18E2F7E-933D-4965-B7D1-1EEF228D2AF3}";

// ---------------------------------------------------------------- utilitaires
std::string ToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}
std::wstring FromUtf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

static bool IEquals(const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) == 0; }

static void ILog(const wchar_t* fmt, ...) {
    wchar_t msg[1024];
    va_list ap; va_start(ap, fmt); _vsnwprintf(msg, 1023, fmt, ap); va_end(ap); msg[1023] = 0;
    std::wstring path = std::wstring(DYN_DATA_DIR) + L"\\install.log";
    CreateDirectoryW(DYN_DATA_DIR, nullptr);
    HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME st; GetLocalTime(&st);
    char head[64];
    int n = snprintf(head, sizeof(head), "%04d-%02d-%02d %02d:%02d:%02d ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    std::string line = std::string(head, n) + ToUtf8(msg) + "\r\n";
    DWORD w; WriteFile(f, line.data(), (DWORD)line.size(), &w, nullptr);
    CloseHandle(f);
}

std::wstring ReadInstallLogTail(size_t maxChars) { return ReadFileTail(std::wstring(DYN_DATA_DIR) + L"\\install.log", maxChars); }

std::wstring ReadFileTail(const std::wstring& path, size_t maxChars) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return L"";
    LARGE_INTEGER sz; GetFileSizeEx(f, &sz);
    LONGLONG start = sz.QuadPart > (LONGLONG)maxChars ? sz.QuadPart - maxChars : 0;
    LARGE_INTEGER pos; pos.QuadPart = start; SetFilePointerEx(f, pos, nullptr, FILE_BEGIN);
    std::string buf((size_t)(sz.QuadPart - start), '\0');
    DWORD r = 0; if (!buf.empty()) ReadFile(f, &buf[0], (DWORD)buf.size(), &r, nullptr);
    CloseHandle(f);
    buf.resize(r);
    return FromUtf8(buf);
}

static std::wstring ClsidString() { return DYN_CLSID_STR; }

static std::wstring SelfPath() {
    wchar_t p[MAX_PATH * 2]; GetModuleFileNameW(nullptr, p, MAX_PATH * 2); return p;
}

std::wstring InstallDir() {
    PWSTR pf = nullptr; std::wstring r = L"C:\\Program Files";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, nullptr, &pf))) { r = pf; CoTaskMemFree(pf); }
    return r + L"\\Dynamo";
}
std::wstring InstalledExePath() { return InstallDir() + L"\\Dynamo.exe"; }
static std::wstring InstalledDllPath() { return InstallDir() + L"\\DynamoAPO.dll"; }

bool IsRunningFromInstallDir() { return IEquals(SelfPath(), InstalledExePath()); }
bool EngineFilesInstalled() { return GetFileAttributesW(InstalledDllPath().c_str()) != INVALID_FILE_ATTRIBUTES; }

// ---------------------------------------------------------------- registre
struct RegValue { DWORD type = 0; std::vector<BYTE> data; };

static bool ReadValue(HKEY k, const wchar_t* name, RegValue& out) {
    DWORD type = 0, size = 0;
    if (RegQueryValueExW(k, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS) return false;
    out.type = type; out.data.resize(size);
    if (size && RegQueryValueExW(k, name, nullptr, &type, out.data.data(), &size) != ERROR_SUCCESS) return false;
    out.data.resize(size);
    return true;
}
static std::wstring ValueAsString(const RegValue& v) {
    if (v.data.size() < 2) return L"";
    return std::wstring((const wchar_t*)v.data.data(), wcsnlen((const wchar_t*)v.data.data(), v.data.size() / 2));
}
static std::vector<std::wstring> ValueAsMulti(const RegValue& v) {
    std::vector<std::wstring> r;
    const wchar_t* p = (const wchar_t*)v.data.data();
    size_t n = v.data.size() / 2, i = 0;
    while (i < n && p[i]) { std::wstring s(p + i); i += s.size() + 1; r.push_back(s); }
    return r;
}
static LONG WriteString(HKEY k, const wchar_t* name, const std::wstring& s) {
    return RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)s.c_str(), (DWORD)((s.size() + 1) * 2));
}
static LONG WriteMulti(HKEY k, const wchar_t* name, const std::vector<std::wstring>& list) {
    std::wstring buf;
    for (auto& s : list) { buf += s; buf.push_back(0); }
    buf.push_back(0);
    return RegSetValueExW(k, name, 0, REG_MULTI_SZ, (const BYTE*)buf.data(), (DWORD)(buf.size() * 2));
}
static LONG WriteDword(HKEY k, const wchar_t* name, DWORD v) {
    return RegSetValueExW(k, name, 0, REG_DWORD, (const BYTE*)&v, sizeof(v));
}

static void EnablePrivilege(const wchar_t* name) {
    HANDLE tok;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok)) return;
    TOKEN_PRIVILEGES tp = {}; tp.PrivilegeCount = 1; tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (LookupPrivilegeValueW(nullptr, name, &tp.Privileges[0].Luid)) AdjustTokenPrivileges(tok, FALSE, &tp, 0, nullptr, nullptr);
    CloseHandle(tok);
}

// Ouvre/cree une cle HKLM en ecriture, en contournant les ACL si besoin (privileges sauvegarde/restauration)
static HKEY OpenHklmWrite(const std::wstring& path, bool* created = nullptr) {
    HKEY h = nullptr; DWORD disp = 0;
    LONG r = RegCreateKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                             KEY_ALL_ACCESS | KEY_WOW64_64KEY, nullptr, &h, &disp);
    if (r != ERROR_SUCCESS) {
        r = RegCreateKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, nullptr, REG_OPTION_BACKUP_RESTORE,
                            KEY_ALL_ACCESS | KEY_WOW64_64KEY, nullptr, &h, &disp);
    }
    if (r != ERROR_SUCCESS) { ILog(L"Impossible d'ouvrir HKLM\\%s en ecriture (erreur %ld)", path.c_str(), r); return nullptr; }
    if (created) *created = (disp == REG_CREATED_NEW_KEY);
    return h;
}

static HKEY OpenHklmRead(const std::wstring& path) {
    HKEY h = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_READ | KEY_WOW64_64KEY, &h) != ERROR_SUCCESS) return nullptr;
    return h;
}

static bool KeyExists(const std::wstring& path) { HKEY h = OpenHklmRead(path); if (h) RegCloseKey(h); return h != nullptr; }

// ---------------------------------------------------------------- etat d'un peripherique
static void ReadInstallState(Device& d) {
    HKEY k = OpenHklmRead(std::wstring(kMMDevRender) + d.guid + L"\\FxProperties");
    if (!k) return;
    RegValue v;
    const std::wstring me = ClsidString();
    if (ReadValue(k, V_EFX, v) && IEquals(ValueAsString(v), me)) { d.installed = true; d.mode = MODE_EFX; }
    if (ReadValue(k, V_MFX, v) && IEquals(ValueAsString(v), me)) { d.installed = true; d.mode = MODE_MFX; }
    if (ReadValue(k, V_GFX, v) && IEquals(ValueAsString(v), me)) { d.installed = true; d.mode = MODE_GFX; }
    if (ReadValue(k, V_SFX, v) && IEquals(ValueAsString(v), me)) { d.installed = true; d.mode = MODE_SFX; }
    if (ReadValue(k, V_CSFX, v)) for (auto& s : ValueAsMulti(v)) if (IEquals(s, me)) { d.installed = true; d.mode = MODE_SFX; }
    if (ReadValue(k, V_CEFX, v)) for (auto& s : ValueAsMulti(v)) if (IEquals(s, me)) { d.installed = true; d.mode = MODE_EFX; }
    if (ReadValue(k, V_CMFX, v)) for (auto& s : ValueAsMulti(v)) if (IEquals(s, me)) { d.installed = true; d.mode = MODE_MFX; }
    if (ReadValue(k, V_SYSFX, v) && v.type == REG_DWORD && v.data.size() >= 4 && *(DWORD*)v.data.data() != 0) d.enhancementsOff = true;
    RegCloseKey(k);
}

std::vector<Device> ListRenderDevices() {
    std::vector<Device> out;
    IMMDeviceEnumerator* en = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en))) return out;
    std::wstring defId;
    IMMDevice* def = nullptr;
    if (SUCCEEDED(en->GetDefaultAudioEndpoint(eRender, eConsole, &def))) {
        LPWSTR id = nullptr; if (SUCCEEDED(def->GetId(&id))) { defId = id; CoTaskMemFree(id); }
        def->Release();
    }
    IMMDeviceCollection* col = nullptr;
    if (SUCCEEDED(en->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &col))) {
        UINT n = 0; col->GetCount(&n);
        for (UINT i = 0; i < n; ++i) {
            IMMDevice* dev = nullptr;
            if (FAILED(col->Item(i, &dev))) continue;
            Device d;
            LPWSTR id = nullptr;
            if (SUCCEEDED(dev->GetId(&id))) { d.id = id; CoTaskMemFree(id); }
            size_t dot = d.id.rfind(L'.');
            d.guid = dot != std::wstring::npos ? d.id.substr(dot + 1) : d.id;
            IPropertyStore* ps = nullptr;
            if (SUCCEEDED(dev->OpenPropertyStore(STGM_READ, &ps))) {
                PROPVARIANT pv; PropVariantInit(&pv);
                if (SUCCEEDED(ps->GetValue(kFriendlyName, &pv)) && pv.vt == VT_LPWSTR) d.name = pv.pwszVal;
                PropVariantClear(&pv);
                ps->Release();
            }
            if (d.name.empty()) d.name = d.guid;
            d.isDefault = (d.id == defId);
            ReadInstallState(d);
            out.push_back(d);
            dev->Release();
        }
        col->Release();
    }
    en->Release();
    std::stable_sort(out.begin(), out.end(), [](const Device& a, const Device& b) { return a.isDefault > b.isDefault; });
    return out;
}

// ---------------------------------------------------------------- sortie par defaut (IPolicyConfig, interface non documentee de Windows)
struct IPolicyConfig : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(PCWSTR, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR, INT, WAVEFORMATEX**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR, WAVEFORMATEX*, WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR, INT, PINT64, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR, PINT64) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR, const PROPERTYKEY&, PROPVARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR, ERole) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR, INT) = 0;
};
static const CLSID CLSID_PolicyConfigClient = { 0x870af99c, 0x171d, 0x4f9e, { 0xaf, 0x0d, 0xe6, 0x3d, 0xf4, 0x0c, 0x2b, 0xc9 } };
static const IID   IID_IPolicyConfig        = { 0xf8679f50, 0x850a, 0x41cf, { 0x9c, 0x72, 0x43, 0x0f, 0x29, 0x02, 0x90, 0xc8 } };

bool SetDefaultDevice(const std::wstring& id) {
    IPolicyConfig* pc = nullptr;
    if (FAILED(CoCreateInstance(CLSID_PolicyConfigClient, nullptr, CLSCTX_ALL, IID_IPolicyConfig, (void**)&pc))) return false;
    bool ok = SUCCEEDED(pc->SetDefaultEndpoint(id.c_str(), eConsole));
    pc->SetDefaultEndpoint(id.c_str(), eMultimedia);
    pc->SetDefaultEndpoint(id.c_str(), eCommunications);
    pc->Release();
    return ok;
}

// ---------------------------------------------------------------- demarrage automatique
bool GetAutostart() {
    HKEY k; if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_READ, &k) != ERROR_SUCCESS) return false;
    bool on = RegQueryValueExW(k, L"Dynamo", nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
    RegCloseKey(k);
    return on;
}
void SetAutostart(bool on) {
    HKEY k; if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    if (on) {
        std::wstring exe = SelfPath();
        WriteString(k, L"Dynamo", L"\"" + exe + L"\" --tray");
    } else RegDeleteValueW(k, L"Dynamo");
    RegCloseKey(k);
}

// ---------------------------------------------------------------- elevation
bool RequestElevated(const std::wstring& args, std::wstring& error, unsigned long* codeOut) {
    std::wstring exe = SelfPath();
    std::wstring params = L"--elevated " + args;
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.lpVerb = L"runas";
    sei.lpFile = exe.c_str();
    sei.lpParameters = params.c_str();
    sei.nShow = SW_HIDE;
    if (!ShellExecuteExW(&sei)) {
        DWORD e = GetLastError();
        error = (e == ERROR_CANCELLED) ? L"Autorisation administrateur refusée." : L"Impossible de lancer l'opération administrateur (erreur " + std::to_wstring(e) + L").";
        return false;
    }
    // Attente en pompant les messages pour ne pas figer la fenetre
    for (;;) {
        DWORD r = MsgWaitForMultipleObjects(1, &sei.hProcess, FALSE, 60000, QS_ALLINPUT);
        if (r == WAIT_OBJECT_0) break;
        if (r == WAIT_OBJECT_0 + 1) { MSG m; while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); } continue; }
        break;
    }
    DWORD code = 1; GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    if (codeOut) *codeOut = code;
    if (code != 0 && !(codeOut && code >= 100 && code < 110)) {
        error = L"L'opération a échoué (code " + std::to_wstring(code) + L"). Détails : C:\\ProgramData\\Dynamo\\install.log";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- operations administrateur
static bool SetDirEveryoneFull(const std::wstring& dir) {
    CreateDirectoryW(dir.c_str(), nullptr);
    PSECURITY_DESCRIPTOR sd = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:PAI(A;OICI;FA;;;WD)(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;FA;;;LS)(A;OICI;FA;;;WR)(A;OICI;FA;;;S-1-5-6)",
                                                              SDDL_REVISION_1, &sd, nullptr)) return false;
    BOOL present = FALSE, def = FALSE; PACL dacl = nullptr;
    GetSecurityDescriptorDacl(sd, &present, &dacl, &def);
    DWORD r = SetNamedSecurityInfoW((LPWSTR)dir.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                    nullptr, nullptr, dacl, nullptr);
    LocalFree(sd);
    return r == ERROR_SUCCESS;
}

static bool FilesEqual(const std::wstring& path, const void* data, DWORD size) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz; GetFileSizeEx(f, &sz);
    bool eq = false;
    if (sz.QuadPart == size) {
        std::vector<BYTE> buf(size); DWORD r = 0;
        if (ReadFile(f, buf.data(), size, &r, nullptr) && r == size) eq = memcmp(buf.data(), data, size) == 0;
    }
    CloseHandle(f);
    return eq;
}

// Remplace un fichier eventuellement verrouille (DLL chargee par audiodg.exe)
static bool ReplaceFileSafe(const std::wstring& dst, const void* data, DWORD size) {
    if (FilesEqual(dst, data, size)) return true;
    std::wstring tmp = dst + L".new";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) { ILog(L"Ecriture de %s impossible (%lu)", tmp.c_str(), GetLastError()); return false; }
    DWORD w = 0; BOOL ok = WriteFile(f, data, size, &w, nullptr); CloseHandle(f);
    if (!ok || w != size) return false;
    if (MoveFileExW(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING)) return true;
    // fichier en cours d'utilisation : on le renomme (autorise pour une DLL chargee) puis on le supprime au redemarrage
    std::wstring old = dst + L"." + std::to_wstring(GetTickCount64()) + L".old";
    if (MoveFileExW(dst.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        MoveFileExW(old.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        if (MoveFileExW(tmp.c_str(), dst.c_str(), MOVEFILE_REPLACE_EXISTING)) return true;
    }
    ILog(L"Remplacement de %s impossible (%lu)", dst.c_str(), GetLastError());
    return false;
}

static bool CopySelfTo(const std::wstring& dst) {
    std::wstring self = SelfPath();
    if (IEquals(self, dst)) return true;
    HANDLE f = CreateFileW(self.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz; GetFileSizeEx(f, &sz);
    std::vector<BYTE> buf((size_t)sz.QuadPart); DWORD r = 0;
    ReadFile(f, buf.data(), (DWORD)buf.size(), &r, nullptr); CloseHandle(f);
    if (r != buf.size()) return false;
    return ReplaceFileSafe(dst, buf.data(), (DWORD)buf.size());
}

static bool InstallFiles() {
    std::wstring dir = InstallDir();
    CreateDirectoryW(dir.c_str(), nullptr);
    HRSRC res = FindResourceW(nullptr, L"APODLL", MAKEINTRESOURCEW(10) /* RT_RCDATA */);
    if (!res) { ILog(L"Ressource du moteur introuvable dans l'exe"); return false; }
    HGLOBAL g = LoadResource(nullptr, res);
    const void* data = LockResource(g);
    DWORD size = SizeofResource(nullptr, res);
    if (!data || !size) return false;
    if (!ReplaceFileSafe(InstalledDllPath(), data, size)) return false;
    if (!CopySelfTo(InstalledExePath())) ILog(L"Copie de l'application dans %s impossible (non bloquant)", dir.c_str());
    SetDirEveryoneFull(DYN_DATA_DIR);
    return true;
}

static bool RegisterCom() {
    const std::wstring clsid = ClsidString();
    HKEY k = OpenHklmWrite(L"SOFTWARE\\Classes\\CLSID\\" + clsid);
    if (!k) return false;
    WriteString(k, nullptr, L"Dynamo Audio Processing Object");
    RegCloseKey(k);
    k = OpenHklmWrite(L"SOFTWARE\\Classes\\CLSID\\" + clsid + L"\\InprocServer32");
    if (!k) return false;
    WriteString(k, nullptr, InstalledDllPath());
    WriteString(k, L"ThreadingModel", L"Both");
    RegCloseKey(k);

    // Enregistrement aupres du moteur audio (equivalent de RegisterAPO)
    k = OpenHklmWrite(L"SOFTWARE\\Classes\\AudioEngine\\AudioProcessingObjects\\" + clsid);
    if (!k) return false;
    WriteString(k, L"FriendlyName", L"Dynamo");
    WriteString(k, L"Copyright", L"Dynamo");
    WriteDword(k, L"MajorVersion", 1);
    WriteDword(k, L"MinorVersion", 0);
    WriteDword(k, L"Flags", 0x1 | 0x2 | 0x4 | 0x8);
    WriteDword(k, L"MinInputConnections", 1);
    WriteDword(k, L"MaxInputConnections", 1);
    WriteDword(k, L"MinOutputConnections", 1);
    WriteDword(k, L"MaxOutputConnections", 1);
    WriteDword(k, L"MaxInstances", 0xFFFFFFFF);
    WriteDword(k, L"NumAPOInterfaces", 1);
    WriteString(k, L"APOInterface0", L"{FD7F2B29-24D0-4B5C-B177-592C39F9CA10}");
    RegCloseKey(k);

    // Comme Equalizer APO : desactive la protection de audiodg (sans effet sur les versions recentes)
    k = OpenHklmWrite(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Audio");
    if (k) { WriteDword(k, L"DisableProtectedAudioDG", 1); RegCloseKey(k); }
    return true;
}

static void UnregisterCom() {
    const std::wstring clsid = ClsidString();
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, (L"SOFTWARE\\Classes\\CLSID\\" + clsid).c_str());
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, (L"SOFTWARE\\Classes\\AudioEngine\\AudioProcessingObjects\\" + clsid).c_str());
}

static bool WaitService(SC_HANDLE s, DWORD state, DWORD ms) {
    SERVICE_STATUS st;
    for (DWORD t = 0; t < ms; t += 200) {
        if (!QueryServiceStatus(s, &st)) return false;
        if (st.dwCurrentState == state) return true;
        Sleep(200);
    }
    return false;
}

static bool RestartAudio() {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    SC_HANDLE as  = OpenServiceW(scm, L"Audiosrv", SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS);
    SC_HANDLE aeb = OpenServiceW(scm, L"AudioEndpointBuilder", SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS);
    SERVICE_STATUS st;
    bool ok = true;
    if (as) { ControlService(as, SERVICE_CONTROL_STOP, &st); WaitService(as, SERVICE_STOPPED, 15000); }
    if (aeb) { if (ControlService(aeb, SERVICE_CONTROL_STOP, &st)) WaitService(aeb, SERVICE_STOPPED, 15000); }
    if (aeb) { StartServiceW(aeb, 0, nullptr); WaitService(aeb, SERVICE_RUNNING, 15000); }
    if (as)  { StartServiceW(as, 0, nullptr); ok = WaitService(as, SERVICE_RUNNING, 15000); }
    if (as) CloseServiceHandle(as);
    if (aeb) CloseServiceHandle(aeb);
    CloseServiceHandle(scm);
    ILog(L"Redemarrage du service audio : %s", ok ? L"ok" : L"echec");
    return ok;
}

// Sauvegarde des valeurs d'origine (une seule fois : une reinstallation ne l'ecrase pas)
static bool BackupEndpoint(const std::wstring& guid) {
    std::wstring bpath = std::wstring(kBackupRoot) + guid;
    if (KeyExists(bpath)) return true;
    std::wstring fxPath = std::wstring(kMMDevRender) + guid + L"\\FxProperties";
    HKEY b = OpenHklmWrite(bpath);
    if (!b) return false;
    HKEY fx = OpenHklmRead(fxPath);
    WriteDword(b, L"__FxKeyExisted", fx ? 1 : 0);
    std::vector<std::wstring> absent;
    for (const wchar_t* name : kManaged) {
        RegValue v;
        if (fx && ReadValue(fx, name, v)) RegSetValueExW(b, name, 0, v.type, v.data.data(), (DWORD)v.data.size());
        else absent.push_back(name);
    }
    if (absent.empty()) absent.push_back(L"-");
    WriteMulti(b, L"__Absent", absent);
    if (fx) RegCloseKey(fx);
    RegCloseKey(b);
    ILog(L"Sauvegarde des effets d'origine de %s", guid.c_str());
    return true;
}

static bool RestoreEndpoint(const std::wstring& guid) {
    std::wstring bpath = std::wstring(kBackupRoot) + guid;
    HKEY b = OpenHklmRead(bpath);
    if (!b) return true; // rien a restaurer
    std::wstring fxPath = std::wstring(kMMDevRender) + guid + L"\\FxProperties";
    HKEY fx = OpenHklmWrite(fxPath);
    if (!fx) { RegCloseKey(b); return false; }
    for (const wchar_t* name : kManaged) {
        RegValue v;
        if (ReadValue(b, name, v)) RegSetValueExW(fx, name, 0, v.type, v.data.data(), (DWORD)v.data.size());
        else RegDeleteValueW(fx, name);
    }
    RegValue existed;
    bool fxExisted = ReadValue(b, L"__FxKeyExisted", existed) && existed.data.size() >= 4 && *(DWORD*)existed.data.data() != 0;
    RegCloseKey(fx);
    RegCloseKey(b);
    if (!fxExisted) {
        // la cle n'existait pas : on la supprime si elle est vide
        HKEY chk = OpenHklmRead(fxPath);
        if (chk) {
            DWORD nv = 0, nk = 0;
            RegQueryInfoKeyW(chk, nullptr, nullptr, nullptr, &nk, nullptr, nullptr, &nv, nullptr, nullptr, nullptr, nullptr);
            RegCloseKey(chk);
            if (nv == 0 && nk == 0) {
                HKEY parent = OpenHklmWrite(std::wstring(kMMDevRender) + guid);
                if (parent) { RegDeleteKeyW(parent, L"FxProperties"); RegCloseKey(parent); }
            }
        }
    }
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, bpath.c_str());
    ILog(L"Effets d'origine restaures pour %s", guid.c_str());
    return true;
}

static bool ApplyEndpoint(const std::wstring& guid, int mode) {
    std::wstring fxPath = std::wstring(kMMDevRender) + guid + L"\\FxProperties";
    if (!KeyExists(std::wstring(kMMDevRender) + guid)) { ILog(L"Peripherique %s introuvable", guid.c_str()); return false; }
    HKEY fx = OpenHklmWrite(fxPath);
    if (!fx) return false;
    const std::wstring me = ClsidString();
    const wchar_t* single = mode == MODE_MFX ? V_MFX : mode == MODE_SFX ? V_SFX : mode == MODE_GFX ? V_GFX : V_EFX;
    const wchar_t* composite = mode == MODE_MFX ? V_CMFX : mode == MODE_SFX ? V_CSFX : V_CEFX;
    const wchar_t* pm = mode == MODE_MFX ? V_PM_MFX : mode == MODE_SFX ? V_PM_SFX : V_PM_EFX;
    RegValue v;
    LONG r;
    if (mode == MODE_GFX) {
        // mode historique (Windows 7/8) : effet global apres mixage
        r = WriteString(fx, V_GFX, me);
        RegDeleteValueW(fx, V_SYSFX);
        RegCloseKey(fx);
        ILog(L"Moteur inscrit sur %s en mode GFX (resultat %ld)", guid.c_str(), r);
        return r == ERROR_SUCCESS;
    }
    if (ReadValue(fx, composite, v) && v.type == REG_MULTI_SZ) {
        // Windows 11 : liste d'effets composite -> on se place en tete, on garde ceux du pilote
        std::vector<std::wstring> list = { me };
        for (auto& s : ValueAsMulti(v)) if (!IEquals(s, me)) list.push_back(s);
        r = WriteMulti(fx, composite, list);
    } else {
        r = WriteString(fx, single, me);
    }
    if (!ReadValue(fx, pm, v)) WriteMulti(fx, pm, { kDefaultMode });
    RegDeleteValueW(fx, V_SYSFX); // reactive les "ameliorations audio" sur ce peripherique
    RegCloseKey(fx);
    ILog(L"Moteur inscrit sur %s en mode %s (resultat %ld)", guid.c_str(), mode == MODE_MFX ? L"MFX" : mode == MODE_SFX ? L"SFX" : L"EFX", r);
    return r == ERROR_SUCCESS;
}

static std::vector<std::wstring> BackedUpEndpoints() {
    std::vector<std::wstring> out;
    HKEY k = OpenHklmRead(L"SOFTWARE\\Dynamo\\Endpoints");
    if (!k) return out;
    wchar_t name[256];
    for (DWORD i = 0;; ++i) {
        DWORD len = 256;
        if (RegEnumKeyExW(k, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
        out.push_back(name);
    }
    RegCloseKey(k);
    return out;
}

static bool ValidGuid(const std::wstring& g) {
    if (g.size() != 38 || g[0] != L'{' || g[37] != L'}') return false;
    for (wchar_t c : g) if (!(iswxdigit(c) || c == L'-' || c == L'{' || c == L'}')) return false;
    return true;
}

const char* ModeName(int mode) {
    switch (mode) { case MODE_MFX: return "MFX"; case MODE_GFX: return "GFX"; case MODE_SFX: return "SFX"; default: return "EFX"; }
}

static IMMDevice* GetEndpoint(const std::wstring& id) {
    IMMDeviceEnumerator* en = nullptr;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&en))) return nullptr;
    IMMDevice* dev = nullptr;
    en->GetDevice(id.c_str(), &dev);
    en->Release();
    return dev;
}

struct IDynMeterInfo : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float* pfPeak) = 0;
};
static const IID IID_DynMeterInfo = { 0xC02216F6, 0x8C67, 0x4B5B, { 0x9D, 0x00, 0xD0, 0x08, 0xE7, 0x3E, 0x00, 0x64 } };

float EndpointPeak(const std::wstring& id) {
    IMMDevice* dev = GetEndpoint(id);
    if (!dev) return -1;
    IDynMeterInfo* m = nullptr;
    float pk = -1;
    if (SUCCEEDED(dev->Activate(IID_DynMeterInfo, CLSCTX_ALL, nullptr, (void**)&m))) { m->GetPeakValue(&pk); m->Release(); }
    dev->Release();
    return pk;
}

static bool ModuleLoadedInAudiodg() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe = { sizeof(pe) };
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok && !found; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, L"audiodg.exe") != 0) continue;
        HANDLE ms = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pe.th32ProcessID);
        if (ms == INVALID_HANDLE_VALUE) { ILog(L"Test : lecture des modules de audiodg impossible (%lu)", GetLastError()); continue; }
        MODULEENTRY32W me = { sizeof(me) };
        for (BOOL m = Module32FirstW(ms, &me); m; m = Module32NextW(ms, &me))
            if (_wcsicmp(me.szModule, L"DynamoAPO.dll") == 0) { found = true; break; }
        CloseHandle(ms);
    }
    CloseHandle(snap);
    return found;
}

// Joue un signal inaudible (-120 dB) sur la sortie et verifie que Windows fait tourner le moteur
static bool EngineAliveCheck(const std::wstring& guid, DWORD timeoutMs) {
    std::wstring logPath = DYN_LOG_FILE;
    LONGLONG logStart = 0;
    { WIN32_FILE_ATTRIBUTE_DATA a; if (GetFileAttributesExW(logPath.c_str(), GetFileExInfoStandard, &a)) logStart = ((LONGLONG)a.nFileSizeHigh << 32) | a.nFileSizeLow; }
    HANDLE map = nullptr; DynShared* sh = nullptr; int32_t beat0 = 0; bool haveBeat = false;
    IMMDevice* dev = GetEndpoint(L"{0.0.0.00000000}." + guid);
    IAudioClient* ac = nullptr; IAudioRenderClient* rc = nullptr; WAVEFORMATEX* wf = nullptr;
    if (dev && SUCCEEDED(dev->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&ac)) && SUCCEEDED(ac->GetMixFormat(&wf)) &&
        SUCCEEDED(ac->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 2000000, 0, wf, nullptr)) &&
        SUCCEEDED(ac->GetService(__uuidof(IAudioRenderClient), (void**)&rc))) {
        ac->Start();
    } else ILog(L"Test : impossible d'ouvrir un flux sur la sortie");
    bool alive = false;
    unsigned seed = 1;
    for (DWORD t = 0; t < timeoutMs && !alive; t += 100) {
        if (rc) {
            UINT32 total = 0, pad = 0;
            ac->GetBufferSize(&total); ac->GetCurrentPadding(&pad);
            UINT32 n = total - pad; BYTE* buf = nullptr;
            if (n && SUCCEEDED(rc->GetBuffer(n, &buf))) {
                bool isFloat = wf->wBitsPerSample == 32;
                size_t cnt = (size_t)n * wf->nChannels;
                for (size_t i = 0; i < cnt; ++i) {
                    seed = seed * 1664525u + 1013904223u;
                    float v = ((seed >> 9) / 8388608.f - 1.f) * 1e-6f;
                    if (isFloat) ((float*)buf)[i] = v; else if (wf->wBitsPerSample == 16) ((short*)buf)[i] = 0; else memset(buf, 0, (size_t)n * wf->nBlockAlign);
                }
                rc->ReleaseBuffer(n, 0);
            }
        }
        Sleep(100);
        if (!sh) {
            HANDLE sf = nullptr;
            sh = DynOpenShared(&sf, &map, true);
            if (sf) CloseHandle(sf);
            if (sh) { beat0 = sh->heartbeat; haveBeat = true; }
        } else if (haveBeat && sh->heartbeat != beat0) { alive = true; ILog(L"Test : le moteur traite le son"); }
        if (!alive && t % 1000 == 0 && ModuleLoadedInAudiodg()) { alive = true; ILog(L"Test : DynamoAPO.dll est charge dans audiodg.exe"); }
        if (!alive && t % 1000 == 0) {
            // nouvelle ligne LockForProcess depuis le debut du test
            HANDLE f = CreateFileW(logPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
            if (f != INVALID_HANDLE_VALUE) {
                LARGE_INTEGER sz; GetFileSizeEx(f, &sz);
                if (sz.QuadPart > logStart) {
                    std::string add((size_t)(sz.QuadPart - logStart), '\0');
                    LARGE_INTEGER p; p.QuadPart = logStart; SetFilePointerEx(f, p, nullptr, FILE_BEGIN);
                    DWORD r = 0; ReadFile(f, &add[0], (DWORD)add.size(), &r, nullptr);
                    if (add.find("LockForProcess :") != std::string::npos) alive = true;
                }
                CloseHandle(f);
            }
        }
    }
    if (ac) ac->Stop();
    if (rc) rc->Release();
    if (ac) ac->Release();
    if (wf) CoTaskMemFree(wf);
    if (dev) dev->Release();
    if (sh) UnmapViewOfFile(sh);
    if (map) CloseHandle(map);
    return alive;
}

int RunElevatedCommand(int argc, wchar_t** argv) {
    EnablePrivilege(SE_BACKUP_NAME);
    EnablePrivilege(SE_RESTORE_NAME);
    EnablePrivilege(SE_TAKE_OWNERSHIP_NAME);
    EnablePrivilege(SE_DEBUG_NAME);
    CreateDirectoryW(DYN_DATA_DIR, nullptr);
    SetDirEveryoneFull(DYN_DATA_DIR);
    if (argc < 1) return 2;
    std::wstring cmd = argv[0];
    ILog(L"--- commande administrateur : %s", cmd.c_str());

    if (cmd == L"install" && argc >= 3) {
        std::wstring guid = argv[1];
        int mode = _wtoi(argv[2]); if (mode < 0 || mode > 3) mode = MODE_EFX;
        if (!ValidGuid(guid)) return 3;
        if (!InstallFiles()) return 10;
        if (!RegisterCom()) return 11;
        if (!BackupEndpoint(guid)) return 12;
        // si deja installe dans un autre mode : on repart de l'etat d'origine
        RestoreEndpoint(guid);
        if (!BackupEndpoint(guid)) return 12;
        if (!ApplyEndpoint(guid, mode)) return 13;
        RestartAudio();
        return 0;
    }
    if (cmd == L"uninstall" && argc >= 2) {
        std::wstring guid = argv[1];
        if (!ValidGuid(guid)) return 3;
        if (!RestoreEndpoint(guid)) return 20;
        RestartAudio();
        return 0;
    }
    if (cmd == L"uninstall-all") {
        for (auto& g : BackedUpEndpoints()) RestoreEndpoint(g);
        UnregisterCom();
        RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Dynamo");
        RestartAudio();
        std::wstring dll = InstalledDllPath();
        if (!DeleteFileW(dll.c_str())) MoveFileExW(dll.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        std::wstring exe = InstalledExePath();
        if (!IEquals(exe, SelfPath())) { if (!DeleteFileW(exe.c_str())) MoveFileExW(exe.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT); }
        else MoveFileExW(exe.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(InstallDir().c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        return 0;
    }
    if (cmd == L"restart-audio") return RestartAudio() ? 0 : 30;
    if (cmd == L"cleanup-legacy") {
        // supprime tout ce que les versions 1.x (moteur dans Windows) ont installe
        for (auto& g : BackedUpEndpoints()) RestoreEndpoint(g);
        // securite : retire notre CLSID de toutes les sorties, meme sans sauvegarde
        HKEY rk = OpenHklmRead(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\MMDevices\\Audio\\Render");
        if (rk) {
            wchar_t name[256];
            for (DWORD i = 0;; ++i) {
                DWORD len = 256;
                if (RegEnumKeyExW(rk, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
                std::wstring fxp = std::wstring(kMMDevRender) + name + L"\\FxProperties";
                HKEY fx = OpenHklmRead(fxp);
                if (!fx) continue;
                std::vector<const wchar_t*> ours;
                for (const wchar_t* v : kManaged) { RegValue rv; if (ReadValue(fx, v, rv) && (rv.type == REG_SZ) && IEquals(ValueAsString(rv), ClsidString())) ours.push_back(v); }
                RegCloseKey(fx);
                if (!ours.empty()) { HKEY w = OpenHklmWrite(fxp); if (w) { for (auto v : ours) RegDeleteValueW(w, v); RegCloseKey(w); } ILog(L"Nettoyage de %s", name); }
            }
            RegCloseKey(rk);
        }
        UnregisterCom();
        RegDeleteTreeW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Dynamo");
        HKEY ak = OpenHklmWrite(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Audio");
        if (ak) { RegDeleteValueW(ak, L"DisableProtectedAudioDG"); RegCloseKey(ak); }
        RestartAudio();
        std::wstring dir = InstallDir();
        const wchar_t* files[] = { L"\\DynamoAPO.dll", L"\\Dynamo.exe" };
        for (auto f : files) { std::wstring p = dir + f; if (!DeleteFileW(p.c_str())) MoveFileExW(p.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT); }
        WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((dir + L"\\*.old").c_str(), &fd);
        if (h != INVALID_HANDLE_VALUE) { do { std::wstring p = dir + L"\\" + fd.cFileName; if (!DeleteFileW(p.c_str())) MoveFileExW(p.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT); } while (FindNextFileW(h, &fd)); FindClose(h); }
        if (!RemoveDirectoryW(dir.c_str())) MoveFileExW(dir.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
        const wchar_t* data[] = { L"\\shared.bin", L"\\state.bin", L"\\moteur.log" };
        for (auto f : data) DeleteFileW((std::wstring(DYN_DATA_DIR) + f).c_str());
        ILog(L"Anciennes versions supprimees");
        return 0;
    }
    if (cmd == L"autofix" && argc >= 2) {
        std::wstring guid = argv[1];
        if (!ValidGuid(guid)) return 3;
        if (!InstallFiles()) return 10;
        if (!RegisterCom()) return 11;
        const int order[4] = { MODE_EFX, MODE_MFX, MODE_GFX, MODE_SFX };
        for (int i = 0; i < 4; ++i) {
            int mode = order[i];
            if (!BackupEndpoint(guid)) return 12;
            RestoreEndpoint(guid);
            if (!BackupEndpoint(guid)) return 12;
            if (!ApplyEndpoint(guid, mode)) return 13;
            RestartAudio();
            Sleep(1500);
            if (EngineAliveCheck(guid, 8000)) { ILog(L"Reparation reussie : moteur actif en mode %S", ModeName(mode)); return 100 + mode; }
            ILog(L"Mode %S : le moteur n'a pas ete charge par Windows", ModeName(mode));
        }
        // rien n'a fonctionne : on laisse le mode standard
        RestoreEndpoint(guid); BackupEndpoint(guid); ApplyEndpoint(guid, MODE_EFX); RestartAudio();
        return 40;
    }
    return 2;
}

bool ProcessRunning(const wchar_t* exeName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe = { sizeof(pe) };
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe))
        if (_wcsicmp(pe.szExeFile, exeName) == 0) { found = true; break; }
    CloseHandle(snap);
    return found;
}

int KillProcess(const wchar_t* exeName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe = { sizeof(pe) };
    int n = 0;
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, exeName) != 0) continue;
        HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
        if (h) { if (TerminateProcess(h, 0)) ++n; CloseHandle(h); }
    }
    CloseHandle(snap);
    return n;
}

bool ComRegistered() {
    HKEY k = OpenHklmRead(L"SOFTWARE\\Classes\\CLSID\\" + ClsidString() + L"\\InprocServer32");
    if (!k) return false;
    RegValue v; bool ok = ReadValue(k, nullptr, v) && GetFileAttributesW(ValueAsString(v).c_str()) != INVALID_FILE_ATTRIBUTES;
    RegCloseKey(k);
    return ok;
}

bool LegacyPresent() {
    if (KeyExists(L"SOFTWARE\\Classes\\CLSID\\" + ClsidString())) return true;
    if (KeyExists(L"SOFTWARE\\Dynamo")) return true;
    if (GetFileAttributesW(InstallDir().c_str()) != INVALID_FILE_ATTRIBUTES) return true;
    return false;
}

// Met a la corbeille les autres Dynamo*.exe du dossier de l'application (anciennes versions)
int RecycleOldExecutables() {
    std::wstring self = SelfPath();
    size_t slash = self.find_last_of(L"\\/");
    std::wstring dir = self.substr(0, slash);
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((dir + L"\\Dynamo*.exe").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    std::wstring list;
    int n = 0;
    do {
        std::wstring p = dir + L"\\" + fd.cFileName;
        if (IEquals(p, self)) continue;
        // verifie que c'est bien un Dynamo (description de version)
        DWORD hnd = 0, sz = GetFileVersionInfoSizeW(p.c_str(), &hnd);
        if (!sz) continue;
        std::vector<BYTE> vi(sz);
        if (!GetFileVersionInfoW(p.c_str(), 0, sz, vi.data())) continue;
        wchar_t* prod = nullptr; UINT plen = 0;
        if (!VerQueryValueW(vi.data(), L"\\StringFileInfo\\040C04B0\\ProductName", (void**)&prod, &plen) || !prod || wcscmp(prod, L"Dynamo") != 0) continue;
        list += p; list.push_back(0); ++n;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!n) return 0;
    list.push_back(0);
    SHFILEOPSTRUCTW op = {};
    op.wFunc = FO_DELETE;
    op.pFrom = list.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    return SHFileOperationW(&op) == 0 ? n : 0;
}

} // namespace sys
