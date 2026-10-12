// Dynamo - fonctions systeme : sorties audio, sortie par defaut, demarrage automatique.
// (Aucun droit administrateur : depuis la 2.5, Dynamo ne modifie plus que des reglages de l'utilisateur.)
#pragma once
#include <string>
#include <vector>

namespace sys {

struct Device {
    std::wstring id;        // identifiant complet MMDevice : {0.0.0.00000000}.{guid}
    std::wstring guid;      // partie {guid}
    std::wstring name;      // nom affiche
    bool isDefault = false;
};

std::vector<Device> ListRenderDevices();
bool SetDefaultDevice(const std::wstring& id);

// Demarrage avec Windows (HKCU\...\Run)
bool GetAutostart();
void SetAutostart(bool on);

std::wstring DataDir();                 // %APPDATA%\Dynamo
std::wstring ReadFileTail(const std::wstring& path, size_t maxChars);

std::string  ToUtf8(const std::wstring& w);
std::wstring FromUtf8(const std::string& s);

} // namespace sys
