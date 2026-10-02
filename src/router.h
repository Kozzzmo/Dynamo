// Dynamo - routage interne : capture "loopback" d'une sortie Windows, traitement, rendu sur la sortie reelle.
#pragma once
#include <string>
#include "shared.h"

class Router {
public:
    // sh : zone de parametres / vumetres (memes structures que le moteur APO)
    bool Start(const std::wstring& sourceId, const std::wstring& targetId, DynShared* sh);
    void Stop();
    bool Running() const { return thread_ != nullptr; }

    // etat (lu par l'interface)
    volatile float latencyMs = 0;
    volatile bool streaming = false;     // flux ouverts
    volatile long lastErrorCode = 0;      // 0 = ok
    wchar_t lastError[256] = {};          // message lisible
    volatile long xruns = 0;

private:
    static unsigned long __stdcall ThreadProc(void* self);
    void Run();
    bool Session();  // une session audio complete ; false = erreur (on reessaie)

    std::wstring src_, dst_;
    DynShared* sh_ = nullptr;
    void* thread_ = nullptr;
    volatile long stop_ = 0;
};
