#pragma once

#include <QPalette>
#include "themepresets.h"

namespace brightless {
inline QPalette themePalette(QStringView mode)
{
    // An unresolved palette restores the current system palette, including future changes.
    const auto *preset = themePreset(mode);
    if (!preset) return QPalette();
    const bool dark = preset->dark;
    const QColor window(preset->window);
    const QColor text(dark ? "#eeeeee" : "#202020");
    const QColor base(preset->base);
    const QColor muted(dark ? "#909090" : "#767676");
    QPalette palette(text, QColor(preset->button),
                     QColor(dark ? "#4d4d4d" : "#ffffff"),
                     QColor(dark ? "#151515" : "#aaaaaa"),
                     QColor(dark ? "#777777" : "#b0b0b0"), text, Qt::white, base, window);
    palette.setColor(QPalette::AlternateBase, window);
    palette.setColor(QPalette::ToolTipBase, base);
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Highlight, QColor(preset->highlight));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, QColor(dark ? "#70b5ff" : "#1d4ed8"));
    palette.setColor(QPalette::LinkVisited, QColor(dark ? "#c4a5ff" : "#6d28d9"));
    palette.setColor(QPalette::PlaceholderText, muted);
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        palette.setColor(QPalette::Disabled, role, muted);
    }
    return palette;
}
}
