#include "theme.h"

#include <QApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickStyle>
#include <cstdlib>
#include <iostream>
#include <memory>

namespace {
void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "Theme check failed: " << message << '\n';
        std::exit(1);
    }
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));
    const auto systemPalette = QApplication::palette();
    check(brightless::themePalette(u"auto").resolveMask() == 0, "auto must not override system colors");
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(R"(
        import QtQuick
        import QtQuick.Controls
        ApplicationWindow {
            id: window
            width: 240; height: 120
            readonly property color backgroundColor: palette.window
            readonly property color textColor: label.color
            readonly property color buttonColor: button.palette.button
            readonly property color osdColor: system.window
            Label { id: label; text: "Theme test" }
            Button { id: button; text: "Button"; y: 40 }
            SystemPalette { id: system }
        }
    )", QUrl());
    check(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> window(component.create());
    check(bool(window), "could not create test window");
    for (const auto mode : {u"light", u"dark", u"light", u"auto", u"dark", u"auto"}) {
        QApplication::setPalette(brightless::themePalette(mode));
        QApplication::processEvents();
        const auto palette = QApplication::palette();
        check(window->property("backgroundColor").value<QColor>() == palette.color(QPalette::Window),
              "window did not update immediately");
        check(window->property("textColor").value<QColor>() == palette.color(QPalette::WindowText),
              "label did not update immediately");
        check(window->property("buttonColor").value<QColor>() == palette.color(QPalette::Button),
              "button did not update immediately");
        check(window->property("osdColor").value<QColor>() == palette.color(QPalette::Window),
              "OSD system palette did not update immediately");
        if (QStringView(mode) == u"auto") {
            check(palette == systemPalette, "auto did not restore the system palette");
            check(!QApplication::testAttribute(Qt::AA_SetPalette), "auto still overrides system changes");
        } else {
            const bool dark = QStringView(mode) == u"dark";
            check((palette.color(QPalette::Window).lightness() < 128) == dark, "wrong background mode");
            check((palette.color(QPalette::Text).lightness() > 128) == dark, "unreadable text");
            check(palette.color(QPalette::Disabled, QPalette::Text) != palette.color(QPalette::Active, QPalette::Text),
                  "disabled text is indistinguishable");
        }
    }
}
