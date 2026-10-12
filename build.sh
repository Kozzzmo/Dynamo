#!/bin/sh
# Compilation croisee Linux -> Windows x64 (MinGW-w64)
set -e
cd "$(dirname "$0")"
CXX=${CXX:-x86_64-w64-mingw32-g++}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
STRIP=${STRIP:-x86_64-w64-mingw32-strip}
IM=third_party/imgui
mkdir -p build/obj
FLAGS="-O2 -std=c++17 -DUNICODE -D_UNICODE -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00 -Isrc -I$IM -I$IM/backends"
# 2. ImGui (une seule fois)
for f in imgui imgui_draw imgui_tables imgui_widgets; do
  [ -f build/obj/$f.o ] || $CXX $FLAGS -c $IM/$f.cpp -o build/obj/$f.o
done
for f in imgui_impl_win32 imgui_impl_dx11; do
  [ -f build/obj/$f.o ] || $CXX $FLAGS -c $IM/backends/$f.cpp -o build/obj/$f.o
done
# 3. ressources (icone, manifeste, polices)
$WINDRES -I res -I src res/app.rc -O coff -o build/obj/app.res.o
# 4. application
$CXX $FLAGS -mwindows -municode -static -Wno-class-memaccess -o build/Dynamo.exe \
    src/main.cpp src/app.cpp src/ui.cpp src/system.cpp src/router.cpp src/updater.cpp build/obj/*.o \
    -ld3d11 -ldxgi -ld3dcompiler -ldwmapi -lole32 -loleaut32 -luuid -lshell32 -ladvapi32 -luser32 -lgdi32 -limm32 -lavrt -lwinmm -lversion -lwinhttp -lbcrypt
$STRIP build/Dynamo.exe
ls -la build/Dynamo.exe
