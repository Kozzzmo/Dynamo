// Dynamo - mise a jour automatique et silencieuse depuis les "Releases" GitHub.
#pragma once
#include <string>

class Updater {
public:
    // Lance la surveillance en arriere-plan (premiere verification apres delaySec, puis toutes les 6 h).
    void Start(int delaySec = 20);
    void Stop();
    void CheckNow();

    // Etat (lu par l'interface)
    enum State { IDLE, CHECKING, DOWNLOADING, READY, UP_TO_DATE, FAILED };
    volatile long state = IDLE;
    char newVersion[32] = {};   // version telechargee et installee (prete au prochain demarrage)
    char lastError[160] = {};

    // Supprime les restes d'une mise a jour precedente (Dynamo.exe.old)
    static void CleanupPrevious();
    // Compare "a.b.c" : <0, 0, >0
    static int CompareVersions(const char* a, const char* b);

private:
    static unsigned long __stdcall ThreadProc(void* self);
    void Run();
    bool CheckOnce();
    void* thread_ = nullptr;
    void* wake_ = nullptr;
    volatile long stop_ = 0;
};
