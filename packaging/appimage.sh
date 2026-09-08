#!/usr/bin/env bash
set -euo pipefail
version=$(awk '/project\(brightless VERSION/{print $3}' /src/CMakeLists.txt)
mkdir -p /build/tools /build/AppDir
if test ! -s /build/tools/linuxdeploy-x86_64.AppImage; then
  curl -fL --retry 3 -o /build/tools/linuxdeploy-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage
fi
if test ! -s /build/tools/linuxdeploy-plugin-qt-x86_64.AppImage; then
  curl -fL --retry 3 -o /build/tools/linuxdeploy-plugin-qt-x86_64.AppImage \
    https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage
fi
chmod +x /build/tools/*.AppImage 2>/dev/null || true
export PATH="/build/tools:$PATH" APPIMAGE_EXTRACT_AND_RUN=1
export QMAKE=/usr/bin/qmake6 QML_SOURCES_PATHS=/src/src/qt/qml EXTRA_QT_PLUGINS="platforms/libqoffscreen.so;kf6/kscreen"
cp -a /stage/usr /build/AppDir/
linuxdeploy-x86_64.AppImage --appdir /build/AppDir --plugin qt
# The AppImage must not rely on a host-installed KScreen D-Bus launcher.
mv /build/AppDir/AppRun /build/AppDir/AppRun.original
cat > /build/AppDir/AppRun <<'EOF'
#!/bin/sh
export KSCREEN_BACKEND_INPROCESS=1
here=$(dirname "$(readlink -f "$0")")
exec "$here/AppRun.original" "$@"
EOF
chmod +x /build/AppDir/AppRun
dbus-run-session -- python3 /src/tests/app_smoke_test.py /build/AppDir/AppRun
export VERSION="$version" OUTPUT="/out/brightless-${version}-x86_64.AppImage"
linuxdeploy-x86_64.AppImage --appdir /build/AppDir --output appimage
chmod +x "$OUTPUT"
