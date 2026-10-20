#!/bin/bash
set -euo pipefail
format=${1:?Expected debian, fedora, or arch}
version=$(awk '/^project\(brightless VERSION / {print $3}' /src/CMakeLists.txt)
[[ $version =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]
mkdir -p /work /out
cd /work

if [[ $format == arch ]]; then
    useradd -m builder
    mkdir -p "archive/brightless-$version"
    cp -a /src/. "archive/brightless-$version/"
    tar -czf source.tar.gz -C archive "brightless-$version"
    checksum=$(sha256sum source.tar.gz | cut -d' ' -f1)
    sed -e "s/@VERSION@/$version/g" -e "s/@SHA256@/$checksum/g" /src/packaging/PKGBUILD > PKGBUILD
    chown -R builder:builder /work
    runuser -u builder -- makepkg --noconfirm
    package="brightless-$version-1-x86_64.pkg.tar.zst"
    pacman -U --noconfirm "$package"
    cp "$package" /out/
else
    case $format in debian|fedora) ;; *) echo "Unknown format: $format" >&2; exit 1 ;; esac
    cmake -S /src -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build build --parallel 4
    dbus-run-session -- ctest --test-dir build --output-on-failure
    desktop-file-validate /src/resources/com.brightless.Brightless.desktop
    appstreamcli validate --no-net /src/resources/com.brightless.Brightless.metainfo.xml
    DESTDIR=/work/stage cmake --install build
    cd build
    if [[ $format == debian ]]; then
        package="brightless_${version}-1_amd64.deb"
        cpack -G DEB -D "CPACK_PACKAGE_FILE_NAME=${package%.deb}" \
            -D "CPACK_DEBIAN_PACKAGE_VERSION=$version-1" -D CPACK_DEBIAN_PACKAGE_SHLIBDEPS=ON \
            -D 'CPACK_PACKAGE_CONTACT=Yusuf <34187260+sadesakaswl@users.noreply.github.com>' \
            -D 'CPACK_DEBIAN_PACKAGE_DEPENDS=qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-qtquick-window, qml6-module-qtquick-templates, qml6-module-qtqml-workerscript' \
            -D CPACK_PACKAGING_INSTALL_PREFIX=/usr
        dpkg -i "$package"
    else
        package="brightless-$version-1.fc44.x86_64.rpm"
        cpack -G RPM -D "CPACK_PACKAGE_FILE_NAME=${package%.rpm}" \
            -D CPACK_RPM_PACKAGE_RELEASE=1.fc44 -D CPACK_RPM_PACKAGE_LICENSE=GPL-3.0-only \
            -D CPACK_RPM_PACKAGE_REQUIRES=qt6-qtdeclarative -D CPACK_PACKAGING_INSTALL_PREFIX=/usr
        rpm -Uvh "$package"
    fi
    cp "$package" /out/
fi

dbus-run-session -- python3 /src/tests/app_smoke_test.py /usr/bin/brightless
if [[ $format == debian ]]; then
    bash /src/packaging/appimage.sh "$version"
fi
