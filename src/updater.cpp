// Kinetik KDYN-10 (ex-Dynamo) - mise a jour automatique (WinHTTP + SHA-256), sans droits administrateur.
//
// Publication : la page Releases du depot contient deux fichiers
//   latest.txt  : ligne 1 = version (ex. 2.3.0), ligne 2 = SHA-256 de Dynamo.exe
//   Dynamo.exe  : l'application
// et ils sont lus a l'adresse DYN_UPDATE_BASE (version.h) + <fichier>.
//
// Installation : le fichier .exe en cours d'execution peut etre renomme sous Windows ;
// on renomme donc Dynamo.exe -> Dynamo.exe.old puis on met la nouvelle version a sa place.
// La nouvelle version est utilisee au prochain lancement (ou tout de suite si l'interface redemarre).
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "updater.h"
#include "version.h"
extern "C" {
#include "monocypher/monocypher-ed25519.h"
}

// cle publique de signature des mises a jour Kinetik (la cle privee = secret KINETIK_UPDATE_KEY de la CI)
static const unsigned char kUpdateKey[32] = {
    0x59, 0x1d, 0xdc, 0xfe, 0xd6, 0x59, 0xcf, 0xeb, 0x44, 0x1e, 0x69, 0x1f, 0x8b, 0xa6, 0xc6, 0xdc,
    0x93, 0xe7, 0xef, 0x76, 0x13, 0x32, 0x25, 0x6f, 0xe1, 0xcf, 0xf1, 0x9d, 0x96, 0x0b, 0x7d, 0xfa};

// latest.txt ligne 3 : "sig <128 hex>" = Ed25519 sur "KINETIK-KDYN10 <version> <sha256>\n"
static bool SignatureOk(const std::string& ver, const std::string& sha, const std::string& sigLine) {
    if (sigLine.rfind("sig ", 0) != 0 || sigLine.size() != 4 + 128) return false;
    unsigned char sig[64];
    for (int i = 0; i < 64; ++i) {
        unsigned v = 0;
        for (int k = 0; k < 2; ++k) {
            const char c = sigLine[4 + i * 2 + k]; v <<= 4;
            if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
            else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
            else return false;
        }
        sig[i] = (unsigned char)v;
    }
    const std::string msg = "KINETIK-KDYN10 " + ver + " " + sha + "\n";
    return crypto_ed25519_check(sig, kUpdateKey, (const unsigned char*)msg.data(), msg.size()) == 0;
}

static const DWORD kMaxExeSize = 64u << 20;

static std::wstring SelfPath() {
    wchar_t p[MAX_PATH * 2]; GetModuleFileNameW(nullptr, p, MAX_PATH * 2); return p;
}

static void ULog(const char* fmt, ...) {
    char msg[512];
    va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap);
    wchar_t dir[MAX_PATH]; DWORD n = GetEnvironmentVariableW(L"APPDATA", dir, MAX_PATH);
    if (!n || n >= MAX_PATH) return;
    std::wstring path = std::wstring(dir) + L"\\Dynamo";
    CreateDirectoryW(path.c_str(), nullptr);
    path += L"\\update.log";
    HANDLE f = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER sz; if (GetFileSizeEx(f, &sz) && sz.QuadPart > 256 * 1024) { CloseHandle(f); DeleteFileW(path.c_str()); return; }
    SYSTEMTIME st; GetLocalTime(&st);
    char line[600];
    int k = snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d:%02d %s\r\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, msg);
    DWORD w; WriteFile(f, line, (DWORD)k, &w, nullptr);
    CloseHandle(f);
}

int Updater::CompareVersions(const char* a, const char* b) {
    int va[3] = {0, 0, 0}, vb[3] = {0, 0, 0};
    sscanf(a, "%d.%d.%d", &va[0], &va[1], &va[2]);
    sscanf(b, "%d.%d.%d", &vb[0], &vb[1], &vb[2]);
    for (int i = 0; i < 3; ++i) if (va[i] != vb[i]) return va[i] < vb[i] ? -1 : 1;
    return 0;
}

void Updater::CleanupPrevious() {
    std::wstring old = SelfPath() + L".old";
    DeleteFileW(old.c_str());
    DeleteFileW((SelfPath() + L".download").c_str());
}

// ---------------------------------------------------------------- HTTPS
static bool HttpGet(const std::wstring& url, std::vector<char>& out, DWORD maxSize, volatile long* stop) {
    out.clear();
    URL_COMPONENTS uc = { sizeof(uc) };
    wchar_t host[256], path[2048];
    uc.lpszHostName = host; uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) return false;

    HINTERNET ses = WinHttpOpen(L"Dynamo/" DYN_VERSION_SHORT, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) ses = WinHttpOpen(L"Dynamo/" DYN_VERSION_SHORT, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return false;
    WinHttpSetTimeouts(ses, 15000, 15000, 30000, 60000);
    DWORD protos = 0x00000800 /* TLS 1.2 */ | 0x00002000 /* TLS 1.3 */;
    WinHttpSetOption(ses, WINHTTP_OPTION_SECURE_PROTOCOLS, &protos, sizeof(protos));
    bool ok = false;
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (con) {
        HINTERNET req = WinHttpOpenRequest(con, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
        if (req) {
            // les redirections (github.com -> objects.githubusercontent.com) sont suivies automatiquement
            if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                WinHttpReceiveResponse(req, nullptr)) {
                DWORD status = 0, sz = sizeof(status);
                WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
                if (status == 200) {
                    ok = true;
                    for (;;) {
                        if (stop && *stop) { ok = false; break; }
                        DWORD avail = 0;
                        if (!WinHttpQueryDataAvailable(req, &avail)) { ok = false; break; }
                        if (!avail) break;
                        if (out.size() + avail > maxSize) { ok = false; break; }
                        size_t at = out.size();
                        out.resize(at + avail);
                        DWORD got = 0;
                        if (!WinHttpReadData(req, &out[at], avail, &got)) { ok = false; break; }
                        out.resize(at + got);
                    }
                } else ULog("HTTP %lu pour %ls", status, url.c_str());
            }
            WinHttpCloseHandle(req);
        }
        WinHttpCloseHandle(con);
    }
    WinHttpCloseHandle(ses);
    return ok;
}

static std::string Sha256Hex(const std::vector<char>& data) {
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE h = nullptr;
    unsigned char digest[32];
    std::string hex;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return hex;
    if (BCryptCreateHash(alg, &h, nullptr, 0, nullptr, 0, 0) == 0) {
        BCryptHashData(h, (PUCHAR)data.data(), (ULONG)data.size(), 0);
        if (BCryptFinishHash(h, digest, 32, 0) == 0) {
            char b[3];
            for (int i = 0; i < 32; ++i) { snprintf(b, 3, "%02x", digest[i]); hex += b; }
        }
        BCryptDestroyHash(h);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return hex;
}

// Lit le nom de produit et la version dans les ressources d'un .exe telecharge
static bool ReadExeIdentity(const std::wstring& file, std::string& product, std::string& version) {
    DWORD hnd = 0, sz = GetFileVersionInfoSizeW(file.c_str(), &hnd);
    if (!sz) return false;
    std::vector<BYTE> vi(sz);
    if (!GetFileVersionInfoW(file.c_str(), 0, sz, vi.data())) return false;
    wchar_t* p = nullptr; UINT len = 0;
    if (VerQueryValueW(vi.data(), L"\\StringFileInfo\\040C04B0\\ProductName", (void**)&p, &len) && p) {
        char b[128]; WideCharToMultiByte(CP_UTF8, 0, p, -1, b, sizeof(b), nullptr, nullptr); product = b;
    }
    if (VerQueryValueW(vi.data(), L"\\StringFileInfo\\040C04B0\\ProductVersion", (void**)&p, &len) && p) {
        char b[64]; WideCharToMultiByte(CP_UTF8, 0, p, -1, b, sizeof(b), nullptr, nullptr); version = b;
    }
    return true;
}

static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// ---------------------------------------------------------------- verification + installation
bool Updater::CheckOnce() {
    state = CHECKING;
    std::wstring base = DYN_UPDATE_BASE;
    wchar_t ov[512];   // adresse de test (developpement uniquement)
    if (GetEnvironmentVariableW(L"DYNAMO_UPDATE_BASE", ov, 512)) base = ov;
    std::vector<char> txt;
    if (!HttpGet(base + L"latest.txt", txt, 4096, &stop_)) {
        snprintf(lastError, sizeof(lastError), "Serveur de mise à jour injoignable.");
        state = FAILED; return false;
    }
    std::string s(txt.begin(), txt.end());
    std::vector<std::string> lines;
    for (size_t p = 0; p <= s.size();) { size_t e = s.find('\n', p); if (e == std::string::npos) e = s.size(); lines.push_back(Trim(s.substr(p, e - p))); p = e + 1; }
    std::string ver = lines.size() > 0 ? lines[0] : "", sha = lines.size() > 1 ? lines[1] : "", sigLine = lines.size() > 2 ? lines[2] : "";
    for (auto& c : sha) c = (char)tolower((unsigned char)c);
    if (ver.empty() || sha.size() != 64) { snprintf(lastError, sizeof(lastError), "Fichier latest.txt invalide."); state = FAILED; return false; }
    // une version n'est installee que si elle est signee par la cle Kinetik : un acces au depot de
    // telechargements ne suffit pas pour faire installer autre chose
    if (!SignatureOk(ver, sha, sigLine)) {
        ULog("Signature absente ou invalide pour la version %s", ver.c_str());
        snprintf(lastError, sizeof(lastError), "Mise à jour %s refusée (signature invalide).", ver.c_str());
        state = FAILED; return false;
    }

    const char* installed = newVersion[0] ? newVersion : DYN_VERSION_STR;
    if (CompareVersions(ver.c_str(), installed) <= 0) { state = newVersion[0] ? READY : UP_TO_DATE; return true; }

    ULog("Nouvelle version %s disponible (actuelle %s)", ver.c_str(), DYN_VERSION_STR);
    state = DOWNLOADING;
    std::vector<char> exe;
    if (!HttpGet(base + L"Dynamo.exe", exe, kMaxExeSize, &stop_)) {
        snprintf(lastError, sizeof(lastError), "Téléchargement de la version %s interrompu.", ver.c_str());
        state = FAILED; return false;
    }
    std::string got = Sha256Hex(exe);
    if (got != sha) {
        ULog("Empreinte incorrecte : attendu %s, recu %s", sha.c_str(), got.c_str());
        snprintf(lastError, sizeof(lastError), "Mise à jour %s rejetée (empreinte incorrecte).", ver.c_str());
        state = FAILED; return false;
    }
    if (exe.size() < 2 || exe[0] != 'M' || exe[1] != 'Z') { snprintf(lastError, sizeof(lastError), "Fichier reçu invalide."); state = FAILED; return false; }

    const std::wstring self = SelfPath();
    const std::wstring tmp = self + L".download", old = self + L".old";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        ULog("Ecriture impossible a cote de %ls (%lu)", self.c_str(), GetLastError());
        snprintf(lastError, sizeof(lastError), "Le dossier de KDYN-10 n'est pas modifiable.");
        state = FAILED; return false;
    }
    DWORD w = 0; BOOL ok = WriteFile(f, exe.data(), (DWORD)exe.size(), &w, nullptr);
    FlushFileBuffers(f); CloseHandle(f);
    if (!ok || w != exe.size()) { DeleteFileW(tmp.c_str()); state = FAILED; return false; }

    std::string product, fileVer;
    ReadExeIdentity(tmp, product, fileVer);
    // "Dynamo" reste le nom interne (ProductName) : les copies deja installees n'acceptent que ce nom
    if (product != "Dynamo") {
        ULog("Fichier telecharge refuse (produit '%s')", product.c_str());
        DeleteFileW(tmp.c_str());
        snprintf(lastError, sizeof(lastError), "Fichier reçu refusé (ce n'est pas KDYN-10).");
        state = FAILED; return false;
    }

    // remplacement : l'exe en cours peut etre renomme
    DeleteFileW(old.c_str());
    if (!MoveFileExW(self.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        ULog("Renommage de l'ancienne version impossible (%lu)", GetLastError());
        DeleteFileW(tmp.c_str()); state = FAILED; return false;
    }
    if (!MoveFileExW(tmp.c_str(), self.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        ULog("Mise en place impossible (%lu) : retour a l'ancienne version", GetLastError());
        MoveFileExW(old.c_str(), self.c_str(), MOVEFILE_REPLACE_EXISTING);
        DeleteFileW(tmp.c_str()); state = FAILED; return false;
    }
    // le fichier vient d'Internet via Dynamo : pas de marque "telecharge", donc pas d'avertissement SmartScreen
    snprintf(newVersion, sizeof(newVersion), "%s", ver.c_str());
    lastError[0] = 0;
    ULog("Version %s installee, active au prochain demarrage", ver.c_str());
    state = READY;
    return true;
}

// ---------------------------------------------------------------- fil d'arriere-plan
unsigned long __stdcall Updater::ThreadProc(void* self) { ((Updater*)self)->Run(); return 0; }

static DWORD g_firstDelayMs = 20000;

void Updater::Run() {
    DWORD wait = g_firstDelayMs;
    while (!stop_) {
        WaitForSingleObject((HANDLE)wake_, wait);
        if (stop_) break;
        bool ok = CheckOnce();
        // toutes les 6 h ; en cas d'echec reseau, nouvel essai dans 30 min
        wait = ok ? 6 * 3600 * 1000 : 30 * 60 * 1000;
    }
}

void Updater::Start(int delaySec) {
    if (thread_) return;
    g_firstDelayMs = (DWORD)delaySec * 1000;
    stop_ = 0;
    wake_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    thread_ = CreateThread(nullptr, 0, ThreadProc, this, 0, nullptr);
    if (thread_) SetThreadPriority((HANDLE)thread_, THREAD_PRIORITY_BELOW_NORMAL);
}

void Updater::CheckNow() { if (wake_) SetEvent((HANDLE)wake_); }

void Updater::Stop() {
    if (!thread_) return;
    InterlockedExchange(&stop_, 1);
    SetEvent((HANDLE)wake_);
    WaitForSingleObject((HANDLE)thread_, 3000);
    CloseHandle((HANDLE)thread_); thread_ = nullptr;
    CloseHandle((HANDLE)wake_); wake_ = nullptr;
}
