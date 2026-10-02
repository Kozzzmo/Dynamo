// Dynamo - interface
#pragma once
#include "imgui.h"
#include "app.h"

struct UiContext {
    float dpi = 1.0f;
    ImFont* fBody = nullptr;
    ImFont* fSmall = nullptr;
    ImFont* fLabel = nullptr;   // petites capitales des sections
    ImFont* fValue = nullptr;
    ImFont* fTitle = nullptr;
    // demandes vers la fenetre
    bool requestQuit = false;
    bool requestHide = false;
    bool requestRestart = false;
};

void UiApplyStyle(float dpi);
void UiDraw(App& app, UiContext& ui);
