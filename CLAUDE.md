# Kinetik KDYN-10 (formerly Dynamo) — project memory for Claude

Windows 10/11 application: compressor, 10-band parametric EQ and limiter for all of Windows' sound. Win32 +
Direct3D 11 + Dear ImGui, cross-compiled with MinGW (`CXX=x86_64-w64-mingw32-g++-posix ./build.sh`). French UI.

## The rename (2.5.0)

The product was renamed from "Dynamo" to **Kinetik KDYN-10** in user-visible text only. Internal identifiers keep
"Dynamo" on purpose, so existing installations and the auto-updater keep working — do not rename them without a
migration: the version resource `ProductName` ("Dynamo", checked by the updater of installed copies and by
`RecycleOldExecutables`), registry keys `SOFTWARE\Dynamo`, `C:\ProgramData\Dynamo`, `%APPDATA%\Dynamo`,
`DynamoAPO.dll`, the CLSID, mutex/window class names, the autostart value, the preset format header `[Dynamo]`,
the release assets `Dynamo.exe` + `latest.txt` read by the updater.
Releases also carry `Kinetik-KDYN10.exe` (same file) for new downloads.

## Releasing

Bump `src/version.h`, add a `CHANGELOG.md` section, push to `main`: `.github/workflows/release.yml` builds, uploads
`Kinetik-KDYN10.exe`, `Dynamo.exe` and `latest.txt` to the fixed-tag release `kdyn10` of the PUBLIC repository
`Kozzzmo/kinetik-downloads` (secret `DOWNLOADS_TOKEN`), then creates `v<version>` here (only if new).
Installed copies >= 2.7.0 update from `DYN_UPDATE_BASE` (`src/version.h`) = that public release, so this
repository can be private. Copies <= 2.6.1 still read this repository's latest release: keep it public until they
have updated (they check at start-up and every 6 hours).

## Working with the owner (read first)

- The owner (GitHub **Kozzzmo**) writes in **French**: answer in French.
- Git: commit as `Kozzzmo <Kozzzmo@users.noreply.github.com>`. **No `Co-Authored-By`, `Claude-Session` or any
  Claude attribution** in commits, PRs or files — the owner asked explicitly (history was rewritten to remove them).
- The code is **proprietary** (see `LICENSE`). Third-party code is allowed only under a permissive licence and must be
  listed in `THIRD_PARTY_NOTICES.txt`, which ships with every release.
- Product family: Kinetik (KDSP-16, KMSP-6, J-Slice, KDYN-10). Shared look: serif italic "Kinetik" logo + four-point
  star, skins NEON (default) / MIDNIGHT / RISO / CLASSIC, Instrument Serif + IBM Plex Mono + Archivo Black (OFL).
- Launch tracker (claude.ai artifact, tasks in its db collection `tasks`):
  https://claude.ai/artifact/3DAkMghoFDeC59JUTAcdkn — update task status with the ArtifactData tool when work
  from the list is done.
