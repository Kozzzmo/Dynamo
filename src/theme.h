// Dynamo - skins de l'interface, dans la famille visuelle des plugins Kinetik / J-Slice
// (NEON, MIDNIGHT, RISO, CLASSIC : memes couleurs, memes polices, memes potentiometres).
#pragma once
#include "imgui.h"

enum KnobStyle { KS_TICKS, KS_ARC_THICK, KS_ARC };
enum NumFont { NF_SERIF, NF_BLACK, NF_SANS };

struct Skin {
    const char* name;
    // surfaces
    ImU32 bg, panel, panelEdge, inner;
    ImU32 text, dim, faint;
    ImU32 accent, accentText;       // LED, etoile du logo
    ImU32 selFill, selText;         // boutons / onglets selectionnes
    ImU32 btnFill, btnEdge, btnText;
    ImU32 lcdBg, lcdText, lcdDim;   // ecrans (bandeau, graphes)
    ImU32 knobBody, knobRing, knobTrack, knobPointer;
    ImU32 dots;                     // trame demi-teinte (RISO)
    // couleurs propres a Dynamo
    ImU32 eq, comp, lim;            // egaliseur, compresseur, limiteur
    ImU32 ok, warn, err;
    ImU32 meterLow, meterMid, meterHigh, meterClip, ledOff;
    ImU32 curve;                    // courbe totale de l'egaliseur
    ImU32 nodeText;                 // chiffre dans un point d'EQ actif
    ImU32 bands[10];
    float radius, edgeWidth;
    float labelSpacing;             // espacement des lettres des etiquettes (em)
    KnobStyle knob;
    NumFont num;
    bool glow, halftone, sansLabels;
};

#define RGB_(r, g, b) IM_COL32(r, g, b, 255)

static const Skin kSkins[] = {
    // ---- NEON : bleu nuit et pervenche lumineuse (skin par defaut de Kinetik) ----
    { "NEON",
      RGB_(9, 11, 26), RGB_(14, 18, 40), RGB_(42, 50, 100), RGB_(8, 10, 24),
      RGB_(200, 208, 255), RGB_(118, 128, 180), RGB_(60, 68, 120),
      RGB_(170, 182, 255), RGB_(9, 11, 26),
      RGB_(170, 182, 255), RGB_(9, 11, 26),
      RGB_(14, 18, 40), RGB_(52, 62, 120), RGB_(200, 208, 255),
      RGB_(6, 8, 20), RGB_(206, 214, 255), RGB_(118, 128, 180),
      RGB_(16, 20, 46), RGB_(42, 50, 100), RGB_(36, 42, 84), RGB_(225, 230, 255),
      RGB_(70, 80, 140),
      RGB_(170, 182, 255), RGB_(127, 226, 210), RGB_(255, 70, 85),
      RGB_(127, 226, 210), RGB_(255, 196, 120), RGB_(255, 104, 150),
      RGB_(127, 226, 210), RGB_(170, 182, 255), RGB_(255, 140, 150), RGB_(255, 104, 150), RGB_(22, 27, 58),
      RGB_(225, 230, 255), RGB_(9, 11, 26),
      { RGB_(255, 134, 200), RGB_(255, 160, 150), RGB_(255, 206, 140), RGB_(214, 236, 150), RGB_(127, 226, 210),
        RGB_(120, 210, 255), RGB_(150, 170, 255), RGB_(180, 150, 255), RGB_(214, 140, 255), RGB_(255, 150, 235) },
      6.f, 1.f, 0.07f, KS_TICKS, NF_SERIF, true, false, false },

    // ---- MIDNIGHT : marine, creme et vermillon, chiffres serif ----
    { "MIDNIGHT",
      RGB_(44, 51, 82), RGB_(40, 47, 77), RGB_(78, 87, 122), RGB_(33, 39, 66),
      RGB_(237, 230, 214), RGB_(160, 166, 190), RGB_(98, 106, 140),
      RGB_(232, 90, 64), RGB_(30, 34, 58),
      RGB_(237, 230, 214), RGB_(32, 37, 64),
      RGB_(40, 47, 77), RGB_(96, 105, 140), RGB_(237, 230, 214),
      RGB_(29, 34, 58), RGB_(237, 230, 214), RGB_(160, 166, 190),
      RGB_(29, 34, 58), RGB_(96, 105, 140), RGB_(78, 87, 122), RGB_(237, 230, 214),
      RGB_(120, 128, 160),
      RGB_(237, 230, 214), RGB_(111, 199, 184), RGB_(236, 160, 70),
      RGB_(111, 199, 184), RGB_(236, 160, 70), RGB_(232, 90, 64),
      RGB_(111, 199, 184), RGB_(237, 230, 214), RGB_(236, 160, 70), RGB_(232, 90, 64), RGB_(36, 43, 72),
      RGB_(237, 230, 214), RGB_(32, 37, 64),
      { RGB_(232, 90, 64), RGB_(236, 160, 70), RGB_(237, 206, 140), RGB_(186, 204, 150), RGB_(111, 199, 184),
        RGB_(120, 170, 220), RGB_(160, 160, 230), RGB_(185, 168, 230), RGB_(220, 150, 190), RGB_(237, 230, 214) },
      3.f, 1.2f, 0.07f, KS_TICKS, NF_SERIF, false, false, false },

    // ---- RISO : risographie vermillon, papier saumon, encre noire ----
    { "RISO",
      RGB_(214, 69, 43), RGB_(214, 69, 43), RGB_(120, 30, 18), RGB_(240, 178, 160),
      RGB_(24, 16, 14), RGB_(96, 30, 18), RGB_(150, 50, 30),
      RGB_(24, 16, 14), RGB_(244, 190, 172),
      RGB_(24, 16, 14), RGB_(244, 190, 172),
      RGB_(214, 69, 43), RGB_(120, 30, 18), RGB_(24, 16, 14),
      RGB_(240, 178, 160), RGB_(24, 16, 14), RGB_(110, 40, 26),
      RGB_(244, 196, 180), RGB_(120, 30, 18), RGB_(196, 80, 56), RGB_(24, 16, 14),
      RGB_(170, 60, 40),
      RGB_(24, 16, 14), RGB_(36, 56, 140), RGB_(120, 30, 18),
      RGB_(24, 16, 14), RGB_(120, 30, 18), RGB_(255, 244, 236),
      RGB_(24, 16, 14), RGB_(24, 16, 14), RGB_(120, 30, 18), RGB_(214, 69, 43), RGB_(230, 160, 140),
      RGB_(24, 16, 14), RGB_(244, 190, 172),
      { RGB_(24, 16, 14), RGB_(36, 56, 140), RGB_(24, 16, 14), RGB_(36, 56, 140), RGB_(24, 16, 14),
        RGB_(36, 56, 140), RGB_(24, 16, 14), RGB_(36, 56, 140), RGB_(24, 16, 14), RGB_(36, 56, 140) },
      2.f, 2.f, 0.07f, KS_ARC_THICK, NF_BLACK, false, true, false },

    // ---- CLASSIC : le graphite d'origine ----
    { "CLASSIC",
      RGB_(22, 24, 28), RGB_(33, 36, 42), RGB_(60, 65, 74), RGB_(28, 30, 35),
      RGB_(214, 218, 224), RGB_(120, 127, 138), RGB_(80, 85, 94),
      RGB_(240, 64, 52), RGB_(255, 255, 255),
      RGB_(36, 110, 104), RGB_(255, 255, 255),
      RGB_(42, 46, 53), RGB_(60, 65, 74), RGB_(214, 218, 224),
      RGB_(18, 26, 22), RGB_(150, 240, 170), RGB_(60, 110, 75),
      RGB_(52, 57, 65), RGB_(55, 60, 68), RGB_(55, 60, 68), RGB_(235, 238, 242),
      RGB_(80, 85, 94),
      RGB_(240, 64, 52), RGB_(60, 200, 190), RGB_(255, 170, 40),
      RGB_(64, 200, 110), RGB_(255, 170, 40), RGB_(240, 64, 52),
      RGB_(64, 200, 110), RGB_(210, 210, 70), RGB_(255, 150, 40), RGB_(240, 64, 52), RGB_(40, 44, 50),
      RGB_(245, 245, 250), RGB_(20, 20, 22),
      { RGB_(255, 99, 99), RGB_(255, 146, 76), RGB_(255, 199, 64), RGB_(196, 222, 70), RGB_(98, 214, 122),
        RGB_(60, 200, 190), RGB_(70, 166, 255), RGB_(120, 124, 255), RGB_(182, 106, 255), RGB_(240, 96, 200) },
      4.f, 1.f, 0.f, KS_ARC, NF_SANS, false, false, true },
};
static const int kNumSkins = (int)(sizeof(kSkins) / sizeof(kSkins[0]));
static const int kDefaultSkin = 0; // NEON

#undef RGB_
