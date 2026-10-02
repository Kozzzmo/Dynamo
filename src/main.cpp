// Dynamo - point d'entree Windows : fenetre, Direct3D 11, zone de notification.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <objbase.h>
#include <d3d11.h>
#include <string>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "app.h"
#include "ui.h"
#include "system.h"
#include "version.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define WM_TRAY (WM_APP + 1)
#define WM_SHOW_EXISTING (WM_APP + 2)
#define ID_TRAY_SHOW   1001
#define ID_TRAY_BYPASS 1002
#define ID_TRAY_QUIT   1003
#define ID_TRAY_PRESET 1100

static ID3D11Device* g_dev = nullptr;
static ID3D11DeviceContext* g_ctx = nullptr;
static IDXGISwapChain* g_swap = nullptr;
static ID3D11RenderTargetView* g_rtv = nullptr;
static UINT g_resizeW = 0, g_resizeH = 0;
static HWND g_hwnd = nullptr;
static bool g_visible = true, g_quit = false;
static App g_app;
static UiContext g_ui;
static NOTIFYICONDATAW g_nid = {};
static UINT g_msgTaskbarCreated = 0;

static void CreateRT() {
    ID3D11Texture2D* back = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&back));
    if (back) { g_dev->CreateRenderTargetView(back, nullptr, &g_rtv); back->Release(); }
}
static void CleanupRT() { if (g_rtv) { g_rtv->Release(); g_rtv = nullptr; } }

static bool CreateDevice(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_dev, &got, &g_ctx);
    if (hr == DXGI_ERROR_UNSUPPORTED || FAILED(hr))
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd, &g_swap, &g_dev, &got, &g_ctx);
    if (FAILED(hr)) return false;
    CreateRT();
    return true;
}

static void CleanupDevice() {
    CleanupRT();
    if (g_swap) { g_swap->Release(); g_swap = nullptr; }
    if (g_ctx) { g_ctx->Release(); g_ctx = nullptr; }
    if (g_dev) { g_dev->Release(); g_dev = nullptr; }
}

// ---------------------------------------------------------------- zone de notification
static void TrayAdd() {
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = g_hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAY;
    g_nid.hIcon = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
    wcscpy_s(g_nid.szTip, L"Dynamo");
    Shell_NotifyIconW(NIM_ADD, &g_nid);
}
static void TrayUpdateTip() {
    std::wstring t = L"Dynamo — " + sys::FromUtf8(g_app.CurrentPresetLabel());
    if (g_app.params.bypass) t += L" (contourné)";
    wcsncpy_s(g_nid.szTip, t.c_str(), _TRUNCATE);
    g_nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}
static void TrayRemove() { Shell_NotifyIconW(NIM_DELETE, &g_nid); }

static bool g_restarting = false;
// Relance Dynamo (nouvelle version installee) sans couper le routage plus que necessaire
static void RestartSelf(bool hidden) {
    wchar_t self[MAX_PATH * 2]; GetModuleFileNameW(nullptr, self, MAX_PATH * 2);
    g_restarting = true;
    g_app.keepRoutingOnExit = true;
    ShellExecuteW(nullptr, L"open", self, hidden ? L"--tray --updated" : L"--updated", nullptr, SW_SHOWNORMAL);
    g_quit = true;
}

static void ShowMain() {
    g_visible = true;
    ShowWindow(g_hwnd, IsIconic(g_hwnd) ? SW_RESTORE : SW_SHOW);
    SetForegroundWindow(g_hwnd);
}
static void HideMain() { g_visible = false; ShowWindow(g_hwnd, SW_HIDE); }

static void TrayMenu() {
    HMENU m = CreatePopupMenu();
    HMENU pm = CreatePopupMenu();
    for (int i = 0; i < (int)g_app.presets.size() && i < 90; ++i)
        AppendMenuW(pm, MF_STRING | (i == g_app.currentPreset ? MF_CHECKED : 0), ID_TRAY_PRESET + i, sys::FromUtf8(g_app.presets[i].name).c_str());
    AppendMenuW(m, MF_STRING | MF_DEFAULT, ID_TRAY_SHOW, L"Ouvrir Dynamo");
    AppendMenuW(m, MF_POPUP, (UINT_PTR)pm, L"Presets");
    AppendMenuW(m, MF_STRING | (g_app.params.bypass ? MF_CHECKED : 0), ID_TRAY_BYPASS, L"Contourner (son d'origine)");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_TRAY_QUIT, L"Quitter");
    POINT pt; GetCursorPos(&pt);
    SetForegroundWindow(g_hwnd);
    UINT cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, g_hwnd, nullptr);
    DestroyMenu(m);
    if (cmd == ID_TRAY_SHOW) ShowMain();
    else if (cmd == ID_TRAY_BYPASS) { g_app.params.bypass = !g_app.params.bypass; g_app.ParamsChanged(); }
    else if (cmd == ID_TRAY_QUIT) g_quit = true;
    else if (cmd >= ID_TRAY_PRESET && cmd < ID_TRAY_PRESET + 90) g_app.LoadPreset(cmd - ID_TRAY_PRESET);
    TrayUpdateTip();
}

// ---------------------------------------------------------------- fenetre
static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    if (msg == g_msgTaskbarCreated && g_msgTaskbarCreated) { TrayAdd(); return 0; }
    switch (msg) {
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) return 0;
        g_resizeW = LOWORD(lParam); g_resizeH = HIWORD(lParam);
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mm = (MINMAXINFO*)lParam;
        UINT dpi = GetDpiForWindow(hWnd); if (!dpi) dpi = 96;
        mm->ptMinTrackSize.x = MulDiv(1080, dpi, 96);
        mm->ptMinTrackSize.y = MulDiv(740, dpi, 96);
        return 0;
    }
    case WM_CLOSE:
        if (g_app.settings.closeToTray) HideMain(); else g_quit = true;
        return 0;
    case WM_TRAY:
        if (LOWORD(lParam) == WM_LBUTTONUP || LOWORD(lParam) == WM_LBUTTONDBLCLK) ShowMain();
        else if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) TrayMenu();
        return 0;
    case WM_SHOW_EXISTING:
        ShowMain();
        return 0;
    case WM_APP + 3:
        g_app.keepRoutingOnExit = true;   // une autre instance (nouvelle version) prend le relais
        g_quit = true;
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) return 0;
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void LoadFonts(float dpi) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    static const ImWchar ranges[] = { 0x0020, 0x00FF, 0x0152, 0x0153, 0x2013, 0x2014, 0x2019, 0x201D, 0x2022, 0x2022, 0x2026, 0x2026,
                                      0x2190, 0x2193, 0x221E, 0x221E, 0x2248, 0x2248, 0 };
    wchar_t win[MAX_PATH]; GetWindowsDirectoryW(win, MAX_PATH);
    std::string fonts = sys::ToUtf8(std::wstring(win) + L"\\Fonts\\");
    auto load = [&](const char* file, float px, const char* fallback) -> ImFont* {
        std::string p = fonts + file;
        if (GetFileAttributesA(p.c_str()) == INVALID_FILE_ATTRIBUTES && fallback) p = fonts + fallback;
        ImFontConfig cfg; cfg.OversampleH = 2; cfg.OversampleV = 2;
        ImFont* f = nullptr;
        if (GetFileAttributesA(p.c_str()) != INVALID_FILE_ATTRIBUTES) f = io.Fonts->AddFontFromFileTTF(p.c_str(), px * dpi, &cfg, ranges);
        if (!f) { ImFontConfig c2; c2.SizePixels = px * dpi; f = io.Fonts->AddFontDefault(&c2); }
        return f;
    };
    g_ui.fBody  = load("segoeui.ttf", 15.5f, nullptr);
    g_ui.fSmall = load("segoeui.ttf", 12.5f, nullptr);
    g_ui.fLabel = load("seguisb.ttf", 12.5f, "segoeuib.ttf");
    g_ui.fValue = load("seguisb.ttf", 13.5f, "segoeui.ttf");
    g_ui.fTitle = load("segoeuib.ttf", 22.0f, "segoeui.ttf");
    io.FontDefault = g_ui.fBody;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Processus administrateur : installation / desinstallation
    if (argc >= 2 && wcscmp(argv[1], L"--elevated") == 0) {
        int r = sys::RunElevatedCommand(argc - 2, argv + 2);
        CoUninitialize();
        return r;
    }
    bool startHidden = false;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--tray") == 0) startHidden = true;
        if (wcscmp(argv[i], L"--demo") == 0) g_app.demoDevices = true;
    }

    // instance unique
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\DynamoAudioUI");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND other = FindWindowW(L"DynamoMainWindow", nullptr);
        // une instance tourne deja (peut-etre une ancienne version) : on lui demande de quitter et on prend sa place
        if (other) PostMessageW(other, WM_APP + 3, 0, 0);
        DWORD w = WaitForSingleObject(mutex, 6000);
        if (w != WAIT_OBJECT_0 && w != WAIT_ABANDONED) {
            if (other) PostMessageW(other, WM_SHOW_EXISTING, 0, 0);
            return 0;
        }
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    ImGui_ImplWin32_EnableDpiAwareness();

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0, 0, hInst,
                       LoadIconW(hInst, MAKEINTRESOURCEW(1)), LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"DynamoMainWindow",
                       (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, 0) };
    RegisterClassExW(&wc);

    POINT origin = { 0, 0 };
    HMONITOR mon = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    float dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(mon);
    if (dpi < 1.0f) dpi = 1.0f;
    int ww = (int)(1280 * dpi), wh = (int)(840 * dpi);
    MONITORINFO mi = { sizeof(mi) }; GetMonitorInfoW(mon, &mi);
    int sw = mi.rcWork.right - mi.rcWork.left, shh = mi.rcWork.bottom - mi.rcWork.top;
    if (ww > sw) ww = sw; if (wh > shh) wh = shh;
    g_hwnd = CreateWindowExW(0, wc.lpszClassName, L"Dynamo " TEXT(DYN_VERSION_SHORT), WS_OVERLAPPEDWINDOW,
                             mi.rcWork.left + (sw - ww) / 2, mi.rcWork.top + (shh - wh) / 2, ww, wh, nullptr, nullptr, hInst, nullptr);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(g_hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    COLORREF cap = RGB(17, 17, 19);
    DwmSetWindowAttribute(g_hwnd, 35 /* DWMWA_CAPTION_COLOR */, &cap, sizeof(cap));

    if (!CreateDevice(g_hwnd)) {
        MessageBoxW(nullptr, L"Impossible d'initialiser l'affichage Direct3D 11.", L"Dynamo", MB_ICONERROR);
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    g_ui.dpi = dpi;
    UiApplyStyle(dpi);
    LoadFonts(dpi);
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_dev, g_ctx);

    g_app.Init();
    g_msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    TrayAdd();
    TrayUpdateTip();

    if (!startHidden) { ShowWindow(g_hwnd, SW_SHOWDEFAULT); UpdateWindow(g_hwnd); }
    else g_visible = false;

    LARGE_INTEGER freq, last, now;
    QueryPerformanceFrequency(&freq); QueryPerformanceCounter(&last);
    std::string lastTip;

    while (!g_quit) {
        bool minimized = !g_visible || IsIconic(g_hwnd);
        if (minimized) {
            // fenetre cachee : pas de rendu, on reste reactif aux messages
            MsgWaitForMultipleObjects(0, nullptr, FALSE, 250, QS_ALLINPUT);
        }
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) g_quit = true;
        }
        if (g_quit) break;

        QueryPerformanceCounter(&now);
        double dt = (double)(now.QuadPart - last.QuadPart) / freq.QuadPart;
        last = now;
        if (dt > 0.5) dt = 0.5;
        g_app.Tick(dt);

        std::string tip = g_app.CurrentPresetLabel() + (g_app.params.bypass ? "b" : "");
        if (tip != lastTip) { lastTip = tip; TrayUpdateTip(); }

        if (g_app.updater.state == Updater::READY && !g_visible && !g_restarting) RestartSelf(true);
        if (g_quit) break;

        if (!g_visible || IsIconic(g_hwnd)) continue;

        if (g_resizeW != 0 && g_resizeH != 0) {
            CleanupRT();
            g_swap->ResizeBuffers(0, g_resizeW, g_resizeH, DXGI_FORMAT_UNKNOWN, 0);
            g_resizeW = g_resizeH = 0;
            CreateRT();
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        UiDraw(g_app, g_ui);
        ImGui::Render();
        const float clear[4] = { 17 / 255.f, 17 / 255.f, 19 / 255.f, 1 };
        g_ctx->OMSetRenderTargets(1, &g_rtv, nullptr);
        g_ctx->ClearRenderTargetView(g_rtv, clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        HRESULT hr = g_swap->Present(1, 0);
        if (hr == DXGI_STATUS_OCCLUDED) Sleep(16);

        if (g_ui.requestQuit) g_quit = true;
        if (g_ui.requestRestart) { g_ui.requestRestart = false; RestartSelf(false); }
        if (g_ui.requestHide) { g_ui.requestHide = false; HideMain(); }
    }

    g_app.Shutdown();
    TrayRemove();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDevice();
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, hInst);
    if (mutex) CloseHandle(mutex);
    CoUninitialize();
    return 0;
}
