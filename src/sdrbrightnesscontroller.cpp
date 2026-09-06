#include "sdrbrightnesscontroller.h"

#include <KScreen/ConfigMonitor>
#include <KScreen/GetConfigOperation>
#include <KScreen/SetConfigOperation>

#include <algorithm>

namespace {
bool supportsSdrBrightness(const KScreen::OutputPtr &output)
{
    return output && output->isConnected() && output->isEnabled()
        && output->capabilities().testFlag(KScreen::Output::Capability::HighDynamicRange)
        && output->isHdrEnabled();
}
} // namespace

SdrBrightnessController::SdrBrightnessController(QObject *parent)
    : QObject(parent)
{
    connect(KScreen::ConfigMonitor::instance(), &KScreen::ConfigMonitor::configurationChanged,
            this, &SdrBrightnessController::updateOutputs);
}

void SdrBrightnessController::initialize()
{
    auto *operation = new KScreen::GetConfigOperation(KScreen::ConfigOperation::NoEDID, this);
    connect(operation, &KScreen::ConfigOperation::finished, this,
            [this](KScreen::ConfigOperation *result) {
        if (!result->hasError() && result->config()) {
            if (config_) {
                KScreen::ConfigMonitor::instance()->removeConfig(config_);
            }
            config_ = result->config();
            KScreen::ConfigMonitor::instance()->addConfig(config_);
            updateOutputs();
        }
        // Unsupported compositors simply have no SDR brightness controls.
        ready_ = true;
        emit readyChanged();
    });
}

QVariantMap SdrBrightnessController::brightness() const
{
    QVariantMap values;
    for (const auto id : outputs_) {
        values.insert(QString::number(id), pending_.value(id, config_->output(id)->sdrBrightness()));
    }
    return values;
}

QString SdrBrightnessController::name(int outputId) const
{
    const auto output = config_ ? config_->output(outputId) : KScreen::OutputPtr();
    return output ? output->name() : QString();
}

void SdrBrightnessController::updateOutputs()
{
    QList<int> outputs;
    if (config_) {
        for (const auto &output : config_->outputs()) {
            if (supportsSdrBrightness(output)) {
                outputs.append(output->id());
            }
        }
    }
    std::sort(outputs.begin(), outputs.end());
    if (outputs_ != outputs) {
        outputs_ = outputs;
        emit outputsChanged();
    }
    emit brightnessChanged();
}

void SdrBrightnessController::setBrightness(int outputId, int nits)
{
    if (!config_ || !supportsSdrBrightness(config_->output(outputId))) {
        return;
    }
    // KScreen's SDR white level is measured in nits, not DDC percentages.
    pending_.insert(outputId, std::clamp(nits, 50, 10000));
    if (!error_.isEmpty()) {
        error_.clear();
        emit errorChanged();
    }
    emit brightnessChanged();
    applyBrightness();
}

void SdrBrightnessController::applyBrightness()
{
    if (busy_ || pending_.isEmpty() || !config_) {
        return;
    }

    // Clone the live configuration so unrelated display settings stay intact.
    const auto config = config_->clone();
    QHash<int, int> writes;
    for (auto it = pending_.begin(); it != pending_.end();) {
        const auto output = config->output(it.key());
        if (!supportsSdrBrightness(output)) {
            it = pending_.erase(it);
            continue;
        }
        output->setSdrBrightness(it.value());
        writes.insert(it.key(), it.value());
        ++it;
    }
    if (writes.isEmpty()) {
        return;
    }

    // Only one display configuration request at a time; retain newer slider values.
    busy_ = true;
    auto *operation = new KScreen::SetConfigOperation(config, this);
    connect(operation, &KScreen::ConfigOperation::finished, this,
            [this, writes](KScreen::ConfigOperation *result) {
        for (auto it = writes.constBegin(); it != writes.constEnd(); ++it) {
            if (pending_.value(it.key(), -1) == it.value()) {
                pending_.remove(it.key());
            }
        }
        if (result->hasError()) {
            error_ = result->errorString();
            emit errorChanged();
        }
        busy_ = false;
        emit brightnessChanged();
        applyBrightness();
    });
}
