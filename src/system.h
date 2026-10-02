// Dynamo - fonctions systeme : peripheriques audio, installation du moteur, routage.
#pragma once
#include <string>
#include <vector>

namespace sys {

struct Device {
    std::wstring id;        // identifiant complet MMDevice : {0.0.0.00000000}.{guid}
    std::wstring guid;      // partie {guid} (cle de registre)
    std::wstring name;      // nom affiche
    bool isDefault = false;
    bool installed = false;       // notre moteur est inscrit sur ce peripherique
    bool enhancementsOff = false; // "ameliorations audio" desactivees par Windows
    int  mode = 0;                // 0 = EFX (defaut), 1 = MFX, 2 = GFX (ancien), 3 = SFX
};

enum InstallMode { MODE_EFX = 0, MODE_MFX = 1, MODE_GFX = 2, MODE_SFX = 3 };
const char* ModeName(int mode);
float EndpointPeak(const std::wstring& id);   // niveau du son qui arrive sur la sortie (0..1), -1 si inconnu

// Enumeration (sans droits administrateur)
std::vector<Device> ListRenderDevices();
bool SetDefaultDevice(const std::wstring& id);

// Operations administrateur (executees dans un processus eleve)
int  RunElevatedCommand(int argc, wchar_t** argv);             // point d'entree "--elevated ..."
bool RequestElevated(const std::wstring& args, std::wstring& error, unsigned long* codeOut = nullptr); // lance l'exe en admin et attend

// Demarrage avec Windows (HKCU\...\Run)
bool GetAutostart();
void SetAutostart(bool on);

std::wstring InstallDir();      // C:\Program Files\Dynamo
std::wstring InstalledExePath();
bool IsRunningFromInstallDir();
bool EngineFilesInstalled();
std::wstring ReadInstallLogTail(size_t maxChars);

bool ProcessRunning(const wchar_t* exeName);
int  KillProcess(const wchar_t* exeName);
bool ComRegistered();
bool LegacyPresent();          // restes des versions 1.x (moteur APO)
int  RecycleOldExecutables();  // anciens Dynamo*.exe -> corbeille
std::wstring ReadFileTail(const std::wstring& path, size_t maxChars);

std::string  ToUtf8(const std::wstring& w);
std::wstring FromUtf8(const std::string& s);

} // namespace sys
