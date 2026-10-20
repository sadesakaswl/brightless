#!/bin/bash
set -euo pipefail
version=${1:?Expected release version}
cp -a /work/stage /work/AppDir
mkdir -p /work/tools /work/bin
cd /work/tools
python3 - <<'PY'
import hashlib
import json
import os
from pathlib import Path
import urllib.request

for repo, name in [('linuxdeploy/linuxdeploy', 'linuxdeploy'),
                   ('linuxdeploy/linuxdeploy-plugin-qt', 'linuxdeploy-plugin-qt'),
                   ('AppImage/appimagetool', 'appimagetool')]:
    headers = {'Authorization': f"Bearer {os.environ['GH_TOKEN']}"} if os.environ.get('GH_TOKEN') else {}
    request = urllib.request.Request(f'https://api.github.com/repos/{repo}/releases/tags/continuous', headers=headers)
    with urllib.request.urlopen(request, timeout=120) as response:
        release = json.load(response)
    filename = f'{name}-x86_64.AppImage'
    asset = next(item for item in release['assets'] if item['name'] == filename)
    urllib.request.urlretrieve(asset['browser_download_url'], filename)
    digest = hashlib.sha256(Path(filename).read_bytes()).hexdigest()
    assert asset['digest'] == f'sha256:{digest}', f'{filename}: checksum mismatch'
    Path(filename).chmod(0o755)
    print(filename, digest)
PY
for name in linuxdeploy linuxdeploy-plugin-qt appimagetool; do
    mkdir "$name"
    (cd "$name" && "../$name-x86_64.AppImage" --appimage-extract >/dev/null)
done
ln -s /work/tools/linuxdeploy-plugin-qt/squashfs-root/AppRun /work/bin/linuxdeploy-plugin-qt
rm /work/tools/*-x86_64.AppImage
export PATH=/work/bin:$PATH
export QMAKE=/usr/bin/qmake6
export QML_SOURCES_PATHS=/src/src/qt/qml
export EXTRA_PLATFORM_PLUGINS='libqwayland-egl.so;libqwayland-generic.so;libqoffscreen.so'
export QT_QPA_PLATFORM=offscreen
plugins=$(/usr/bin/qmake6 -query QT_INSTALL_PLUGINS)
while IFS= read -r plugin; do
    destination="/work/AppDir/usr/plugins/${plugin#"$plugins/"}"
    mkdir -p "$(dirname "$destination")"
    cp "$plugin" "$destination"
done < <(find "$plugins" -name 'KSC_*.so')
/work/tools/linuxdeploy/squashfs-root/AppRun --appdir /work/AppDir \
    --deploy-deps-only /work/AppDir/usr/plugins --plugin qt \
    --desktop-file /src/resources/com.brightless.Brightless.desktop --icon-file /src/resources/app-icon.png
# These excluded libraries are not guaranteed on minimal desktops.
for library in libusb-1.0.so.0 libcom_err.so.2; do
    cp -L "/usr/lib/x86_64-linux-gnu/$library" /work/AppDir/usr/lib/
done
export ARCH=x86_64
/work/tools/appimagetool/squashfs-root/AppRun /work/AppDir "/out/brightless-$version-x86_64.AppImage"
cd /work
"/out/brightless-$version-x86_64.AppImage" --appimage-extract >/dev/null
dbus-run-session -- python3 /src/tests/app_smoke_test.py /work/squashfs-root/AppRun
