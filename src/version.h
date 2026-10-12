// Dynamo - numero de version (source unique : interface, fichier .exe, mises a jour)
#pragma once
#define DYN_VERSION_MAJOR 2
#define DYN_VERSION_MINOR 7
#define DYN_VERSION_PATCH 0
#define DYN_VERSION_STR   "2.7.0"
#define DYN_VERSION_SHORT "2.7"
#define DYN_VERSION_RC    2,7,0,0

// Adresse des mises a jour : depot PUBLIC de telechargements, release a etiquette fixe "kdyn10"
// (le depot du code source peut ainsi rester prive). Fichiers : latest.txt et Dynamo.exe.
// Les versions <= 2.6.1 lisent encore github.com/Kozzzmo/dynamo/releases/latest/download/.
#define DYN_UPDATE_BASE   L"https://github.com/Kozzzmo/kinetik-downloads/releases/download/kdyn10/"
