#!/usr/bin/env bash
set -euo pipefail
version=$(awk '/project\(brightless VERSION/{print $3}' /src/CMakeLists.txt)
export XDG_RUNTIME_DIR=/tmp/flatpak-runtime
mkdir -p "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak install --user -y --noninteractive flathub org.kde.Sdk//6.11 org.kde.Platform//6.11
flatpak-builder --user --force-clean --disable-rofiles-fuse --repo=/build/flatpak-repo \
    /build/flatpak /src/packaging/flatpak.json
# Smoke-test against the runtime, not the development SDK.
set +e
timeout 5 flatpak build --runtime --env=QT_QPA_PLATFORM=offscreen --env=QT_QUICK_BACKEND=software \
    /build/flatpak /app/bin/brightless --autostart > /build/flatpak-smoke.log 2>&1
status=$?
set -e
cat /build/flatpak-smoke.log
if test "$status" != 124 || grep -E 'failed to load component|ReferenceError:|TypeError:|Cannot assign' /build/flatpak-smoke.log; then
    echo 'Flatpak runtime smoke test failed' >&2
    exit 1
fi
flatpak build-bundle /build/flatpak-repo "/out/brightless-${version}-x86_64.flatpak" \
    io.github.sadesakaswl.brightless --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo
