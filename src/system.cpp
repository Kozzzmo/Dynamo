// Dynamo - fonctions systeme (sans droits administrateur)

#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <algorithm>

#include "system.h"

namespace sys {

static const PROPERTYKEY kFriendlyName = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } }, 14 };
static const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

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

std::wstring DataDir() {
    PWSTR p = nullptr; std::wstring r = L".";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &p))) { r = p; CoTaskMemFree(p); }
    r += L"\\Dynamo";
    CreateDirectoryW(r.c_str(), nullptr);
    return r;
}

std::wstring ReadFileTail(const std::wstring& path, size_t maxChars) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return L"";
    LARGE_INTEGER sz; GetFileSizeEx(f, &sz);
    LONGLONG start = sz.QuadPart > (LONGLONG)maxChars ? sz.QuadPart - (LONGLONG)maxChars : 0;
    LARGE_INTEGER pos; pos.QuadPart = start; SetFilePointerEx(f, pos, nullptr, FILE_BEGIN);
    std::string buf((size_t)(sz.QuadPart - start), '\0');
    DWORD r = 0; if (!buf.empty()) ReadFile(f, &buf[0], (DWORD)buf.size(), &r, nullptr);
    CloseHandle(f);
    buf.resize(r);
    return FromUtf8(buf);
}

static std::wstring SelfPath() {
    wchar_t p[MAX_PATH * 2]; GetModuleFileNameW(nullptr, p, MAX_PATH * 2); return p;
}

// ---------------------------------------------------------------- sorties audio
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
            out.push_back(d);
            dev->Release();
        }
        col->Release();
    }
    en->Release();
    std::stable_sort(out.begin(), out.end(), [](const Device& a, const Device& b) { return a.isDefault > b.isDefault; });
    return out;
}

// ---------------------------------------------------------------- sortie par defaut
// IPolicyConfig : interface de Windows utilisee par le panneau Son (non documentee, stable depuis Windows 7)
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
    HKEY k; if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    if (on) {
        std::wstring v = L"\"" + SelfPath() + L"\" --tray";
        RegSetValueExW(k, L"Dynamo", 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * sizeof(wchar_t)));
    } else RegDeleteValueW(k, L"Dynamo");
    RegCloseKey(k);
}

} // namespace sys
