// Dynamo - etat de l'application (hors dessin)
#pragma once
#include <string>
#include <vector>
#include "shared.h"
#include "system.h"
#include "router.h"
#include "updater.h"

struct Preset {
    std::string name;
    DynParams params;
    bool builtin = false;
};

struct DiagItem {
    std::string label;
    int state = 0;          // 0 = ok, 1 = attention, 2 = probleme
    std::string detail;
};

struct Settings {
    bool startWithWindows = false;
    bool closeToTray = true;
    bool showSpectrum = true;
    bool setAsDefault = true;
    std::string lastPreset = "Neutre";
    int selectedBand = 0;
    int selectedCompBand = 1;
    // routage interne
    bool autoUpdate = true;
    int skin = 0;              // 0 = NEON
    bool routingOn = false;
    std::string routeSource;   // id MMDevice de la sortie capturee (ex. Analog (3+4))
    std::string routeTarget;   // id MMDevice de la sortie reelle (ex. Haut-parleurs RME)
};

enum EngineStatus {
    ENGINE_NOT_INSTALLED,    // aucun peripherique n'a le moteur
    ENGINE_WAITING,          // installe mais aucun son ne passe (ou pas encore charge)
    ENGINE_ACTIVE,           // du son est traite
};

class App {
public:
    void Init();
    void Shutdown();
    void Tick(double dt);   // appele a chaque image : vumetres, spectre, sauvegardes differees

    // parametres
    DynParams params{};
    void ParamsChanged();   // a appeler apres toute modification (envoi au moteur + marque modifie)
    bool modified = false;

    // presets
    std::vector<Preset> presets;
    int currentPreset = -1;
    void LoadPreset(int idx);
    bool SavePresetAs(const std::string& name);
    bool SaveCurrentPreset();
    bool DeletePreset(int idx);
    void RefreshPresets();
    std::string CurrentPresetLabel() const;

    // peripheriques
    std::vector<sys::Device> devices;
    int selectedDevice = 0;
    void RefreshDevices();
    bool InstallOnSelected(int mode);
    bool UninstallFromSelected();
    bool UninstallEverything();
    bool RestartAudio();
    bool MakeSelectedDefault();
    std::wstring lastError;
    std::string  lastInfo;
    double infoTime = 0;

    // moteur / lien
    EngineStatus Status() const;
    bool linked() const { return sh_ != nullptr; }
    uint32_t sampleRate() const;
    uint32_t channels() const;
    bool anyInstalled() const;

    // vumetres (lisses pour l'affichage)
    float inLevel[2] = {0, 0}, outLevel[2] = {0, 0};     // dB
    float inHold[2] = {-100, -100}, outHold[2] = {-100, -100};
    float gr[DYN_COMP_BANDS] = {0, 0, 0, 0};
    float grLim = 0;
    float compLevel = -120.f;
    std::vector<float> grHistory;   // historique de reduction de gain (dB)
    int grHistoryPos = 0;

    // spectre (dB par point de frequence logarithmique)
    static const int kSpecPoints = 256;
    float spectrum[kSpecPoints];
    bool spectrumValid = false;

    Settings settings;
    void SaveSettings();

    double time = 0;
    // diagnostic / reparation automatique
    std::vector<DiagItem> diag;
    std::string diagReport;
    void RunDiagnostic();
    void RepairAll();
    double diagRecheckAt = -1;

    // routage interne (capture de "Analog (3+4)" -> traitement -> "Haut-parleurs")
    Router router;
    bool RoutingActive() const { return settings.routingOn; }
    bool ConfigureRouting(const std::wstring& target, const std::wstring& source = L"");
    void DisableRouting();
    void StartRouting();
    std::string DeviceName(const std::string& id) const;
    int FindDevice(const std::string& id) const;

    Updater updater;
    bool keepRoutingOnExit = false;   // relance (mise a jour / nouvelle instance) : ne pas toucher a la sortie Windows
    bool legacyPresent = false;
    bool CleanupLegacy();

    bool demoDevices = false;   // --demo : peripheriques fictifs (tests d'affichage)

private:
    void OpenLink();
    void PushParams();
    void SaveState();
    void LoadSettings();
    void ComputeSpectrum();
    std::wstring DataDir() const;

    void* map_ = nullptr;
    DynShared* local_ = nullptr;
    DynShared* sh_ = nullptr;
    int32_t lastBeat_ = 0;
    double lastBeatTime_ = -100;
    double nextLinkTry_ = 0;
    double nextDeviceCheck_ = 0;
    bool statePending_ = false;
    double stateSaveAt_ = 0;
    double grAccum_ = 0;
    float grPeakSinceSample_ = 0;
    std::vector<float> fftRe_, fftIm_, win_, specSmooth_;
};
