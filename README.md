# Kinetik KDYN-10

*Anciennement « Dynamo ». Le dépôt et les noms internes (dossiers, registre) gardent l'ancien nom pour que
les installations et les mises à jour existantes continuent de fonctionner.*

Compresseur, égaliseur paramétrique 10 bandes et limiteur pour **tout le son de Windows 10/11**,
sans pilote ni logiciel tiers.

## Télécharger

👉 **[Dernière version de Kinetik KDYN-10](../../releases/latest/download/Kinetik-KDYN10.exe)** (Windows 64 bits)

Un seul fichier, rien à installer. Au premier lancement, Windows SmartScreen peut afficher un
avertissement (le logiciel n'est pas signé) : *Informations complémentaires → Exécuter quand même*.

KDYN-10 ne demande jamais les droits administrateur et n'installe rien dans Windows : il ne crée
que ses réglages dans `%APPDATA%\Dynamo` et, si vous le souhaitez, une entrée « lancer au démarrage ».

Ensuite, KDYN-10 **se met à jour tout seul** : il vérifie au démarrage puis toutes les 6 heures,
télécharge la nouvelle version, contrôle son empreinte SHA-256 et l'applique quand la fenêtre est fermée.

## Fonctionnement

1. Cliquez sur **Activer** : Windows joue vers une sortie libre de votre carte son
   (par ex. « Analog (3+4) »).
2. KDYN-10 capte ce son, le traite (EQ → compresseur → limiteur) et l'envoie vers votre vraie sortie
   (par ex. « Haut-parleurs »).

Latence ajoutée : environ 20 à 30 ms (affichée dans le bandeau).

## Publier une nouvelle version

1. Modifier le code, puis augmenter la version dans `src/version.h`.
2. Ajouter une section dans `CHANGELOG.md`.
3. Pousser sur `main` : GitHub Actions compile l'exe et publie la version (`Kinetik-KDYN10.exe` pour les nouveaux téléchargements, `Dynamo.exe` pour les mises à jour automatiques)
   (avec `latest.txt` lu par les mises à jour automatiques).

## Compiler soi-même (Linux)

```sh
sudo apt install g++-mingw-w64-x86-64-posix
CXX=x86_64-w64-mingw32-g++-posix ./build.sh
```

## Licences

Logiciel propriétaire, tous droits réservés (voir `LICENSE`). Interface : [Dear ImGui](https://github.com/ocornut/imgui),
licence MIT. Polices embarquées, licence SIL Open Font License 1.1 : Instrument Serif, IBM Plex Mono, Archivo Black.
Les textes de ces licences sont dans `THIRD_PARTY_NOTICES.txt`, à livrer avec chaque version.
