# Dynamo

Compresseur, égaliseur paramétrique 10 bandes et limiteur pour **tout le son de Windows 10/11**,
sans pilote ni logiciel tiers.

## Télécharger

👉 **[Dernière version de Dynamo.exe](../../releases/latest/download/Dynamo.exe)** (Windows 64 bits)

Un seul fichier, rien à installer. Au premier lancement, Windows SmartScreen peut afficher un
avertissement (le logiciel n'est pas signé) : *Informations complémentaires → Exécuter quand même*.

Ensuite, Dynamo **se met à jour tout seul** : il vérifie au démarrage puis toutes les 6 heures,
télécharge la nouvelle version, contrôle son empreinte SHA-256 et l'applique quand la fenêtre est fermée.

## Fonctionnement

1. Cliquez sur **Activer** : Windows joue vers une sortie libre de votre carte son
   (par ex. « Analog (3+4) »).
2. Dynamo capte ce son, le traite (EQ → compresseur → limiteur) et l'envoie vers votre vraie sortie
   (par ex. « Haut-parleurs »).

Latence ajoutée : environ 20 à 30 ms (affichée dans le bandeau).

## Publier une nouvelle version

1. Modifier le code, puis augmenter la version dans `src/version.h`.
2. Ajouter une section dans `CHANGELOG.md`.
3. Pousser sur `main` : GitHub Actions compile `Dynamo.exe` et publie la version
   (avec `latest.txt` lu par les mises à jour automatiques).

## Compiler soi-même (Linux)

```sh
sudo apt install g++-mingw-w64-x86-64-posix
CXX=x86_64-w64-mingw32-g++-posix ./build.sh
```

## Licences

Interface : [Dear ImGui](https://github.com/ocornut/imgui) — licence MIT
(voir `third_party/imgui/LICENSE.txt`).

Polices embarquées, licence SIL Open Font License 1.1 (voir `res/fonts/OFL-*.txt`) :
Instrument Serif, IBM Plex Mono, Archivo Black.
