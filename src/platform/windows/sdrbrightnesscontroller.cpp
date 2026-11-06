#include "sdrbrightnesscontroller.h"
#include "platform/sdrbrightness.h"

#include <QDebug>
#include <QMap>
#include <QThreadPool>
#include <QTimer>
#include <algorithm>
#include <limits>

namespace {
struct BrightnessWrite {
    int outputId;
    brightless::sdr::Target target;
    int nits;
};

QMap<int, QString> writeValues(const std::vector<BrightnessWrite> &writes)
{
    QMap<int, QString> results;
    for (const auto &[id, target, nits] : writes) {
        const auto error = brightless::sdr::setBrightness(target, nits);
        results.insert(id, error.isEmpty() ? QString() : QStringLiteral("%1: %2").arg(target.name, error));
    }
    return results;
}
}

struct SdrBrightnessController::WindowsState {
    QThreadPool worker;
    QTimer refresh;
    QTimer write;
    std::vector<brightless::sdr::Target> discovered;
    QHash<int, brightless::sdr::Target> targets;
    QHash<QString, int> ids;
    QHash<int, int> values;
    QHash<int, int> inFlight;
    QMap<int, QString> writeErrors;
    int nextId = 1;
    bool refreshRequested = false;
};

SdrBrightnessController::SdrBrightnessController(QObject *parent)
    : QObject(parent), windows_(std::make_unique<WindowsState>())
{
    windows_->worker.setMaxThreadCount(1);
    windows_->refresh.setParent(this);
    windows_->refresh.setObjectName(QStringLiteral("sdrRefreshTimer"));
    windows_->refresh.setInterval(2000);
    connect(&windows_->refresh, &QTimer::timeout, this, &SdrBrightnessController::initialize);
    windows_->write.setParent(this);
    windows_->write.setObjectName(QStringLiteral("sdrWriteTimer"));
    windows_->write.setSingleShot(true);
    windows_->write.setInterval(100);
    connect(&windows_->write, &QTimer::timeout, this, &SdrBrightnessController::applyBrightness);
}

SdrBrightnessController::~SdrBrightnessController()
{
    windows_->refresh.stop();
    windows_->write.stop();
    std::vector<BrightnessWrite> writes;
    for (auto it = pending_.constBegin(); it != pending_.constEnd(); ++it) {
        const auto target = windows_->targets.constFind(it.key());
        if (target != windows_->targets.constEnd() && windows_->inFlight.value(it.key(), -1) != it.value()) {
            writes.push_back({it.key(), target.value(), it.value()});
        }
    }
    // The single worker finishes older requests before flushing the last debounced values.
    if (!writes.empty()) {
        windows_->worker.start([writes = std::move(writes)] {
            for (const auto &error : writeValues(writes)) {
                if (!error.isEmpty()) qWarning().noquote() << error;
            }
        });
    }
    windows_->worker.waitForDone();
}

int SdrBrightnessController::minimumBrightness() const { return 80; }
int SdrBrightnessController::maximumBrightness() const { return 480; }

void SdrBrightnessController::initialize()
{
    if (!windows_->refresh.isActive()) windows_->refresh.start();
    if (busy_) {
        windows_->refreshRequested = true;
        return;
    }
    applyBrightness();
}

QVariantMap SdrBrightnessController::brightness() const
{
    QVariantMap values;
    for (const int id : outputs_) {
        values.insert(QString::number(id), pending_.value(id, windows_->values.value(id)));
    }
    return values;
}

QString SdrBrightnessController::name(int outputId) const
{
    return windows_->targets.value(outputId).name;
}

void SdrBrightnessController::updateOutputs()
{
    QList<int> outputs;
    QHash<int, brightless::sdr::Target> targets;
    QHash<int, int> values;
    bool namesChanged = false;
    for (const auto &target : windows_->discovered) {
        int id = windows_->ids.value(target.key, 0);
        if (!id) {
            if (windows_->nextId == std::numeric_limits<int>::max()) continue;
            id = windows_->nextId++;
            windows_->ids.insert(target.key, id);
        }
        if (targets.contains(id)) continue;
        outputs.append(id);
        targets.insert(id, target);
        values.insert(id, target.nits);
        namesChanged |= name(id) != target.name;
    }
    std::sort(outputs.begin(), outputs.end());
    const bool changed = outputs_ != outputs || namesChanged;
    outputs_ = outputs;
    windows_->targets = std::move(targets);
    windows_->values = std::move(values);
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (!windows_->targets.contains(it.key())) it = pending_.erase(it);
        else ++it;
    }
    if (changed) emit outputsChanged();
    emit brightnessChanged();
}

void SdrBrightnessController::setBrightness(int outputId, int nits)
{
    if (!windows_->targets.contains(outputId)) return;
    pending_.insert(outputId, std::clamp(nits, minimumBrightness(), maximumBrightness()));
    windows_->write.start();
    emit brightnessChanged();
}

void SdrBrightnessController::applyBrightness()
{
    if (busy_) return;
    std::vector<BrightnessWrite> writes;
    const auto requested = pending_;
    for (auto it = requested.constBegin(); it != requested.constEnd(); ++it) {
        if (windows_->targets.contains(it.key())) {
            writes.push_back({it.key(), windows_->targets.value(it.key()), it.value()});
        }
    }
    busy_ = true;
    windows_->inFlight = requested;
    windows_->refreshRequested = false;
    windows_->worker.start([this, requested, writes = std::move(writes)] {
        const auto results = writeValues(writes);
        QString scanError;
        auto targets = brightless::sdr::enumerate(scanError);
        QMetaObject::invokeMethod(this, [this, requested, targets = std::move(targets), results, scanError] {
            windows_->inFlight.clear();
            for (auto it = results.constBegin(); it != results.constEnd(); ++it) {
                if (it.value().isEmpty()) windows_->writeErrors.remove(it.key());
                else windows_->writeErrors.insert(it.key(), it.value());
            }
            for (auto it = requested.constBegin(); it != requested.constEnd(); ++it) {
                if (pending_.value(it.key(), -1) == it.value()) pending_.remove(it.key());
            }
            if (scanError.isEmpty()) {
                windows_->discovered = targets;
                updateOutputs();
            } else {
                emit brightnessChanged();
            }
            const auto error = windows_->writeErrors.isEmpty() ? scanError : windows_->writeErrors.first();
            if (error_ != error) {
                error_ = error;
                emit errorChanged();
            }
            if (!ready_) {
                ready_ = true;
                emit readyChanged();
            }
            busy_ = false;
            if (!pending_.isEmpty()) {
                if (!windows_->write.isActive()) windows_->write.start();
            } else if (windows_->refreshRequested) {
                QTimer::singleShot(0, this, &SdrBrightnessController::initialize);
            }
        }, Qt::QueuedConnection);
    });
}
