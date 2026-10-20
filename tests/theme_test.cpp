#include "theme.h"

#include <QApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickStyle>
#include <cmath>
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

double luminance(const QColor &color)
{
    const auto linear = [](double value) {
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF())
        + 0.0722 * linear(color.blueF());
}

void checkContrast(const QPalette &palette, QPalette::ColorRole foreground, QPalette::ColorRole background)
{
    const auto first = luminance(palette.color(foreground));
    const auto second = luminance(palette.color(background));
    check((qMax(first, second) + 0.05) / (qMin(first, second) + 0.05) >= 4.5,
          "text contrast is below 4.5:1");
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
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
    check(!brightless::isThemeMode(u"invalid"), "unknown theme accepted");
    check(brightless::themePalette(u"invalid").resolveMask() == 0, "unknown theme overrides system colors");
    for (const auto mode : {u"light", u"dark", u"amoled", u"light_green", u"dark_green",
                            u"light_blue", u"dark_blue", u"purple", u"red", u"orange", u"grey", u"brown", u"yellow",
                            u"light", u"auto", u"amoled", u"auto"}) {
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
            const bool dark = QStringView(mode) != u"light" && QStringView(mode) != u"light_green"
                && QStringView(mode) != u"light_blue" && QStringView(mode) != u"orange" && QStringView(mode) != u"grey"
                && QStringView(mode) != u"yellow";
            checkContrast(palette, QPalette::WindowText, QPalette::Window);
            checkContrast(palette, QPalette::Text, QPalette::Base);
            checkContrast(palette, QPalette::ButtonText, QPalette::Button);
            checkContrast(palette, QPalette::ToolTipText, QPalette::ToolTipBase);
            checkContrast(palette, QPalette::HighlightedText, QPalette::Highlight);
            if (QStringView(mode) == u"amoled") {
                for (const auto role : {QPalette::Window, QPalette::Base, QPalette::AlternateBase}) {
                    check(palette.color(role) == QColor(Qt::black), "AMOLED backgrounds must be pure black");
                }
            }
            check((palette.color(QPalette::Window).lightness() < 128) == dark, "wrong background mode");
            check((palette.color(QPalette::Text).lightness() > 128) == dark, "unreadable text");
            check(palette.color(QPalette::Disabled, QPalette::Text) != palette.color(QPalette::Active, QPalette::Text),
                  "disabled text is indistinguishable");
        }
    }
}
