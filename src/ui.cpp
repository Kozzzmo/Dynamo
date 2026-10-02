// Dynamo - interface (Dear ImGui, dessin personnalise)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <math.h>
#include <stdio.h>
#include <string>
#include <algorithm>

#include "imgui.h"
#include "imgui_internal.h"
#include "ui.h"
#include "dsp.h"
#include "version.h"

// ---------------------------------------------------------------- palette
namespace col {
static const ImU32 bg        = IM_COL32(17, 17, 19, 255);
static const ImU32 panel     = IM_COL32(27, 27, 30, 255);
static const ImU32 panelEdge = IM_COL32(40, 40, 45, 255);
static const ImU32 well      = IM_COL32(11, 11, 12, 255);
static const ImU32 grid      = IM_COL32(255, 255, 255, 16);
static const ImU32 gridStrong= IM_COL32(255, 255, 255, 40);
static const ImU32 text      = IM_COL32(232, 232, 236, 255);
static const ImU32 dim       = IM_COL32(140, 140, 150, 255);
static const ImU32 faint     = IM_COL32(90, 90, 98, 255);
static const ImU32 accent    = IM_COL32(236, 56, 96, 255);
static const ImU32 accentDim = IM_COL32(236, 56, 96, 60);
static const ImU32 comp      = IM_COL32(46, 196, 182, 255);
static const ImU32 compDim   = IM_COL32(46, 196, 182, 50);
static const ImU32 lim       = IM_COL32(255, 176, 32, 255);
static const ImU32 ok        = IM_COL32(64, 200, 110, 255);
static const ImU32 warn      = IM_COL32(255, 176, 32, 255);
static const ImU32 err       = IM_COL32(236, 70, 70, 255);
static const ImU32 knobBg    = IM_COL32(46, 46, 52, 255);
static const ImU32 knobBody  = IM_COL32(34, 34, 38, 255);
static ImU32 band(int i, int a = 255) {
    static const unsigned c[DYN_EQ_BANDS][3] = {
        {255, 99, 99}, {255, 146, 76}, {255, 199, 64}, {196, 222, 70}, {98, 214, 122},
        {60, 200, 190}, {70, 166, 255}, {120, 124, 255}, {182, 106, 255}, {240, 96, 200} };
    return IM_COL32(c[i][0], c[i][1], c[i][2], a);
}
}

static UiContext* U = nullptr;
static float S(float v) { return v * U->dpi; }
static ImVec4 V4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

// ---------------------------------------------------------------- formats
typedef void (*Fmt)(char*, size_t, float);
static void FmtHz(char* b, size_t n, float v) { if (v >= 1000) snprintf(b, n, v >= 10000 ? "%.1f kHz" : "%.2f kHz", v / 1000); else snprintf(b, n, "%.0f Hz", v); }
static void FmtDb(char* b, size_t n, float v) { snprintf(b, n, "%+.1f dB", fabsf(v) < 0.05f ? 0.f : v); }
static void FmtDbPlain(char* b, size_t n, float v) { snprintf(b, n, "%.1f dB", v); }
static void FmtQ(char* b, size_t n, float v) { snprintf(b, n, "%.2f", v); }
static void FmtRatio(char* b, size_t n, float v) { if (v >= 29.5f) snprintf(b, n, "∞:1"); else snprintf(b, n, v < 10 ? "%.1f:1" : "%.0f:1", v); }
static void FmtMs(char* b, size_t n, float v) { if (v >= 1000) snprintf(b, n, "%.2f s", v / 1000); else snprintf(b, n, v < 10 ? "%.1f ms" : "%.0f ms", v); }
static void FmtPct(char* b, size_t n, float v) { snprintf(b, n, "%.0f %%", v * 100); }
static void FmtScHpf(char* b, size_t n, float v) { if (v <= 10.5f) snprintf(b, n, "Off"); else FmtHz(b, n, v); }

// ---------------------------------------------------------------- style
void UiApplyStyle(float dpi) {
    ImGuiStyle& s = ImGui::GetStyle();
    s = ImGuiStyle();
    ImGui::StyleColorsDark(&s);
    s.WindowRounding = 0; s.ChildRounding = 10; s.FrameRounding = 7; s.PopupRounding = 8; s.GrabRounding = 6;
    s.ScrollbarRounding = 6; s.TabRounding = 6;
    s.WindowBorderSize = 0; s.ChildBorderSize = 1; s.PopupBorderSize = 1; s.FrameBorderSize = 0;
    s.WindowPadding = ImVec2(12, 12); s.FramePadding = ImVec2(10, 7); s.ItemSpacing = ImVec2(8, 8);
    s.ItemInnerSpacing = ImVec2(6, 6); s.PopupBorderSize = 1;
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = V4(col::bg);
    c[ImGuiCol_ChildBg] = V4(col::panel);
    c[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.12f, 0.135f, 0.98f);
    c[ImGuiCol_Border] = V4(col::panelEdge);
    c[ImGuiCol_Text] = V4(col::text);
    c[ImGuiCol_TextDisabled] = V4(col::faint);
    c[ImGuiCol_FrameBg] = V4(col::well);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.08f, 0.08f, 0.09f, 1);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.10f, 0.10f, 0.11f, 1);
    c[ImGuiCol_Button] = ImVec4(0.17f, 0.17f, 0.19f, 1);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.23f, 0.23f, 0.26f, 1);
    c[ImGuiCol_ButtonActive] = ImVec4(0.28f, 0.28f, 0.31f, 1);
    c[ImGuiCol_Header] = ImVec4(0.93f, 0.22f, 0.38f, 0.30f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.93f, 0.22f, 0.38f, 0.45f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.93f, 0.22f, 0.38f, 0.60f);
    c[ImGuiCol_CheckMark] = V4(col::accent);
    c[ImGuiCol_SliderGrab] = V4(col::accent);
    c[ImGuiCol_Separator] = V4(col::panelEdge);
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0, 0, 0, 0.55f);
    c[ImGuiCol_NavHighlight] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TitleBg] = ImVec4(0.14f, 0.14f, 0.16f, 1);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.18f, 1);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.14f, 0.14f, 0.16f, 1);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.93f, 0.22f, 0.38f, 0.35f);
    s.ScaleAllSizes(dpi);
}

// ---------------------------------------------------------------- widgets
static void TextAt(ImDrawList* dl, ImFont* f, ImVec2 p, ImU32 c, const char* t) { dl->AddText(f, f->FontSize, p, c, t); }
static ImVec2 TextSize(ImFont* f, const char* t) { return f->CalcTextSizeA(f->FontSize, FLT_MAX, 0, t); }
static void TextCentered(ImDrawList* dl, ImFont* f, ImVec2 center, ImU32 c, const char* t) {
    ImVec2 sz = TextSize(f, t); TextAt(dl, f, ImVec2(floorf(center.x - sz.x / 2), floorf(center.y - sz.y / 2)), c, t);
}

static void Tip(const char* t) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::PushFont(U->fSmall);
        ImGui::SetTooltip("%s", t);
        ImGui::PopFont();
    }
}

enum KnobScale { KNOB_LIN, KNOB_LOG };

static bool Knob(const char* label, float* v, float vmin, float vmax, float vdef, Fmt fmt, KnobScale sc = KNOB_LIN,
                 ImU32 color = col::accent, float width = 70, bool enabled = true, const char* tip = nullptr) {
    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float w = S(width), d = S(40);
    const float labelH = U->fSmall->FontSize + S(5), valueH = U->fValue->FontSize + S(4);
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::PushID(label);
    ImGui::InvisibleButton("##k", ImVec2(w, labelH + d + valueH + S(2)));
    bool hovered = ImGui::IsItemHovered(), active = ImGui::IsItemActive(), changed = false;
    auto toN = [&](float x) { return sc == KNOB_LOG ? logf(x / vmin) / logf(vmax / vmin) : (x - vmin) / (vmax - vmin); };
    auto fromN = [&](float t) { t = t < 0 ? 0 : (t > 1 ? 1 : t); return sc == KNOB_LOG ? vmin * powf(vmax / vmin, t) : vmin + t * (vmax - vmin); };
    if (enabled) {
        float t = toN(*v);
        if (active && io.MouseDelta.y != 0) { t -= io.MouseDelta.y / S(1) * (io.KeyShift ? 0.0012f : 0.005f); *v = fromN(t); changed = true; }
        if (hovered && io.MouseWheel != 0) { t += io.MouseWheel * (io.KeyShift ? 0.004f : 0.025f); *v = fromN(t); changed = true; }
        if (hovered && ImGui::IsMouseDoubleClicked(0)) { *v = vdef; changed = true; }
        if (hovered && ImGui::IsMouseClicked(1)) ImGui::OpenPopup("##edit");
        if (hovered || active) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    }
    if (ImGui::BeginPopup("##edit")) {
        ImGui::PushFont(U->fSmall);
        ImGui::TextDisabled("%s", label);
        ImGui::PopFont();
        float tmp = *v;
        ImGui::SetNextItemWidth(S(110));
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        if (ImGui::InputFloat("##val", &tmp, 0, 0, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue)) {
            *v = std::max(vmin, std::min(vmax, tmp)); changed = true; ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    if (tip && hovered && !active) Tip(tip);

    const ImU32 lc = enabled ? col::dim : col::faint;
    TextCentered(dl, U->fSmall, ImVec2(p.x + w / 2, p.y + labelH / 2), lc, label);
    ImVec2 c(p.x + w / 2, p.y + labelH + d / 2);
    const float r = d / 2, a0 = 3.14159f * 0.75f, a1 = 3.14159f * 2.25f;
    float t = toN(*v); t = t < 0 ? 0 : (t > 1 ? 1 : t);
    dl->PathArcTo(c, r - S(2), a0, a1, 40);
    dl->PathStroke(col::knobBg, ImDrawFlags_None, S(4));
    float tz = (vmin < 0 && vmax > 0 && sc == KNOB_LIN) ? toN(0) : 0.f;
    float ta = std::min(t, tz), tb = std::max(t, tz);
    if (tb - ta > 0.001f) {
        dl->PathArcTo(c, r - S(2), a0 + (a1 - a0) * ta, a0 + (a1 - a0) * tb, 32);
        dl->PathStroke(enabled ? color : col::faint, ImDrawFlags_None, S(4));
    }
    dl->AddCircleFilled(c, r - S(7), (hovered || active) && enabled ? IM_COL32(44, 44, 50, 255) : col::knobBody, 32);
    float ang = a0 + (a1 - a0) * t;
    ImVec2 dir(cosf(ang), sinf(ang));
    dl->AddLine(ImVec2(c.x + dir.x * (r - S(15)), c.y + dir.y * (r - S(15))), ImVec2(c.x + dir.x * (r - S(8)), c.y + dir.y * (r - S(8))),
                enabled ? col::text : col::faint, S(2.5f));
    char buf[32]; fmt(buf, sizeof(buf), *v);
    TextCentered(dl, U->fValue, ImVec2(p.x + w / 2, p.y + labelH + d + valueH / 2 + S(1)), enabled ? col::text : col::faint, buf);
    ImGui::PopID();
    return changed;
}

static bool Toggle(const char* id, int* v, ImU32 onColor = col::accent, const char* tip = nullptr) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = S(18), w = S(32);
    ImGui::PushID(id);
    bool clicked = ImGui::InvisibleButton("##t", ImVec2(w, h));
    if (clicked) *v = !*v;
    bool hov = ImGui::IsItemHovered();
    if (tip) Tip(tip);
    ImGui::PopID();
    ImU32 bgc = *v ? onColor : (hov ? IM_COL32(70, 70, 78, 255) : IM_COL32(56, 56, 62, 255));
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), bgc, h / 2);
    float kx = *v ? p.x + w - h / 2 : p.x + h / 2;
    dl->AddCircleFilled(ImVec2(kx, p.y + h / 2), h / 2 - S(3), IM_COL32(245, 245, 248, 255), 20);
    return clicked;
}

static bool Segmented(const char* id, int* v, const char* const* items, int n, float itemW, ImU32 onColor = col::accent) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float h = S(24), w = S(itemW);
    bool changed = false;
    dl->AddRectFilled(p, ImVec2(p.x + w * n, p.y + h), col::well, S(6));
    ImGui::PushID(id);
    for (int i = 0; i < n; ++i) {
        ImGui::SetCursorScreenPos(ImVec2(p.x + w * i, p.y));
        ImGui::PushID(i);
        if (ImGui::InvisibleButton("##s", ImVec2(w, h)) && *v != i) { *v = i; changed = true; }
        bool hov = ImGui::IsItemHovered();
        ImGui::PopID();
        ImVec2 a(p.x + w * i + S(2), p.y + S(2)), b(p.x + w * (i + 1) - S(2), p.y + h - S(2));
        if (*v == i) dl->AddRectFilled(a, b, (onColor & 0x00FFFFFF) | 0x50000000, S(5));
        else if (hov) dl->AddRectFilled(a, b, IM_COL32(255, 255, 255, 12), S(5));
        TextCentered(dl, U->fSmall, ImVec2((a.x + b.x) / 2, (a.y + b.y) / 2), *v == i ? col::text : col::dim, items[i]);
    }
    ImGui::PopID();
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy(ImVec2(w * n, h));
    return changed;
}

static void SectionTitle(const char* t, ImU32 c = col::dim) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    TextAt(dl, U->fLabel, ImVec2(p.x, p.y + S(1)), c, t);
    ImVec2 sz = TextSize(U->fLabel, t);
    ImGui::Dummy(ImVec2(sz.x, S(18)));
}

// vumetre vertical (dB)
static void Meter(ImVec2 p, ImVec2 size, float db, float holdDb) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), col::well, S(3));
    auto y = [&](float v) { float t = (v + 60.f) / 66.f; t = t < 0 ? 0 : (t > 1 ? 1 : t); return p.y + size.y - t * size.y; };
    float yl = y(db);
    if (db > -60) {
        float y18 = y(-18), y6 = y(-6), y0 = y(0);
        ImVec2 a(p.x + S(1), 0), b(p.x + size.x - S(1), 0);
        // vert
        dl->AddRectFilled(ImVec2(a.x, std::max(yl, y18)), ImVec2(b.x, p.y + size.y - S(1)), IM_COL32(64, 200, 110, 255), S(2));
        if (yl < y18) dl->AddRectFilled(ImVec2(a.x, std::max(yl, y6)), ImVec2(b.x, y18), IM_COL32(210, 210, 70, 255));
        if (yl < y6)  dl->AddRectFilled(ImVec2(a.x, std::max(yl, y0)), ImVec2(b.x, y6), IM_COL32(255, 150, 40, 255));
        if (yl < y0)  dl->AddRectFilled(ImVec2(a.x, yl), ImVec2(b.x, y0), IM_COL32(236, 56, 70, 255));
    }
    if (holdDb > -60) { float yh = y(holdDb); dl->AddLine(ImVec2(p.x, yh), ImVec2(p.x + size.x, yh), holdDb > 0 ? col::err : col::text, S(1.5f)); }
}

// reduction de gain (descend depuis le haut)
static void GrMeter(ImVec2 p, ImVec2 size, float grDb, ImU32 c, float range = 24.f) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), col::well, S(3));
    float t = grDb / range; t = t < 0 ? 0 : (t > 1 ? 1 : t);
    if (t > 0.002f) dl->AddRectFilled(ImVec2(p.x + S(1), p.y + S(1)), ImVec2(p.x + size.x - S(1), p.y + S(1) + t * (size.y - S(2))), c, S(2));
}

static void MeterScale(ImVec2 p, float h, bool rightAlign) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const int marks[] = { 6, 0, -6, -12, -18, -24, -36, -48, -60 };
    float lastY = -1e9f;
    for (int m : marks) {
        float t = (m + 60.f) / 66.f; float y = p.y + h - t * h;
        if (y - lastY < U->fSmall->FontSize + S(1)) continue;
        lastY = y;
        char b[8]; snprintf(b, sizeof(b), "%d", m);
        ImVec2 sz = TextSize(U->fSmall, b);
        TextAt(dl, U->fSmall, ImVec2(rightAlign ? p.x - sz.x : p.x, y - sz.y / 2), col::faint, b);
    }
}

static bool IconButton(const char* id, ImVec2 size, bool on, void (*draw)(ImDrawList*, ImVec2, float, ImU32), const char* tip) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(id, size);
    bool hov = ImGui::IsItemHovered();
    if (tip) Tip(tip);
    ImVec2 c(p.x + size.x / 2, p.y + size.y / 2);
    if (hov) dl->AddCircleFilled(c, size.x / 2, IM_COL32(255, 255, 255, 18), 32);
    draw(dl, c, size.x * 0.5f, on ? col::accent : (hov ? col::text : col::dim));
    return clicked;
}

static void DrawPower(ImDrawList* dl, ImVec2 c, float r, ImU32 cl) {
    float rr = r * 0.52f;
    dl->PathArcTo(c, rr, -3.14159f * 0.5f + 0.75f, 3.14159f * 1.5f - 0.75f, 32);
    dl->PathStroke(cl, ImDrawFlags_None, S(2.4f));
    dl->AddLine(ImVec2(c.x, c.y - rr - S(2)), ImVec2(c.x, c.y - S(1)), cl, S(2.4f));
}
static void DrawGear(ImDrawList* dl, ImVec2 c, float r, ImU32 cl) {
    float ro = r * 0.55f, ri = r * 0.40f;
    for (int i = 0; i < 8; ++i) {
        float a = i * 3.14159f / 4;
        ImVec2 d(cosf(a), sinf(a));
        dl->AddLine(ImVec2(c.x + d.x * ri, c.y + d.y * ri), ImVec2(c.x + d.x * ro, c.y + d.y * ro), cl, S(3.2f));
    }
    dl->AddCircle(c, ri, cl, 24, S(2.2f));
    dl->AddCircleFilled(c, r * 0.14f, cl, 12);
}
static void DrawLogo(ImDrawList* dl, ImVec2 p, float h) {
    const float bw = h * 0.16f, gap = h * 0.08f;
    const float hs[4] = { 0.45f, 0.8f, 1.0f, 0.62f };
    for (int i = 0; i < 4; ++i) {
        float x = p.x + i * (bw + gap);
        dl->AddRectFilled(ImVec2(x, p.y + h * (1 - hs[i])), ImVec2(x + bw, p.y + h), col::accent, bw * 0.3f);
    }
}

// ---------------------------------------------------------------- panneau
static bool BeginPanel(const char* id, ImVec2 pos, ImVec2 size) {
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(S(14), S(12)));
    bool r = ImGui::BeginChild(id, size, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();
    return r;
}
static void EndPanel() { ImGui::EndChild(); }

// ---------------------------------------------------------------- egaliseur
static const char* kTypeNames[EQ_TYPE_COUNT] = { "Cloche", "Plateau grave", "Plateau aigu", "Coupe-bas", "Coupe-haut", "Réjection" };
static const char* kSlopeNames[4] = { "12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct" };
static bool HasGain(int type) { return type == EQ_PEAK || type == EQ_LOWSHELF || type == EQ_HIGHSHELF; }

static void ResetBand(DynParams& p, int i) {
    DynParams d; DynDefaultParams(d);
    p.eq[i] = d.eq[i];
}

static int g_dragBand = -1;

static bool EqGraph(App& app, ImVec2 p, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGuiIO& io = ImGui::GetIO();
    DynParams& P = app.params;
    const double fs = app.sampleRate() ? app.sampleRate() : 48000.0;
    const float fmin = 20.f, fmax = 20000.f, gRange = 18.f;
    const float padL = S(34), padB = S(20), padT = S(8), padR = S(8);
    ImVec2 g0(p.x + padL, p.y + padT), g1(p.x + size.x - padR, p.y + size.y - padB);
    const float gw = g1.x - g0.x, gh = g1.y - g0.y;
    auto X = [&](float f) { return g0.x + logf(f / fmin) / logf(fmax / fmin) * gw; };
    auto F = [&](float x) { float t = (x - g0.x) / gw; return fmin * powf(fmax / fmin, t); };
    auto Y = [&](float db) { float t = db / gRange; t = t < -1.1f ? -1.1f : (t > 1.1f ? 1.1f : t); return (g0.y + g1.y) / 2 - t * gh / 2; };
    auto G = [&](float y) { return -((y - (g0.y + g1.y) / 2) / (gh / 2)) * gRange; };
    bool changed = false;

    dl->AddRectFilled(g0, g1, col::well, S(6));
    dl->PushClipRect(g0, g1, true);
    const ImDrawListFlags savedFlags = dl->Flags;
    // grille
    const float gf[] = { 30, 40, 50, 60, 70, 80, 90, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000 };
    for (float f : gf) {
        bool major = (f == 100 || f == 1000 || f == 10000);
        dl->AddLine(ImVec2(X(f), g0.y), ImVec2(X(f), g1.y), major ? col::gridStrong : col::grid, 1);
    }
    for (int db = -18; db <= 18; db += 6) dl->AddLine(ImVec2(g0.x, Y((float)db)), ImVec2(g1.x, Y((float)db)), db == 0 ? col::gridStrong : col::grid, db == 0 ? S(1.2f) : 1);

    // analyseur de spectre (sortie)
    if (app.settings.showSpectrum && app.spectrumValid) {
        const int n = App::kSpecPoints;
        for (int i = 0; i < n - 1; ++i) {
            float f0 = 20.f * powf(1000.f, (float)i / n), f1 = 20.f * powf(1000.f, (float)(i + 1) / n);
            auto sy = [&](float db) { float t = (db + 96.f) / 90.f; t = t < 0 ? 0 : (t > 1 ? 1 : t); return g1.y - t * gh; };
            ImVec2 a(X(sqrtf(f0 * powf(1000.f, 1.f / n) * f0)), sy(app.spectrum[i]));
            ImVec2 b(X(sqrtf(f1 * powf(1000.f, 1.f / n) * f1)), sy(app.spectrum[i + 1]));
            ImVec2 q[4] = { ImVec2(a.x, g1.y), a, b, ImVec2(b.x, g1.y) };
            dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
            dl->AddConvexPolyFilled(q, 4, IM_COL32(120, 160, 255, 26));
            dl->Flags = savedFlags;
            dl->AddLine(a, b, IM_COL32(140, 175, 255, 70), S(1));
        }
    }

    // coefficients de chaque bande pour les courbes
    dyn::Coef coefs[DYN_EQ_BANDS][4]; int ncoef[DYN_EQ_BANDS];
    for (int i = 0; i < DYN_EQ_BANDS; ++i) ncoef[i] = dyn::designBand(P.eq[i], fs, coefs[i]);
    auto bandDb = [&](int i, float f) { double r = 0; for (int k = 0; k < ncoef[i]; ++k) r += dyn::magDb(coefs[i][k], f, fs); return (float)r; };

    const int npts = std::max(64, (int)(gw / S(2.5f)));
    static std::vector<ImVec2> curve; curve.resize(npts);
    // bande selectionnee : sa contribution propre
    int sel = app.settings.selectedBand;
    if (P.eqEnabled && P.eq[sel].enabled && ncoef[sel] > 0) {
        for (int k = 0; k < npts; ++k) { float x = g0.x + gw * k / (npts - 1); curve[k] = ImVec2(x, Y(bandDb(sel, F(x)))); }
        float z = Y(0);
        dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
        for (int k = 0; k < npts - 1; ++k) {
            ImVec2 q[4] = { ImVec2(curve[k].x, z), curve[k], curve[k + 1], ImVec2(curve[k + 1].x, z) };
            if ((curve[k].y - z) * (curve[k + 1].y - z) >= 0) dl->AddConvexPolyFilled(q, 4, col::band(sel, 40));
        }
        dl->Flags = savedFlags;
        dl->AddPolyline(curve.data(), npts, col::band(sel, 150), ImDrawFlags_None, S(1.5f));
    }
    // courbe totale
    for (int k = 0; k < npts; ++k) {
        float x = g0.x + gw * k / (npts - 1), f = F(x); float db = 0;
        if (P.eqEnabled) for (int i = 0; i < DYN_EQ_BANDS; ++i) db += bandDb(i, f);
        curve[k] = ImVec2(x, Y(db));
    }
    {
        float z = Y(0);
        dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
        for (int k = 0; k < npts - 1; ++k) {
            ImVec2 a = curve[k], b = curve[k + 1];
            if ((a.y - z) * (b.y - z) >= 0) { ImVec2 q[4] = { ImVec2(a.x, z), a, b, ImVec2(b.x, z) }; dl->AddConvexPolyFilled(q, 4, IM_COL32(255, 255, 255, 22)); }
        }
        dl->Flags = savedFlags;
    }
    dl->AddPolyline(curve.data(), npts, P.eqEnabled ? IM_COL32(245, 245, 250, 255) : col::faint, ImDrawFlags_None, S(2.2f));

    // noeuds
    ImVec2 nodePos[DYN_EQ_BANDS];
    for (int i = 0; i < DYN_EQ_BANDS; ++i) {
        const DynEqBand& b = P.eq[i];
        float y = HasGain(b.type) ? Y(b.gain) : Y(P.eqEnabled ? bandDb(i, b.freq) * 0 : 0);
        nodePos[i] = ImVec2(X(b.freq), y);
    }
    dl->PopClipRect();

    // axes
    const float lf[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    const char* ll[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };
    for (int i = 0; i < 8; ++i) TextCentered(dl, U->fSmall, ImVec2(X(lf[i]), g1.y + padB / 2 + S(1)), col::faint, ll[i]);
    for (int db = -18; db <= 18; db += 6) {
        char b[8]; snprintf(b, sizeof(b), db > 0 ? "+%d" : "%d", db);
        ImVec2 sz = TextSize(U->fSmall, b);
        TextAt(dl, U->fSmall, ImVec2(g0.x - sz.x - S(6), Y((float)db) - sz.y / 2), col::faint, b);
    }

    // interaction
    ImGui::SetCursorScreenPos(g0);
    ImGui::InvisibleButton("##eqgraph", ImVec2(gw, gh));
    bool hov = ImGui::IsItemHovered();
    int hoverBand = -1;
    if (hov || ImGui::IsItemActive()) {
        float best = S(14) * S(14);
        for (int i = 0; i < DYN_EQ_BANDS; ++i) {
            float dx = io.MousePos.x - nodePos[i].x, dy = io.MousePos.y - nodePos[i].y;
            if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; hoverBand = i; }
        }
    }
    if (ImGui::IsItemClicked(0)) {
        g_dragBand = hoverBand;
        if (hoverBand >= 0) app.settings.selectedBand = hoverBand;
    }
    if (ImGui::IsItemActive() && g_dragBand >= 0 && (io.MouseDelta.x != 0 || io.MouseDelta.y != 0)) {
        DynEqBand& b = P.eq[g_dragBand];
        float mx = std::max(g0.x, std::min(g1.x, io.MousePos.x));
        b.freq = std::max(20.f, std::min(20000.f, F(mx)));
        if (HasGain(b.type)) b.gain = std::max(-24.f, std::min(24.f, roundf(G(io.MousePos.y) * 10) / 10));
        if (!b.enabled) b.enabled = 1;
        changed = true;
    }
    if (!ImGui::IsItemActive()) g_dragBand = -1;
    if (hov && hoverBand >= 0 && io.MouseWheel != 0) {
        DynEqBand& b = P.eq[hoverBand];
        b.q = std::max(0.1f, std::min(18.f, b.q * powf(1.15f, io.MouseWheel)));
        app.settings.selectedBand = hoverBand;
        changed = true;
    }
    if (hov && hoverBand >= 0 && ImGui::IsMouseDoubleClicked(0)) { P.eq[hoverBand].enabled = !P.eq[hoverBand].enabled; changed = true; }
    if (hov && hoverBand >= 0 && ImGui::IsMouseClicked(1)) { app.settings.selectedBand = hoverBand; ImGui::OpenPopup("##bandmenu"); }
    if (hoverBand >= 0 || g_dragBand >= 0) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    // dessin des noeuds (au-dessus de tout)
    for (int pass = 0; pass < 2; ++pass)
    for (int i = 0; i < DYN_EQ_BANDS; ++i) {
        bool isSel = (i == app.settings.selectedBand);
        if ((pass == 1) != isSel) continue;
        const DynEqBand& b = P.eq[i];
        ImVec2 c = nodePos[i];
        c.y = std::max(g0.y + S(8), std::min(g1.y - S(8), c.y));
        float r = S(isSel ? 9.5f : 8.f);
        if (i == hoverBand || i == g_dragBand) r += S(1.5f);
        if (isSel) dl->AddCircle(c, r + S(3.5f), col::band(i, 120), 32, S(1.5f));
        if (b.enabled && P.eqEnabled) {
            dl->AddCircleFilled(c, r, col::band(i), 32);
            char n[4]; snprintf(n, sizeof(n), "%d", i + 1);
            TextCentered(dl, U->fSmall, c, IM_COL32(20, 20, 22, 255), n);
        } else {
            dl->AddCircleFilled(c, r, col::well, 32);
            dl->AddCircle(c, r, col::band(i, 140), 32, S(1.5f));
            char n[4]; snprintf(n, sizeof(n), "%d", i + 1);
            TextCentered(dl, U->fSmall, c, col::band(i, 180), n);
        }
    }
    if (hoverBand >= 0 && g_dragBand < 0 && !ImGui::IsPopupOpen("##bandmenu")) {
        const DynEqBand& b = P.eq[hoverBand];
        char fz[24], gz[24]; FmtHz(fz, sizeof(fz), b.freq); FmtDb(gz, sizeof(gz), b.gain);
        ImGui::PushFont(U->fSmall);
        if (HasGain(b.type)) ImGui::SetTooltip("Bande %d · %s\n%s   %s   Q %.2f\nMolette : largeur (Q) · Double-clic : activer/désactiver\nClic droit : type de filtre", hoverBand + 1, kTypeNames[b.type], fz, gz, b.q);
        else ImGui::SetTooltip("Bande %d · %s\n%s   %s\nDouble-clic : activer/désactiver · Clic droit : type", hoverBand + 1, kTypeNames[b.type], fz, (b.type == EQ_LOWCUT || b.type == EQ_HIGHCUT) ? kSlopeNames[b.slope - 1] : "");
        ImGui::PopFont();
    }
    if (g_dragBand >= 0) {
        const DynEqBand& b = P.eq[g_dragBand];
        char fz[24], gz[24], t[64]; FmtHz(fz, sizeof(fz), b.freq); FmtDb(gz, sizeof(gz), b.gain);
        snprintf(t, sizeof(t), HasGain(b.type) ? "%s  %s" : "%s", fz, gz);
        ImVec2 c = nodePos[g_dragBand];
        ImVec2 sz = TextSize(U->fSmall, t);
        ImVec2 bp(std::min(c.x + S(14), g1.x - sz.x - S(12)), std::max(g0.y + S(4), c.y - S(30)));
        dl->AddRectFilled(bp, ImVec2(bp.x + sz.x + S(12), bp.y + sz.y + S(8)), IM_COL32(0, 0, 0, 200), S(4));
        TextAt(dl, U->fSmall, ImVec2(bp.x + S(6), bp.y + S(4)), col::text, t);
    }

    // menu contextuel d'une bande
    if (ImGui::BeginPopup("##bandmenu")) {
        int i = app.settings.selectedBand;
        DynEqBand& b = P.eq[i];
        ImGui::PushFont(U->fSmall); ImGui::TextDisabled("BANDE %d", i + 1); ImGui::PopFont();
        for (int t = 0; t < EQ_TYPE_COUNT; ++t) if (ImGui::MenuItem(kTypeNames[t], nullptr, b.type == t)) { b.type = t; b.enabled = 1; changed = true; }
        ImGui::Separator();
        bool en = b.enabled != 0;
        if (ImGui::MenuItem("Active", nullptr, &en)) { b.enabled = en; changed = true; }
        if (ImGui::MenuItem("Réinitialiser la bande")) { ResetBand(P, i); changed = true; }
        ImGui::EndPopup();
    }
    return changed;
}

// ---------------------------------------------------------------- courbe de transfert du compresseur
static void TransferGraph(App& app, ImVec2 p, float sz, int band) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const DynCompBand& c = app.params.comp[band];
    ImVec2 a = p, b(p.x + sz, p.y + sz);
    dl->AddRectFilled(a, b, col::well, S(6));
    auto X = [&](float db) { return a.x + (db + 60.f) / 60.f * sz; };
    auto Y = [&](float db) { float t = (db + 60.f) / 60.f; t = t < 0 ? 0 : (t > 1 ? 1 : t); return b.y - t * sz; };
    for (int d = -48; d <= -12; d += 12) {
        dl->AddLine(ImVec2(X((float)d), a.y), ImVec2(X((float)d), b.y), col::grid);
        dl->AddLine(ImVec2(a.x, Y((float)d)), ImVec2(b.x, Y((float)d)), col::grid);
    }
    dl->AddLine(ImVec2(a.x, b.y), ImVec2(b.x, a.y), IM_COL32(255, 255, 255, 30), S(1));
    bool on = app.params.compEnabled && c.enabled;
    ImU32 cc = on ? col::comp : col::faint;
    dl->AddLine(ImVec2(X(c.threshold), a.y), ImVec2(X(c.threshold), b.y), (cc & 0x00FFFFFF) | 0x50000000, S(1));
    ImVec2 pts[121];
    for (int i = 0; i <= 120; ++i) {
        float in = -60.f + i * 0.5f;
        float out = on ? (float)dyn::gainComputer(in, c.threshold, c.ratio, c.knee) : in;
        pts[i] = ImVec2(X(in), Y(out));
    }
    dl->PushClipRect(a, b, true);
    dl->AddPolyline(pts, 121, cc, ImDrawFlags_None, S(2.2f));
    if (on && !app.params.compMultiband && app.Status() == ENGINE_ACTIVE && app.compLevel > -60) {
        float in = app.compLevel, out = (float)dyn::gainComputer(in, c.threshold, c.ratio, c.knee);
        dl->AddCircleFilled(ImVec2(X(in), Y(out)), S(5), col::text, 16);
        dl->AddCircle(ImVec2(X(in), Y(out)), S(8), (col::comp & 0x00FFFFFF) | 0x80000000, 16, S(1.5f));
    }
    dl->PopClipRect();
    TextAt(dl, U->fSmall, ImVec2(a.x + S(6), a.y + S(4)), col::faint, "sortie");
    ImVec2 ts = TextSize(U->fSmall, "entrée");
    TextAt(dl, U->fSmall, ImVec2(b.x - ts.x - S(6), b.y - ts.y - S(4)), col::faint, "entrée");
}

static void GrHistory(App& app, ImVec2 p, ImVec2 size) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), col::well, S(6));
    const int n = (int)app.grHistory.size();
    const float range = 18.f;
    for (int k = 1; k < n; ++k) {
        int i0 = (app.grHistoryPos + k - 1) % n, i1 = (app.grHistoryPos + k) % n;
        float x0 = p.x + size.x * (k - 1) / (n - 1), x1 = p.x + size.x * k / (n - 1);
        float y0 = p.y + std::min(1.f, app.grHistory[i0] / range) * (size.y - S(2));
        float y1 = p.y + std::min(1.f, app.grHistory[i1] / range) * (size.y - S(2));
        if (y0 > p.y + 0.5f || y1 > p.y + 0.5f) {
            ImVec2 q[4] = { ImVec2(x0, p.y), ImVec2(x0, y0), ImVec2(x1, y1), ImVec2(x1, p.y) };
            ImDrawListFlags fl = dl->Flags; dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;
            dl->AddConvexPolyFilled(q, 4, col::compDim);
            dl->Flags = fl;
            dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), col::comp, S(1.3f));
        }
    }
    for (int d = 6; d < range; d += 6) {
        float y = p.y + d / range * size.y;
        dl->AddLine(ImVec2(p.x, y), ImVec2(p.x + size.x, y), col::grid);
        char b[8]; snprintf(b, sizeof(b), "-%d", d);
        TextAt(dl, U->fSmall, ImVec2(p.x + S(4), y - U->fSmall->FontSize - S(1)), col::faint, b);
    }
}

// ---------------------------------------------------------------- barre du haut
static char g_saveName[64] = "";
static bool g_openSaveAs = false, g_openUninstall = false, g_openAbout = false, g_openDiag = false;

static void TopBar(App& app, ImVec2 p, float w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = S(64);
    // logo
    DrawLogo(dl, ImVec2(p.x + S(18), p.y + S(20)), S(24));
    TextAt(dl, U->fTitle, ImVec2(p.x + S(56), p.y + S(17)), col::accent, "Dynamo");
    TextAt(dl, U->fSmall, ImVec2(p.x + S(58), p.y + S(44)), col::faint, "version " DYN_VERSION_SHORT);

    // presets
    float x = p.x + S(190);
    const float comboH = S(38);
    float presetW = std::max(S(220), w * 0.26f);
    ImGui::SetCursorScreenPos(ImVec2(x, p.y + (h - comboH) / 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(14), (comboH - ImGui::GetFontSize()) / 2));
    ImGui::SetNextItemWidth(presetW);
    std::string label = app.CurrentPresetLabel();
    if (ImGui::BeginCombo("##preset", label.c_str(), ImGuiComboFlags_HeightLarge)) {
        ImGui::PushFont(U->fSmall); ImGui::TextDisabled("PRESETS INTÉGRÉS"); ImGui::PopFont();
        bool userHeader = false;
        for (int i = 0; i < (int)app.presets.size(); ++i) {
            if (!app.presets[i].builtin && !userHeader) {
                userHeader = true; ImGui::Separator();
                ImGui::PushFont(U->fSmall); ImGui::TextDisabled("MES PRESETS"); ImGui::PopFont();
            }
            if (ImGui::Selectable(app.presets[i].name.c_str(), i == app.currentPreset)) app.LoadPreset(i);
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar();
    x += presetW + S(8);

    auto smallBtn = [&](const char* t, bool enabled, const char* tip) {
        ImGui::SetCursorScreenPos(ImVec2(x, p.y + (h - S(32)) / 2));
        ImGui::BeginDisabled(!enabled);
        bool r = ImGui::Button(t, ImVec2(0, S(32)));
        ImGui::EndDisabled();
        if (tip) Tip(tip);
        x = ImGui::GetItemRectMax().x + S(6);
        return r;
    };
    bool isUser = app.currentPreset >= 0 && !app.presets[app.currentPreset].builtin;
    if (smallBtn("Enregistrer", app.modified && app.currentPreset >= 0, "Enregistrer les modifications dans ce preset (Ctrl+S)")) app.SaveCurrentPreset();
    if (smallBtn("Enregistrer sous…", true, "Créer un nouveau preset à partir des réglages actuels")) {
        snprintf(g_saveName, sizeof(g_saveName), "%s", app.currentPreset >= 0 ? app.presets[app.currentPreset].name.c_str() : "Mon preset");
        g_openSaveAs = true;
    }
    if (smallBtn("Supprimer", isUser, "Supprimer ce preset personnel")) {
        app.DeletePreset(app.currentPreset);
    }

    // sortie audio
    const float right = p.x + w - S(16);
    const float iconW = S(40);
    float devW = std::max(S(240), std::min(S(380), right - iconW * 2 - S(24) - x - S(16)));
    float dx = right - iconW * 2 - S(16) - devW;
    ImGui::SetCursorScreenPos(ImVec2(dx, p.y + (h - comboH) / 2));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(34), (comboH - ImGui::GetFontSize()) / 2));
    ImGui::SetNextItemWidth(devW);
    std::string devLabel = app.devices.empty() ? "Aucune sortie audio" : sys::ToUtf8(app.devices[app.selectedDevice].name);
    if (app.RoutingActive()) devLabel = app.DeviceName(app.settings.routeTarget);
    ImVec2 comboPos = ImGui::GetCursorScreenPos();
    if (ImGui::BeginCombo("##device", devLabel.c_str(), ImGuiComboFlags_HeightLarge)) {
        ImGui::PushFont(U->fSmall); ImGui::TextDisabled(app.RoutingActive() ? "DYNAMO ENVOIE LE SON TRAITÉ VERS" : "SORTIE AUDIO"); ImGui::PopFont();
        for (int i = 0; i < (int)app.devices.size(); ++i) {
            const auto& d = app.devices[i];
            std::string id = sys::ToUtf8(d.id);
            if (app.RoutingActive() && id == app.settings.routeSource) continue;
            std::string n = sys::ToUtf8(d.name);
            if (!app.RoutingActive() && d.isDefault) n += "   · par défaut";
            bool sel = app.RoutingActive() ? id == app.settings.routeTarget : i == app.selectedDevice;
            if (ImGui::Selectable(n.c_str(), sel)) {
                app.selectedDevice = i;
                if (app.RoutingActive() && id != app.settings.routeTarget) {
                    std::wstring src = sys::FromUtf8(app.settings.routeSource);
                    app.ConfigureRouting(d.id, src == d.id ? L"" : src);
                }
            }
        }
        ImGui::Separator();
        if (ImGui::Selectable("Actualiser la liste")) app.RefreshDevices();
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar();
    // pastille d'etat dans la liste
    {
        ImU32 c = col::faint;
        if (app.RoutingActive() || (!app.devices.empty() && app.devices[app.selectedDevice].installed)) c = app.Status() == ENGINE_ACTIVE ? col::ok : col::warn;
        dl->AddCircleFilled(ImVec2(comboPos.x + S(16), comboPos.y + comboH / 2), S(5), c, 16);
    }

    // contournement
    ImGui::SetCursorScreenPos(ImVec2(right - iconW * 2 - S(8), p.y + (h - iconW) / 2));
    bool on = !app.params.bypass;
    if (IconButton("##power", ImVec2(iconW, iconW), on, DrawPower, on ? "Traitement actif — cliquer pour contourner (son d'origine)" : "Contourné — cliquer pour réactiver")) {
        app.params.bypass = !app.params.bypass;
        app.ParamsChanged();
        app.modified = app.modified; // le contournement ne compte pas comme une modification du preset
    }
    ImGui::SetCursorScreenPos(ImVec2(right - iconW, p.y + (h - iconW) / 2));
    if (IconButton("##gear", ImVec2(iconW, iconW), false, DrawGear, "Réglages")) ImGui::OpenPopup("##settings");

    if (ImGui::BeginPopup("##settings")) {
        ImGui::PushFont(U->fSmall); ImGui::TextDisabled("GÉNÉRAL"); ImGui::PopFont();
        if (ImGui::Checkbox("Lancer Dynamo au démarrage de Windows", &app.settings.startWithWindows)) { sys::SetAutostart(app.settings.startWithWindows); app.SaveSettings(); }
        if (ImGui::Checkbox("Fermer la fenêtre = réduire près de l'horloge", &app.settings.closeToTray)) app.SaveSettings();
        if (ImGui::Checkbox("Afficher l'analyseur de spectre", &app.settings.showSpectrum)) app.SaveSettings();
        if (ImGui::Checkbox("Mises à jour automatiques", &app.settings.autoUpdate)) {
            app.SaveSettings();
            if (app.settings.autoUpdate) app.updater.Start(1); else app.updater.Stop();
        }
        if (ImGui::Checkbox("Définir comme sortie Windows par défaut à l'installation", &app.settings.setAsDefault)) app.SaveSettings();
        ImGui::Separator();
        ImGui::PushFont(U->fSmall); ImGui::TextDisabled("ROUTAGE"); ImGui::PopFont();
        if (!app.RoutingActive()) {
            if (ImGui::MenuItem("Activer le routage (recommandé)") && !app.devices.empty()) app.ConfigureRouting(app.devices[app.selectedDevice].id);
        } else {
            if (ImGui::BeginMenu("Sortie capturée par Dynamo")) {
                for (auto& d : app.devices) {
                    std::string id = sys::ToUtf8(d.id);
                    if (id == app.settings.routeTarget) continue;
                    if (ImGui::MenuItem(sys::ToUtf8(d.name).c_str(), nullptr, id == app.settings.routeSource))
                        app.ConfigureRouting(sys::FromUtf8(app.settings.routeTarget), d.id);
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Rediriger Windows vers Dynamo")) app.StartRouting();
            if (ImGui::MenuItem("Désactiver le routage")) app.DisableRouting();
        }
        ImGui::Separator();
        ImGui::PushFont(U->fSmall); ImGui::TextDisabled("MODE AVANCÉ : MOTEUR DANS WINDOWS (APO)"); ImGui::PopFont();
        bool inst = !app.devices.empty() && app.devices[app.selectedDevice].installed;
        if (ImGui::MenuItem(inst ? "Réinstaller le moteur (standard)" : "Installer le moteur (standard)")) app.InstallOnSelected(sys::MODE_EFX);
        if (ImGui::MenuItem("Installer en mode compatibilité")) app.InstallOnSelected(sys::MODE_MFX);
        if (ImGui::IsItemHovered()) { ImGui::PushFont(U->fSmall); ImGui::SetTooltip("À utiliser si le mode standard ne traite pas le son\n(certains pilotes n'acceptent pas les effets de sortie)."); ImGui::PopFont(); }
        if (ImGui::MenuItem("Retirer le moteur de ce périphérique", nullptr, false, inst)) app.UninstallFromSelected();
        if (ImGui::MenuItem("Définir comme sortie Windows par défaut", nullptr, false, !app.devices.empty() && !app.devices[app.selectedDevice].isDefault)) app.MakeSelectedDefault();
        if (ImGui::MenuItem("Redémarrer le service audio de Windows")) app.RestartAudio();
        if (ImGui::MenuItem("Diagnostic et réparation…")) { app.RunDiagnostic(); g_openDiag = true; }
        ImGui::Separator();
        if (ImGui::MenuItem("Ouvrir le dossier des journaux")) ShellExecuteW(nullptr, L"open", DYN_DATA_DIR, nullptr, nullptr, SW_SHOWNORMAL);
        if (ImGui::MenuItem("Supprimer les anciennes versions")) app.CleanupLegacy();
        if (ImGui::MenuItem("Désinstaller complètement Dynamo…")) g_openUninstall = true;
        if (ImGui::MenuItem("Rechercher une mise à jour")) { app.updater.Start(0); app.updater.CheckNow(); }
        if (ImGui::MenuItem("À propos")) g_openAbout = true;
        ImGui::Separator();
        if (ImGui::MenuItem("Quitter Dynamo")) U->requestQuit = true;
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------- bandeau d'etat
static void StatusBar(App& app, ImVec2 p, float w) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float h = S(38);
    dl->AddRectFilled(p, ImVec2(p.x + w, p.y + h), col::panel, S(8));
    const sys::Device* d = app.devices.empty() ? nullptr : &app.devices[app.selectedDevice];
    ImU32 dot = col::faint;
    std::string msg;
    char buf[256];
    EngineStatus st = app.Status();
    int action = 0; // 1 installer, 2 definir par defaut, 3 redemarrer audio
    bool virtualFx = d && (d->name.find(L"FxSound") != std::wstring::npos);
    if (app.RoutingActive()) {
        std::string srcN = app.DeviceName(app.settings.routeSource), dstN = app.DeviceName(app.settings.routeTarget);
        int si = app.FindDevice(app.settings.routeSource);
        bool srcDefault = si >= 0 && app.devices[si].isDefault;
        if (st == ENGINE_ACTIVE) {
            dot = app.params.bypass ? col::warn : col::ok;
            snprintf(buf, sizeof(buf), "%s  ·  %s  →  %s  ·  %u Hz  ·  latence ≈ %.0f ms",
                     app.params.bypass ? "Contourné (son d'origine)" : "Traitement actif", srcN.c_str(), dstN.c_str(), app.sampleRate(), app.router.latencyMs);
            msg = buf;
        } else if (app.router.lastErrorCode) {
            dot = col::err; msg = sys::ToUtf8(app.router.lastError);
        } else if (!srcDefault) {
            dot = col::warn;
            snprintf(buf, sizeof(buf), "Windows ne joue plus vers « %s » : le son ne passe pas par Dynamo.", srcN.c_str());
            msg = buf; action = 6;
        } else {
            dot = col::warn;
            snprintf(buf, sizeof(buf), "Routage prêt : %s  →  Dynamo  →  %s  —  en attente de son.", srcN.c_str(), dstN.c_str());
            msg = buf;
        }
    }
    else if (!d) { msg = "Aucune sortie audio active n'a été trouvée."; dot = col::err; }
    else if (virtualFx && !d->installed) {
        dot = col::warn;
        msg = "Ceci est la sortie virtuelle de FxSound : choisissez votre interface audio (ex. RME Fireface UC) dans la liste en haut à droite.";
    }
    else if (!d->installed || true) {
        dot = col::warn;
        snprintf(buf, sizeof(buf), "Dynamo n'est pas actif. « Activer » : Windows jouera vers une sortie libre de ta carte, Dynamo traitera et enverra le son sur « %s ».", sys::ToUtf8(d->name).c_str());
        msg = buf; action = 5;
    } else if (st == ENGINE_ACTIVE) {
        dot = app.params.bypass ? col::warn : col::ok;
        snprintf(buf, sizeof(buf), "%s  ·  %s  ·  %u Hz  ·  %u canaux  ·  latence ajoutée : 0 ms",
                 app.params.bypass ? "Contourné (son d'origine)" : "Traitement actif", sys::ToUtf8(d->name).c_str(), app.sampleRate(), app.channels());
        msg = buf;
        if (!d->isDefault) { msg += "   —   ce n'est pas la sortie par défaut de Windows"; action = 2; }
    } else {
        dot = col::warn;
        snprintf(buf, sizeof(buf), "Installé sur « %s » — en attente de son. Lancez une lecture ; sans effet, redémarrez l'audio ou essayez le mode compatibilité (menu Réglages).", sys::ToUtf8(d->name).c_str());
        msg = buf;
        action = 4;
    }
    if (app.RoutingActive() && app.legacyPresent && app.lastError.empty()) {
        dot = col::warn;
        msg = "Des restes des anciennes versions de Dynamo (1.x) sont encore installés dans Windows.";
        action = 7;
    }
    if (app.updater.state == Updater::READY && app.lastError.empty()) {
        dot = col::ok;
        snprintf(buf, sizeof(buf), "Mise à jour %s installée : elle s'appliquera quand tu fermeras la fenêtre (ou maintenant).", app.updater.newVersion);
        msg = buf; action = 8;
    }
    if (!app.lastError.empty()) { msg = sys::ToUtf8(app.lastError); dot = col::err; }
    else if (!app.lastInfo.empty() && app.time - app.infoTime < 8.0) { msg = app.lastInfo; if (action == 7) action = 0; }

    dl->AddCircleFilled(ImVec2(p.x + S(18), p.y + h / 2), S(5), dot, 16);
    float btnW = 0;
    const char* btn = action == 8 ? "Redémarrer maintenant" : action == 7 ? "Tout supprimer" : action == 5 ? "Activer" : action == 6 ? "Rediriger vers Dynamo" : action == 1 ? "Installer sur ce périphérique" : action == 2 ? "Définir par défaut" : action == 3 ? "Redémarrer l'audio" : action == 4 ? "Diagnostic et réparation" : nullptr;
    if (!app.lastError.empty()) btn = "OK";
    if (btn) btnW = ImGui::CalcTextSize(btn).x + S(28);
    dl->PushClipRect(p, ImVec2(p.x + w - btnW - S(16), p.y + h), true);
    TextAt(dl, U->fBody, ImVec2(p.x + S(32), p.y + (h - U->fBody->FontSize) / 2), col::text, msg.c_str());
    dl->PopClipRect();
    if (btn) {
        ImGui::SetCursorScreenPos(ImVec2(p.x + w - btnW - S(5), p.y + S(4)));
        bool primary = (action == 1 || action == 4 || action == 5 || action == 6 || action == 7) && app.lastError.empty();
        if (primary) {
            ImGui::PushStyleColor(ImGuiCol_Button, V4(col::accent));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.98f, 0.30f, 0.46f, 1));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.80f, 0.18f, 0.32f, 1));
        }
        if (ImGui::Button(btn, ImVec2(btnW, h - S(8)))) {
            if (!app.lastError.empty()) app.lastError.clear();
            else if (action == 7) app.CleanupLegacy();
            else if (action == 8) U->requestRestart = true;
            else if (action == 5) app.ConfigureRouting(app.devices[app.selectedDevice].id);
            else if (action == 6) app.StartRouting();
            else if (action == 1) app.InstallOnSelected(sys::MODE_EFX);
            else if (action == 2) app.MakeSelectedDefault();
            else if (action == 3) app.RestartAudio();
            else if (action == 4) { app.RunDiagnostic(); g_openDiag = true; }
        }
        if (primary) { ImGui::PopStyleColor(3); Tip("Une autorisation administrateur sera demandée.\nLe son sera coupé une seconde pendant le redémarrage de l'audio."); }
    }
}

// ---------------------------------------------------------------- panneau egaliseur
static void EqPanel(App& app, ImVec2 pos, ImVec2 size) {
    DynParams& P = app.params;
    bool changed = false;
    BeginPanel("##eqpanel", pos, size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    const float pad = S(14);
    const float meterPanelW = S(74);

    // en-tete
    ImGui::SetCursorScreenPos(ImVec2(wp.x + pad, wp.y + S(12)));
    SectionTitle("ÉGALISEUR PARAMÉTRIQUE");
    ImGui::SameLine(0, S(12));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + S(0));
    if (Toggle("eqon", &P.eqEnabled, col::accent, "Activer / désactiver l'égaliseur")) changed = true;
    ImGui::SameLine(0, S(24));
    {
        ImGui::PushFont(U->fSmall);
        ImGui::AlignTextToFramePadding();
        ImGui::PopFont();
        static const char* pos[] = { "Avant compression", "Après compression" };
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(3));
        if (Segmented("eqpos", &P.eqPost, pos, 2, 132)) changed = true;
        Tip("Position de l'égaliseur dans la chaîne.\nAvant : l'EQ influence ce que le compresseur « entend ».\nAprès : l'EQ colore le son déjà compressé.");
    }
    // boutons a droite
    {
        const char* t = "Remettre à plat";
        float bw = ImGui::CalcTextSize(t).x + S(20);
        ImGui::SetCursorScreenPos(ImVec2(wp.x + size.x - pad - bw, wp.y + S(8)));
        ImGui::PushFont(U->fSmall);
        if (ImGui::Button(t, ImVec2(bw, S(26)))) { for (int i = 0; i < DYN_EQ_BANDS; ++i) ResetBand(P, i); changed = true; }
        ImGui::PopFont();
        Tip("Toutes les bandes à 0 dB (fréquences d'origine)");
    }

    const float top = wp.y + S(46);
    const float editorH = S(104);
    const float graphH = size.y - (top - wp.y) - editorH - S(10);

    // vumetres d'entree + gain
    {
        ImVec2 mp(wp.x + pad, top);
        TextCentered(dl, U->fSmall, ImVec2(mp.x + meterPanelW / 2 - S(6), mp.y + S(6)), col::dim, "ENTRÉE");
        float mh = graphH - S(98);
        ImVec2 m0(mp.x + S(22), mp.y + S(20));
        Meter(m0, ImVec2(S(10), mh), app.inLevel[0], app.inHold[0]);
        Meter(ImVec2(m0.x + S(13), m0.y), ImVec2(S(10), mh), app.inLevel[1], app.inHold[1]);
        MeterScale(ImVec2(m0.x - S(4), m0.y), mh, true);
        ImGui::SetCursorScreenPos(ImVec2(mp.x - S(4), m0.y + mh + S(6)));
        if (Knob("Gain entrée", &P.inputGain, -24, 24, 0, FmtDb, KNOB_LIN, col::accent, 74, true, "Gain appliqué avant tout traitement")) changed = true;
    }
    // graphe
    ImVec2 gpos(wp.x + pad + meterPanelW + S(6), top);
    ImVec2 gsize(size.x - 2 * pad - 2 * meterPanelW - S(12), graphH);
    if (EqGraph(app, gpos, gsize)) changed = true;
    // vumetres de sortie + gain
    {
        ImVec2 mp(wp.x + size.x - pad - meterPanelW, top);
        TextCentered(dl, U->fSmall, ImVec2(mp.x + meterPanelW / 2 + S(6), mp.y + S(6)), col::dim, "SORTIE");
        float mh = graphH - S(98);
        ImVec2 m0(mp.x + S(26), mp.y + S(20));
        Meter(m0, ImVec2(S(10), mh), app.outLevel[0], app.outHold[0]);
        Meter(ImVec2(m0.x + S(13), m0.y), ImVec2(S(10), mh), app.outLevel[1], app.outHold[1]);
        MeterScale(ImVec2(m0.x + S(28), m0.y), mh, false);
        ImGui::SetCursorScreenPos(ImVec2(mp.x + S(2), m0.y + mh + S(6)));
        if (Knob("Gain sortie", &P.outputGain, -24, 24, 0, FmtDb, KNOB_LIN, col::accent, 74, true, "Volume final, avant le limiteur")) changed = true;
    }

    // editeur de la bande selectionnee
    {
        float ey = top + graphH + S(10);
        ImVec2 e0(gpos.x, ey);
        int& sel = app.settings.selectedBand;
        // pastilles des bandes
        const float chip = S(26);
        for (int i = 0; i < DYN_EQ_BANDS; ++i) {
            ImVec2 c0(e0.x + i * (chip + S(5)), ey + S(4));
            ImGui::SetCursorScreenPos(c0);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##chip", ImVec2(chip, chip))) sel = i;
            bool hov = ImGui::IsItemHovered();
            ImGui::PopID();
            bool en = P.eq[i].enabled && P.eqEnabled;
            ImVec2 cc(c0.x + chip / 2, c0.y + chip / 2);
            if (i == sel) dl->AddRectFilled(c0, ImVec2(c0.x + chip, c0.y + chip), col::band(i, 60), S(6));
            else if (hov) dl->AddRectFilled(c0, ImVec2(c0.x + chip, c0.y + chip), IM_COL32(255, 255, 255, 14), S(6));
            if (en) dl->AddCircleFilled(cc, S(8), col::band(i), 20); else dl->AddCircle(cc, S(8), col::band(i, 150), 20, S(1.5f));
            char n[4]; snprintf(n, sizeof(n), "%d", i + 1);
            TextCentered(dl, U->fSmall, cc, en ? IM_COL32(20, 20, 22, 255) : col::band(i, 200), n);
        }
        DynEqBand& b = P.eq[sel];
        float cx = e0.x;
        float row2 = ey + chip + S(16);
        ImGui::SetCursorScreenPos(ImVec2(cx, row2 + S(2)));
        if (Toggle("bandon", &b.enabled, col::band(sel), "Activer cette bande")) changed = true;
        ImGui::SetCursorScreenPos(ImVec2(cx + S(42), row2));
        ImGui::SetNextItemWidth(S(150));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(S(10), S(4)));
        if (ImGui::BeginCombo("##type", kTypeNames[b.type])) {
            for (int t = 0; t < EQ_TYPE_COUNT; ++t) if (ImGui::Selectable(kTypeNames[t], b.type == t)) { b.type = t; b.enabled = 1; changed = true; }
            ImGui::EndCombo();
        }
        bool isCut = (b.type == EQ_LOWCUT || b.type == EQ_HIGHCUT);
        ImGui::SetCursorScreenPos(ImVec2(cx + S(42), row2 + S(34)));
        ImGui::SetNextItemWidth(S(150));
        ImGui::BeginDisabled(!isCut);
        if (ImGui::BeginCombo("##slope", kSlopeNames[b.slope - 1])) {
            for (int s = 1; s <= 4; ++s) if (ImGui::Selectable(kSlopeNames[s - 1], b.slope == s)) { b.slope = s; changed = true; }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        ImGui::PopStyleVar();

        float kx = cx + S(206);
        float ky = ey - S(2);
        // on place les potentiometres a droite des pastilles
        kx = std::max(kx, e0.x + DYN_EQ_BANDS * (chip + S(5)) + S(16));
        ImU32 bc = col::band(sel);
        ImGui::SetCursorScreenPos(ImVec2(kx, ky));
        DynParams defs; DynDefaultParams(defs);
        if (Knob("Fréquence", &b.freq, 20, 20000, defs.eq[sel].freq, FmtHz, KNOB_LOG, bc, 78)) changed = true;
        ImGui::SetCursorScreenPos(ImVec2(kx + S(80), ky));
        if (Knob("Gain", &b.gain, -24, 24, 0, FmtDb, KNOB_LIN, bc, 72, HasGain(b.type))) changed = true;
        ImGui::SetCursorScreenPos(ImVec2(kx + S(154), ky));
        bool qOn = !(isCut && b.slope > 1);
        if (Knob(b.type == EQ_LOWSHELF || b.type == EQ_HIGHSHELF ? "Pente (Q)" : "Largeur (Q)", &b.q, 0.1f, 18, 0.707f, FmtQ, KNOB_LOG, bc, 78, qOn,
                 "Q faible = large, Q élevé = étroit")) changed = true;
        ImGui::SetCursorScreenPos(ImVec2(kx + S(240), ky + S(22)));
        ImGui::PushFont(U->fSmall);
        if (ImGui::Button("Réinit. bande", ImVec2(0, S(26)))) { ResetBand(P, sel); changed = true; }
        ImGui::PopFont();
        // aide
        const char* help = "Glisser un point : fréquence/gain · Molette : largeur · Double-clic : activer · Clic droit : type";
        ImVec2 hs = TextSize(U->fSmall, help);
        float hx = gpos.x + gsize.x - hs.x;
        if (hx > kx + S(340)) TextAt(dl, U->fSmall, ImVec2(hx, ey + editorH - hs.y - S(12)), col::faint, help);
    }
    EndPanel();
    if (changed) app.ParamsChanged();
}

// ---------------------------------------------------------------- panneau compresseur
static void CompressorPanel(App& app, ImVec2 pos, ImVec2 size) {
    DynParams& P = app.params;
    bool changed = false;
    BeginPanel("##comppanel", pos, size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    const float pad = S(14);

    ImGui::SetCursorScreenPos(ImVec2(wp.x + pad, wp.y + S(12)));
    SectionTitle("COMPRESSEUR");
    ImGui::SameLine(0, S(12));
    if (Toggle("compon", &P.compEnabled, col::comp, "Activer / désactiver le compresseur")) changed = true;
    ImGui::SameLine(0, S(24));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - S(3));
    static const char* modes[] = { "Mono-bande", "Multibande" };
    if (Segmented("mode", &P.compMultiband, modes, 2, 104, col::comp)) changed = true;
    Tip("Multibande : graves, médiums et aigus sont compressés séparément\n(évite que les basses fassent « pomper » tout le son).");
    ImGui::SameLine(0, S(16));
    static const char* det[] = { "Crête", "RMS" };
    if (Segmented("det", &P.detectorRms, det, 2, 64, col::comp)) changed = true;
    Tip("Crête : réagit à chaque pic (plus ferme).\nRMS : suit le niveau moyen (plus naturel, plus doux).");

    const float top = wp.y + S(50);
    const float bottom = wp.y + size.y - S(12);
    const float avail = bottom - top;
    // zone graphique a droite
    const float graphSz = std::min(avail - S(4), S(200));
    const float grW = S(14);
    const float rightW = graphSz + S(12) + (P.compMultiband ? 3 * (grW + S(6)) : grW + S(6)) + S(4);
    const float gx = wp.x + size.x - pad - rightW;
    TransferGraph(app, ImVec2(gx, top), graphSz, P.compMultiband ? app.settings.selectedCompBand : 0);
    {
        float mx = gx + graphSz + S(12);
        if (!P.compMultiband) {
            GrMeter(ImVec2(mx, top), ImVec2(grW, graphSz), app.gr[0], col::comp);
            char b[16]; snprintf(b, sizeof(b), "%.1f", app.gr[0]);
            TextCentered(dl, U->fSmall, ImVec2(mx + grW / 2, top + graphSz + S(10)), col::dim, "GR");
        } else {
            const char* bn[3] = { "G", "M", "A" };
            for (int i = 0; i < 3; ++i) {
                GrMeter(ImVec2(mx + i * (grW + S(6)), top), ImVec2(grW, graphSz), app.gr[i + 1], col::comp);
                TextCentered(dl, U->fSmall, ImVec2(mx + i * (grW + S(6)) + grW / 2, top + graphSz + S(10)), col::dim, bn[i]);
            }
        }
    }

    // zone des reglages
    const float lx = wp.x + pad;
    const float lw = gx - lx - S(16);
    const float kw = std::max(74.f, std::min(96.f, lw / U->dpi / 6.f - 4.f));
    int band = 0;
    float ky = top;
    if (P.compMultiband) {
        // onglets de bandes
        int& sb = app.settings.selectedCompBand;
        if (sb < 1 || sb > 3) sb = 1;
        static const char* names[3] = { "Graves", "Médiums", "Aigus" };
        float tx = lx;
        for (int i = 0; i < 3; ++i) {
            ImVec2 t0(tx, ky);
            ImVec2 tsz(S(128), S(30));
            ImGui::SetCursorScreenPos(t0);
            ImGui::PushID(i);
            if (ImGui::InvisibleButton("##tab", tsz)) sb = i + 1;
            bool hov = ImGui::IsItemHovered();
            ImGui::PopID();
            bool isSel = sb == i + 1;
            dl->AddRectFilled(t0, ImVec2(t0.x + tsz.x, t0.y + tsz.y), isSel ? IM_COL32(46, 196, 182, 45) : (hov ? IM_COL32(255, 255, 255, 12) : col::well), S(6));
            TextAt(dl, U->fBody, ImVec2(t0.x + S(34), t0.y + (tsz.y - U->fBody->FontSize) / 2), isSel ? col::text : col::dim, names[i]);
            // mini vumetre GR horizontal
            float g = std::min(1.f, app.gr[i + 1] / 18.f);
            dl->AddRectFilled(ImVec2(t0.x + S(92), t0.y + S(12)), ImVec2(t0.x + S(120), t0.y + S(18)), col::panel, S(2));
            if (g > 0.01f) dl->AddRectFilled(ImVec2(t0.x + S(92), t0.y + S(12)), ImVec2(t0.x + S(92) + g * S(28), t0.y + S(18)), col::comp, S(2));
            ImGui::SetCursorScreenPos(ImVec2(t0.x + S(8), t0.y + S(7)));
            ImGui::PushID(100 + i);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            bool e = P.comp[i + 1].enabled != 0;
            if (ImGui::Checkbox("##en", &e)) { P.comp[i + 1].enabled = e; changed = true; }
            ImGui::PopStyleVar();
            ImGui::PopID();
            tx += tsz.x + S(6);
        }
        band = sb;
        ky += S(40);
    } else band = 0;

    DynCompBand& c = P.comp[band];
    const bool on = P.compEnabled && c.enabled;
    float kx = lx;
    const float step = S(kw + 4);
    auto knobAt = [&](const char* l, float* v, float mn, float mx, float df, Fmt f, KnobScale sc, bool en, const char* tip) {
        if (kx + S(kw) > lx + lw + S(2)) return;
        ImGui::SetCursorScreenPos(ImVec2(kx, ky));
        if (Knob(l, v, mn, mx, df, f, sc, col::comp, kw, en, tip)) changed = true;
        kx += step;
    };
    knobAt("Seuil", &c.threshold, -60, 0, -18, FmtDbPlain, KNOB_LIN, on, "Niveau à partir duquel la compression commence");
    knobAt("Ratio", &c.ratio, 1, 30, 3, FmtRatio, KNOB_LOG, on, "Force de la compression au-dessus du seuil\n(4:1 = 4 dB en entrée → 1 dB en sortie)");
    knobAt("Attaque", &c.attack, 0.05f, 300, 10, FmtMs, KNOB_LOG, on, "Vitesse de réaction aux pics");
    knobAt("Relâchement", &c.release, 5, 3000, 150, FmtMs, KNOB_LOG, on, "Vitesse de retour après un pic");
    knobAt("Genou", &c.knee, 0, 24, 6, FmtDbPlain, KNOB_LIN, on, "Transition douce autour du seuil");
    knobAt("Gain", &c.makeup, -12, 30, 0, FmtDb, KNOB_LIN, on, "Gain de compensation (make-up)");
    // ligne 2
    kx = lx; ky += S(98);
    if (ky + S(90) > bottom + S(6)) ky = bottom - S(90);
    {
        ImGui::SetCursorScreenPos(ImVec2(kx, ky + S(20)));
        if (Toggle("automk", &c.autoMakeup, col::comp, "Compense automatiquement la perte de volume")) changed = true;
        TextAt(dl, U->fSmall, ImVec2(kx + S(40), ky + S(21)), col::dim, "Gain auto");
        char b[32]; snprintf(b, sizeof(b), "+%.1f dB", c.autoMakeup ? (float)dyn::autoMakeupDb(c) : 0.f);
        TextAt(dl, U->fSmall, ImVec2(kx, ky + S(46)), c.autoMakeup ? col::comp : col::faint, b);
        kx += S(110);
    }
    knobAt("Mélange", &P.mix, 0, 1, 1, FmtPct, KNOB_LIN, P.compEnabled != 0, "Compression parallèle : 100 % = tout compressé,\nmoins = on mélange le son d'origine (plus de naturel)");
    if (!P.compMultiband) {
        knobAt("Filtre détecteur", &P.scHpf, 10, 500, 60, FmtScHpf, KNOB_LOG, P.compEnabled != 0, "Le compresseur ignore les basses sous cette fréquence\n(évite le « pompage » sur les basses)");
    } else {
        knobAt("Coupure G/M", &P.xoverLow, 40, 1000, 150, FmtHz, KNOB_LOG, P.compEnabled != 0, "Fréquence de séparation graves / médiums");
        knobAt("Coupure M/A", &P.xoverHigh, 1000, 12000, 3000, FmtHz, KNOB_LOG, P.compEnabled != 0, "Fréquence de séparation médiums / aigus");
    }
    EndPanel();
    if (changed) app.ParamsChanged();
}

// ---------------------------------------------------------------- panneau limiteur
static void LimiterPanel(App& app, ImVec2 pos, ImVec2 size) {
    DynParams& P = app.params;
    bool changed = false;
    BeginPanel("##limpanel", pos, size);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    const float pad = S(14);
    ImGui::SetCursorScreenPos(ImVec2(wp.x + pad, wp.y + S(12)));
    SectionTitle("LIMITEUR");
    ImGui::SameLine(0, S(12));
    if (Toggle("limon", &P.limEnabled, col::lim, "Empêche tout dépassement du plafond (aucune saturation)")) changed = true;
    const float top = wp.y + S(50);
    const float bottom = wp.y + size.y - S(12);
    ImGui::SetCursorScreenPos(ImVec2(wp.x + pad, top));
    if (Knob("Plafond", &P.limCeiling, -24, 0, -0.3f, FmtDbPlain, KNOB_LIN, col::lim, 74, P.limEnabled != 0, "Niveau maximal en sortie")) changed = true;
    ImGui::SetCursorScreenPos(ImVec2(wp.x + pad, top + S(98)));
    if (bottom - (top + S(98)) > S(80))
        if (Knob("Relâchement", &P.limRelease, 1, 2000, 60, FmtMs, KNOB_LOG, col::lim, 74, P.limEnabled != 0)) changed = true;
    // GR du limiteur
    float mx = wp.x + pad + S(86);
    float mh = bottom - top - S(22);
    GrMeter(ImVec2(mx, top), ImVec2(S(14), mh), app.grLim, col::lim, 12.f);
    TextCentered(dl, U->fSmall, ImVec2(mx + S(7), top + mh + S(12)), col::dim, "GR");
    // historique de reduction globale
    float hx = mx + S(28);
    float hw = wp.x + size.x - pad - hx;
    if (hw > S(60)) {
        TextAt(dl, U->fSmall, ImVec2(hx, top - S(2)), col::dim, "Réduction de gain (compresseur)");
        GrHistory(app, ImVec2(hx, top + S(18)), ImVec2(hw, mh - S(18)));
    }
    EndPanel();
    if (changed) app.ParamsChanged();
}

// ---------------------------------------------------------------- fenetres modales
static void Modals(App& app) {
    if (g_openSaveAs) { ImGui::OpenPopup("Enregistrer le preset"); g_openSaveAs = false; }
    if (g_openUninstall) { ImGui::OpenPopup("Désinstaller Dynamo"); g_openUninstall = false; }
    if (g_openAbout) { ImGui::OpenPopup("À propos de Dynamo"); g_openAbout = false; }
    if (g_openDiag) { ImGui::OpenPopup("Diagnostic"); g_openDiag = false; }
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Enregistrer le preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Nom du preset :");
        ImGui::SetNextItemWidth(S(300));
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputText("##name", g_saveName, sizeof(g_saveName), ImGuiInputTextFlags_EnterReturnsTrue);
        bool exists = false;
        for (auto& p : app.presets) if (p.name == g_saveName) exists = true;
        if (exists) { ImGui::PushFont(U->fSmall); ImGui::TextColored(V4(col::warn), "Un preset porte déjà ce nom : il sera remplacé."); ImGui::PopFont(); }
        ImGui::Spacing();
        if (ImGui::Button("Enregistrer", ImVec2(S(140), 0)) || enter) {
            if (g_saveName[0]) { app.SavePresetAs(g_saveName); ImGui::CloseCurrentPopup(); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Annuler", ImVec2(S(140), 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Désinstaller Dynamo", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Retirer le moteur de tous les périphériques, restaurer les effets\nd'origine de Windows et supprimer les fichiers installés ?");
        ImGui::PushFont(U->fSmall); ImGui::TextDisabled("Vos presets personnels sont conservés dans %%APPDATA%%\\Dynamo."); ImGui::PopFont();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, V4(col::accent));
        if (ImGui::Button("Désinstaller", ImVec2(S(140), 0))) { app.UninstallEverything(); ImGui::CloseCurrentPopup(); }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Annuler", ImVec2(S(140), 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Diagnostic", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static double nextRefresh = 0;
        if (app.time > nextRefresh) { app.RunDiagnostic(); nextRefresh = app.time + 2.0; }
        ImGui::TextDisabled("Vérification automatique (actualisée toutes les 2 s). Laisse de la musique jouer.");
        ImGui::Spacing();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        for (auto& d : app.diag) {
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImU32 c = d.state == 0 ? col::ok : d.state == 1 ? col::warn : col::err;
            dl->AddCircleFilled(ImVec2(p.x + S(7), p.y + ImGui::GetFontSize() * 0.55f), S(5), c, 16);
            ImGui::SetCursorScreenPos(ImVec2(p.x + S(20), p.y));
            ImGui::Text("%s", d.label.c_str());
            ImGui::SameLine(S(300));
            ImGui::PushTextWrapPos(S(760));
            ImGui::TextColored(d.state == 0 ? V4(col::dim) : V4(c), "%s", d.detail.c_str());
            ImGui::PopTextWrapPos();
        }
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
        ImGui::PushFont(U->fSmall);
        ImGui::TextDisabled("« Tout réparer » ferme FxSound, définit la sortie par défaut, réactive les améliorations audio,\npuis essaie automatiquement les 4 modes de branchement jusqu'à ce que le moteur tourne\n(une seule autorisation administrateur, jusqu'à 40 s, le son peut se couper quelques fois).");
        ImGui::PopFont();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, V4(col::accent));
        if (ImGui::Button("Tout réparer", ImVec2(S(170), 0))) app.RepairAll();
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Copier le rapport", ImVec2(S(170), 0))) ImGui::SetClipboardText(app.diagReport.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Fermer", ImVec2(S(120), 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        if (!app.lastError.empty()) { ImGui::Spacing(); ImGui::TextColored(V4(col::err), "%s", sys::ToUtf8(app.lastError).c_str()); }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("À propos de Dynamo", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushFont(U->fTitle); ImGui::TextColored(V4(col::accent), "Dynamo " DYN_VERSION_STR); ImGui::PopFont();
        {
            const char* us = "";
            switch (app.updater.state) {
            case Updater::CHECKING: us = "Recherche de mise à jour…"; break;
            case Updater::DOWNLOADING: us = "Téléchargement d'une mise à jour…"; break;
            case Updater::READY: us = "Nouvelle version installée, active au prochain démarrage."; break;
            case Updater::UP_TO_DATE: us = "Dynamo est à jour."; break;
            case Updater::FAILED: us = app.updater.lastError; break;
            default: us = app.settings.autoUpdate ? "Mises à jour automatiques activées." : "Mises à jour automatiques désactivées."; break;
            }
            ImGui::PushFont(U->fSmall); ImGui::TextDisabled("%s", us); ImGui::PopFont();
        }
        ImGui::Text("Compresseur, égaliseur paramétrique et limiteur pour tout le son de Windows.");
        ImGui::Spacing();
        ImGui::PushFont(U->fSmall);
        ImGui::TextDisabled("Le traitement est inséré directement dans le moteur audio de Windows\n"
                            "(effet de sortie sur le périphérique choisi) : aucune latence ajoutée,\n"
                            "aucun pilote ni logiciel tiers.\n\n"
                            "Raccourcis : Ctrl+S enregistrer le preset · double-clic sur un bouton = valeur par défaut\n"
                            "clic droit sur un bouton = saisir une valeur · Maj = réglage fin");
        ImGui::PopFont();
        ImGui::Spacing();
        if (ImGui::Button("Fermer", ImVec2(S(120), 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

// ---------------------------------------------------------------- point d'entree
void UiDraw(App& app, UiContext& ui) {
    U = &ui;
    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    ImGui::PushFont(ui.fBody);

    const float W = io.DisplaySize.x, H = io.DisplaySize.y;
    const float m = S(14);
    TopBar(app, ImVec2(0, 0), W);
    float y = S(64);
    StatusBar(app, ImVec2(m, y), W - 2 * m);
    y += S(38) + S(10);

    float rest = H - y - m;
    float eqH = std::max(S(330), rest * 0.56f);
    float dynH = rest - eqH - S(10);
    if (dynH < S(250)) { dynH = S(250); eqH = rest - dynH - S(10); }
    EqPanel(app, ImVec2(m, y), ImVec2(W - 2 * m, eqH));
    y += eqH + S(10);
    float limW = std::max(S(300), (W - 2 * m) * 0.30f);
    CompressorPanel(app, ImVec2(m, y), ImVec2(W - 2 * m - limW - S(10), dynH));
    LimiterPanel(app, ImVec2(W - m - limW, y), ImVec2(limW, dynH));

    // ouverture automatique du diagnostic si le moteur installe ne recoit rien
    {
        static bool autoDiagDone = false;
        static double waitingSince = -1;
        bool waiting = !app.RoutingActive() && !app.devices.empty() && app.devices[app.selectedDevice].installed && app.Status() == ENGINE_WAITING;
        if (!waiting) waitingSince = -1;
        else if (waitingSince < 0) waitingSince = app.time;
        if (!autoDiagDone && waiting && app.time - waitingSince > 12.0 && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
            autoDiagDone = true; app.RunDiagnostic(); g_openDiag = true;
        }
    }
    Modals(app);

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        if (app.currentPreset >= 0) app.SaveCurrentPreset();
        else { snprintf(g_saveName, sizeof(g_saveName), "Mon preset"); g_openSaveAs = true; }
    }
    ImGui::PopFont();
    ImGui::End();
}
