#include "sdrbrightnesscontroller.h"

// Windows DDC is supported; HDR SDR-white-level control needs a separate supported API.
// An empty output list keeps unavailable controls out of QML without a KDE dependency.
SdrBrightnessController::SdrBrightnessController(QObject *parent) : QObject(parent) {}
void SdrBrightnessController::initialize() { ready_ = true; emit readyChanged(); }
QVariantMap SdrBrightnessController::brightness() const { return {}; }
QString SdrBrightnessController::name(int) const { return {}; }
void SdrBrightnessController::setBrightness(int, int) {}
