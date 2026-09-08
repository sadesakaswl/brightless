#!/usr/bin/env bash
# Builds the Flatpak bundle without flatpak-builder: the flatpak CLI drives each
# dependency directly, so this works anywhere bwrap runs natively (no Docker nesting).
set -euo pipefail
version=$(awk '/project\(brightless VERSION/{print $3}' /src/CMakeLists.txt)
app=io.github.sadesakaswl.brightless
work=/tmp/flatpak-work
flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
if ! flatpak info --user org.kde.Sdk//6.11 >/dev/null 2>&1; then
    flatpak install --user -y --noninteractive flathub org.kde.Sdk//6.11 org.kde.Platform//6.11
fi

rm -rf "$work"; mkdir -p "$work"; cd "$work"
curl -fL --retry 3 -o jansson.tar.gz https://github.com/akheron/jansson/releases/download/v2.14/jansson-2.14.tar.gz
curl -fL --retry 3 -o ddcutil.tar.gz https://www.ddcutil.com/releases/ddcutil-2.2.7.tar.gz
curl -fL --retry 3 -o libkscreen.tar.xz https://download.kde.org/stable/plasma/6.7.5/libkscreen-6.7.5.tar.xz
tar xf jansson.tar.gz; tar xf ddcutil.tar.gz; tar xf libkscreen.tar.xz

flatpak build-init app "$app" org.kde.Sdk org.kde.Platform 6.11
# CMake try_run binaries cannot execute in nested containers; both builds are portable.
run() { flatpak build --share=network app "$@"; }

run cmake -S jansson-2.14 -B build-jansson -G Ninja -DCMAKE_INSTALL_PREFIX=/app \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DJANSSON_BUILD_DOCS=OFF -DJANSSON_BUILD_SHARED_LIBS=ON -DCMAKE_C_COMPILER_WORKS=1
run cmake --build build-jansson
run cmake --install build-jansson

(cd ddcutil-2.2.7 && run ./configure --prefix=/app --disable-static)
(cd ddcutil-2.2.7 && run make -j"$(nproc)")
(cd ddcutil-2.2.7 && run make install)

run cmake -S libkscreen-6.7.5 -B build-kscreen -G Ninja -DCMAKE_INSTALL_PREFIX=/app \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_CXX_COMPILER_WORKS=1
run cmake --build build-kscreen
run cmake --install build-kscreen

run cmake -S /src -B build-brightless -G Ninja -DCMAKE_INSTALL_PREFIX=/app \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_CXX_COMPILER_WORKS=1
run cmake --build build-brightless
run cmake --install build-brightless

mkdir -p app/files/share/applications app/files/share/metainfo \
    app/files/share/icons/hicolor/256x256/apps
sed 's/^Icon=.*/Icon=io.github.sadesakaswl.brightless/' \
    /src/resources/com.brightless.Brightless.desktop \
    > app/files/share/applications/io.github.sadesakaswl.brightless.desktop
cp /src/resources/com.brightless.Brightless.metainfo.xml \
    app/files/share/metainfo/io.github.sadesakaswl.brightless.metainfo.xml
cp /src/resources/app-icon.png app/files/share/icons/hicolor/256x256/apps/io.github.sadesakaswl.brightless.png

cat > app/metadata <<EOF
[Application]
name=$app
runtime=org.kde.Platform/x86_64/6.11
sdk=org.kde.Sdk/x86_64/6.11
command=brightless

[Context]
shared=ipc;
sockets=x11;wayland;fallback-x11;
devices=all;
filesystems=xdg-config/kdeglobals:ro;/run/udev:ro;

[Environment]
KSCREEN_BACKEND_INPROCESS=1

[Session Bus Policy]
com.canonical.AppMenu.Registrar=talk
org.kde.kconfig.notify=talk
org.kde.KGlobalSettings=talk
org.kde.StatusNotifierWatcher=talk
org.kde.StatusNotifierItem-*=own
com.brightless.Application=own
org.kde.plasmashell=talk
org.kde.kglobalaccel=talk

[Extension io.github.sadesakaswl.brightless.Debug]
directory=lib/debug
autodelete=true
no-autodownload=true

[Build]
built-extensions=io.github.sadesakaswl.brightless.Debug;
EOF
flatpak build-finish --command=brightless app

set +e
timeout 5 flatpak build --runtime --env=QT_QPA_PLATFORM=offscreen --env=QT_QUICK_BACKEND=software \
    app /app/bin/brightless --autostart > smoke.log 2>&1
status=$?
set -e
cat smoke.log
if test "$status" != 124 || grep -E 'failed to load component|ReferenceError:|TypeError:|Cannot assign' smoke.log; then
    echo 'Flatpak runtime smoke test failed' >&2
    exit 1
fi
rm -rf repo
flatpak build-export repo app
mkdir -p /out
flatpak build-bundle repo "/out/brightless-${version}-x86_64.flatpak" "$app" \
    --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo
