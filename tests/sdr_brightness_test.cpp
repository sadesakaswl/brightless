#include "sdrbrightnesscontroller.h"

#include <KScreen/GetConfigOperation>
#include <KScreen/Screen>
#include <KScreen/SetConfigOperation>
#include <QDeadlineTimer>
#include <QGuiApplication>
#include <QThread>

#include <functional>
#include <iostream>

bool waitFor(const std::function<bool()> &condition)
{
    QDeadlineTimer deadline(5000);
    while (!condition() && !deadline.hasExpired()) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return condition();
}

int main(int argc, char *argv[])
{
    // Never touch the real compositor or monitors, even on a developer's desktop.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("KSCREEN_BACKEND", "Fake");
    qputenv("KSCREEN_BACKEND_INPROCESS", "1");
    QGuiApplication app(argc, argv);

    auto config = KScreen::ConfigPtr::create();
    config->setScreen(KScreen::ScreenPtr::create());
    for (int id = 1; id <= 6; ++id) {
        auto output = KScreen::OutputPtr::create();
        output->setId(id);
        output->setName(QStringLiteral("DP-%1").arg(id));
        output->setConnected(id != 5);
        output->setEnabled(id != 3);
        output->setCapabilities(id == 4 ? KScreen::Output::Capabilities()
                                       : KScreen::Output::Capability::HighDynamicRange);
        output->setHdrEnabled(id != 2);
        output->setSdrBrightness(200);
        config->addOutput(output);
    }
    config->output(6)->setPos(QPoint(1920, 0));
    config->output(6)->setScale(1.5);
    if (!(new KScreen::SetConfigOperation(config))->exec()) {
        std::cerr << "KScreen Fake backend unavailable\n";
        return 77;
    }

    SdrBrightnessController controller;
    controller.initialize();
    if (!waitFor([&] { return controller.ready(); })
        || controller.outputs() != QList<int>{1, 6}
        || controller.name(1) != QStringLiteral("DP-1")) {
        return 1;
    }

    auto readConfig = [] {
        auto *get = new KScreen::GetConfigOperation(KScreen::ConfigOperation::NoEDID);
        if (!get->exec()) {
            return KScreen::ConfigPtr();
        }
        return get->config();
    };
    auto applied = [&](int nits) {
        return waitFor([&] {
            const auto current = readConfig();
            return current && current->output(1)->sdrBrightness() == nits;
        });
    };
    controller.setBrightness(1, -10);
    if (!applied(50)) {
        return 2;
    }
    controller.setBrightness(1, 20000);
    if (!applied(10000)) {
        return 3;
    }
    controller.setBrightness(1, 250);
    controller.setBrightness(1, 300);
    controller.setBrightness(1, 350);
    controller.setBrightness(6, 220);
    if (!applied(350)
        || !waitFor([&] {
            const auto current = readConfig();
            return current && current->output(6)->sdrBrightness() == 220;
        })) {
        return 4;
    }
    for (int id : {-1, 2, 3, 4, 5, 99}) {
        controller.setBrightness(id, 500);
    }
    config = readConfig();
    if (!config || config->output(2)->sdrBrightness() != 200
        || config->output(6)->pos() != QPoint(1920, 0)
        || config->output(6)->scale() != 1.5
        || !controller.error().isEmpty()) {
        return 5;
    }

    // External brightness and HDR changes must be reflected without restarting.
    config->output(1)->setSdrBrightness(400);
    if (!(new KScreen::SetConfigOperation(config))->exec()
        || !waitFor([&] { return controller.brightness().value("1").toInt() == 400; })) {
        return 6;
    }
    config->output(1)->setHdrEnabled(false);
    if (!(new KScreen::SetConfigOperation(config))->exec()
        || !waitFor([&] { return controller.outputs() == QList<int>{6}; })) {
        return 7;
    }
    controller.setBrightness(1, 800);
    config = readConfig();
    if (!config || config->output(1)->sdrBrightness() != 400
        || config->output(1)->isHdrEnabled()) {
        return 8;
    }
    config->output(1)->setHdrEnabled(true);
    if (!(new KScreen::SetConfigOperation(config))->exec()
        || !waitFor([&] { return controller.outputs() == QList<int>{1, 6}; })) {
        return 9;
    }
    std::cout << "SDR brightness detection, writes, bounds and external changes passed\n";
}
