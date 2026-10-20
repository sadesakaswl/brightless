#pragma once

#include <QStringView>

namespace brightless {
struct ThemePreset {
    QStringView mode;
    bool dark;
    const char *window;
    const char *base;
    const char *button;
    const char *highlight;
};

inline constexpr ThemePreset themePresets[]{
    {u"light", false, "#f4f4f4", "#ffffff", "#e3e3e3", "#2563eb"},
    {u"dark", true, "#252525", "#181818", "#343434", "#2563eb"},
    {u"amoled", true, "#000000", "#000000", "#343434", "#2563eb"},
    {u"light_green", false, "#edf7ed", "#f8fff8", "#d5e8d5", "#28733c"},
    {u"dark_green", true, "#172b20", "#101e16", "#294534", "#28733c"},
    {u"light_blue", false, "#edf5fc", "#f8fcff", "#d4e5f4", "#215fba"},
    {u"dark_blue", true, "#19293d", "#111c2c", "#2a405b", "#215fba"},
    {u"purple", true, "#2d2138", "#201727", "#43314f", "#8052ad"},
    {u"red", true, "#382122", "#271617", "#503032", "#b53640"},
    {u"orange", false, "#fff2e5", "#fffaf5", "#f5d9bd", "#a84b0d"},
    {u"grey", false, "#e5e5e5", "#f5f5f5", "#cccccc", "#595959"},
    {u"brown", true, "#30261f", "#211a15", "#49392d", "#926439"},
    {u"yellow", false, "#fff9df", "#fffdf4", "#f2e6ad", "#806200"},
};

inline const ThemePreset *themePreset(QStringView mode)
{
    for (const auto &preset : themePresets) {
        if (preset.mode == mode) return &preset;
    }
    return nullptr;
}

inline bool isThemeMode(QStringView mode)
{
    return mode == u"auto" || themePreset(mode);
}
}
