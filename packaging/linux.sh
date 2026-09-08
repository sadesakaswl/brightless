#!/usr/bin/env bash
set -euo pipefail
version=$(awk '/project\(brightless VERSION/{print $3}' /src/CMakeLists.txt)
cmake -S /src -B /build/cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build /build/cmake --parallel "${JOBS:-4}"
dbus-run-session -- ctest --test-dir /build/cmake --output-on-failure
DESTDIR=/stage cmake --install /build/cmake --strip
desktop-file-validate /stage/usr/share/applications/com.brightless.Brightless.desktop
dbus-run-session -- python3 /src/tests/app_smoke_test.py /stage/usr/bin/brightless
mkdir -p /out

if test -f /etc/debian_version; then
    mkdir -p debian /stage/DEBIAN
    printf 'Source: brightless\nSection: utils\nPriority: optional\nMaintainer: Brightless contributors <noreply@github.com>\nStandards-Version: 4.7.0\n\nPackage: brightless\nArchitecture: amd64\nDescription: External monitor controls\n' > debian/control
    deps=$(dpkg-shlibdeps -O -e/stage/usr/bin/brightless | sed -n 's/^shlibs:Depends=//p')
    cat > /stage/DEBIAN/control <<EOF
Package: brightless
Version: $version-1
Section: utils
Priority: optional
Architecture: amd64
Maintainer: Brightless contributors <noreply@github.com>
Installed-Size: $(du -sk /stage/usr | cut -f1)
Depends: $deps, libkscreen-bin, qt6-wayland, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-window, qml6-module-qtquick-templates, qml6-module-qtqml-workerscript
Homepage: https://github.com/sadesakaswl/brightless
Description: External monitor brightness, contrast and volume control
 Qt DDC/CI controls with optional KDE Plasma desktop integration.
EOF
    dpkg-deb --build --root-owner-group /stage "/out/brightless_${version}-1_amd64.deb"
    # Match the previous portable Linux archive layout. Runtime libraries remain distro-provided.
    portable="brightless-v${version}-linux-x64"
    mkdir -p "$portable/bin"
    cp /stage/usr/bin/brightless "$portable/bin/"
    cp /src/LICENSE /src/README.md "$portable/"
    zip -r "/out/$portable.zip" "$portable"
elif test -f /etc/fedora-release; then
    cat > brightless.spec <<EOF
Name: brightless
Version: $version
Release: 1%{?dist}
Summary: External monitor brightness, contrast and volume control
License: GPL-3.0-only
URL: https://github.com/sadesakaswl/brightless
Requires: qt6-qtdeclarative, qt6-qtwayland
%global debug_package %{nil}
%description
Qt DDC/CI controls with optional KDE Plasma desktop integration.
%install
mkdir -p %{buildroot}
cp -a /stage/usr %{buildroot}/
%files
/usr/bin/brightless
/usr/share/applications/com.brightless.Brightless.desktop
/usr/share/metainfo/com.brightless.Brightless.metainfo.xml
/usr/share/icons/hicolor/256x256/apps/com.brightless.Brightless.png
%doc /usr/share/doc/brightless/README.md
%license /usr/share/doc/brightless/LICENSE
EOF
    rpmbuild --define '_topdir /build/rpmbuild' -bb brightless.spec
    cp /build/rpmbuild/RPMS/x86_64/*.rpm /out/
else
    cat > PKGBUILD <<EOF
pkgname=brightless
pkgver=$version
pkgrel=1
pkgdesc='External monitor brightness, contrast and volume control'
arch=('x86_64')
url='https://github.com/sadesakaswl/brightless'
license=('GPL-3.0-only')
depends=('qt6-base' 'qt6-declarative' 'qt6-wayland' 'kglobalaccel' 'kstatusnotifieritem' 'libkscreen' 'ddcutil')
options=('!strip' '!debug')
package() { cp -a /stage/usr "\$pkgdir/"; }
EOF
    id -u builder >/dev/null 2>&1 || useradd --create-home builder
    chown -R builder:builder /build
    runuser -u builder -- makepkg --nodeps --noconfirm --force
    cp ./*.pkg.tar.zst /out/
fi
