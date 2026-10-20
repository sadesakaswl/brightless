#!/bin/bash
set -euo pipefail
version=$(awk '/^project\(brightless VERSION / {print $3}' /src/CMakeLists.txt)
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
export HOME=/work/home
export XDG_RUNTIME_DIR=/work/runtime
mkdir -p "$HOME" "$XDG_RUNTIME_DIR"
chmod 700 "$XDG_RUNTIME_DIR"
# Check the export sandbox before downloading the SDK or compiling dependencies.
/usr/libexec/flatpak-validate-icon --sandbox 512 512 /src/resources/app-icon.png
flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
flatpak config --user --set languages en
flatpak install --user -y --noninteractive --no-related flathub org.kde.Platform//6.11 org.kde.Sdk//6.11
flatpak-builder --user --disable-rofiles-fuse --force-clean --repo=/work/repo /work/app /src/packaging/com.brightless.Brightless.json
bundle="/out/brightless-$version-x86_64.flatpak"
flatpak build-bundle /work/repo "$bundle" com.brightless.Brightless stable
flatpak install --user -y --noninteractive --no-deps --no-related "$bundle"
cat > /work/launch <<'SH'
#!/bin/bash
exec flatpak run --user --env=QT_QPA_PLATFORM=offscreen --env=QT_QUICK_BACKEND=software --env=QT_FORCE_STDERR_LOGGING=1 com.brightless.Brightless "$@"
SH
chmod +x /work/launch
dbus-run-session -- python3 /src/tests/app_smoke_test.py /work/launch
