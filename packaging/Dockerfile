ARG BASE=debian:13
FROM ${BASE}
RUN if test -f /etc/debian_version; then \
      apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake ninja-build pkg-config python3 ca-certificates curl file \
        qt6-base-dev qt6-declarative-dev qt6-tools-dev qt6-base-dev-tools \
        libkf6globalaccel-dev libkf6statusnotifieritem-dev libkscreen-dev libkscreen-bin libddcutil-dev qt6-wayland \
        qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
        qml6-module-qtquick-window qml6-module-qtquick-templates qml6-module-qtqml-workerscript \
        dbus-daemon dpkg-dev zip patchelf desktop-file-utils; \
    elif test -f /etc/fedora-release; then \
      dnf install -y gcc-c++ cmake ninja-build pkgconf-pkg-config python3 git curl file \
        qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qttools-devel qt6-qtwayland \
        kf6-kglobalaccel-devel kf6-kstatusnotifieritem-devel libkscreen-devel \
        libddcutil-devel dbus-daemon rpm-build zip desktop-file-utils flatpak flatpak-builder; \
    else \
      pacman -Syu --noconfirm --needed base-devel cmake ninja python git curl \
        qt6-base qt6-declarative qt6-tools qt6-wayland kglobalaccel kstatusnotifieritem libkscreen ddcutil \
        dbus zip desktop-file-utils; \
    fi
WORKDIR /build
CMD ["bash", "/src/packaging/linux.sh"]
