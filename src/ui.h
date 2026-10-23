// Dynamo - interface
#pragma once
#include "imgui.h"
#include "app.h"

struct UiFontSet {            // une famille de polices (selon le skin)
    ImFont* body = nullptr;   // texte courant
    ImFont* small = nullptr;  // petites etiquettes
    ImFont* label = nullptr;  // titres de sections, boutons (graisse forte)
    ImFont* value = nullptr;  // valeurs des potentiometres
    ImFont* num = nullptr;    // grands chiffres (ecrans)
};

struct UiContext {
    float dpi = 1.0f;
    // polices du skin courant
    ImFont* fBody = nullptr;
    ImFont* fSmall = nullptr;
    ImFont* fLabel = nullptr;   // petites capitales des sections
    ImFont* fValue = nullptr;
    ImFont* fNum = nullptr;
    ImFont* fTitle = nullptr;   // ancien logo (serif italique)
    ImFont* fLogo = nullptr;    // nom du produit a cote du symbole Orbite (Unbounded Bold)
    // familles chargees au demarrage
    UiFontSet setMonoSerif, setMonoBlack, setSans;
    // skin
    int skin = 0;
    bool captionDirty = true;   // couleur de la barre de titre Windows a mettre a jour
    unsigned long captionColor = 0, captionText = 0;
    // demandes vers la fenetre
    bool requestQuit = false;
    bool requestHide = false;
    bool requestRestart = false;
};

void UiApplyStyle(float dpi);
void UiSetSkin(UiContext& ui, int index);   // applique couleurs + polices du skin
void UiDraw(App& app, UiContext& ui);
