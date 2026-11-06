#pragma once

#include <QtGlobal>
#ifdef BRIGHTLESS_WINDOWS_SDR_BRIGHTNESS
#include <memory>
#else
#include <KScreen/Config>
#endif
#include <QHash>
#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class SdrBrightnessController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QList<int> outputs READ outputs NOTIFY outputsChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantMap brightness READ brightness NOTIFY brightnessChanged)
    Q_PROPERTY(int minimumBrightness READ minimumBrightness CONSTANT)
    Q_PROPERTY(int maximumBrightness READ maximumBrightness CONSTANT)

public:
    explicit SdrBrightnessController(QObject *parent = nullptr);
    ~SdrBrightnessController() override;
    int minimumBrightness() const;
    int maximumBrightness() const;
    QList<int> outputs() const { return outputs_; }
    bool ready() const { return ready_; }
    QString error() const { return error_; }
    QVariantMap brightness() const;
    Q_INVOKABLE QString name(int outputId) const;
    Q_INVOKABLE void initialize();
    Q_INVOKABLE void setBrightness(int outputId, int nits);

signals:
    void outputsChanged();
    void readyChanged();
    void errorChanged();
    void brightnessChanged();

private:
    void updateOutputs();
    void applyBrightness();

#ifdef BRIGHTLESS_WINDOWS_SDR_BRIGHTNESS
    struct WindowsState;
    std::unique_ptr<WindowsState> windows_;
#else
    KScreen::ConfigPtr config_;
#endif
    QList<int> outputs_;
    QHash<int, int> pending_;
    QString error_;
    bool ready_ = false;
    bool busy_ = false;
};
