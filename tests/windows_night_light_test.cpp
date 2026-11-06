#include "nightlightcontroller.h"
#include "nightlightschedule.h"
#include "platform/nightlight.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdlib>
#include <iostream>

namespace {
void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "Windows Night Light check failed: " << message << '\n';
        std::exit(1);
    }
}

bool failInitialize = false, failGet = false, failSet = false;
int initialized = 0, uninitialized = 0, writes = 0;
auto current = brightless::nightLightColorEffect(6500);

struct Fixture {
    QTemporaryDir directory;
    Fixture()
    {
        check(directory.isValid(), "isolated config directory");
        qputenv("BRIGHTLESS_TEST_CONFIG", directory.path().toUtf8());
        failInitialize = failGet = failSet = false;
        initialized = uninitialized = writes = 0;
        current = brightless::nightLightColorEffect(6500);
    }
    QString path() const { return directory.path() + "/brightless/night-light.json"; }
    QByteArray data() const
    {
        QFile file(path());
        check(file.open(QIODevice::ReadOnly), "read saved settings");
        return file.readAll();
    }
    void write(const QByteArray &data) const
    {
        check(QDir().mkpath(directory.path() + "/brightless"), "make fixture config directory");
        QFile file(path());
        check(file.open(QIODevice::WriteOnly) && file.write(data) == data.size(), "write fixture config");
    }
};

void tick(NightLightController &controller)
{
    auto *timer = controller.findChild<QTimer *>(QStringLiteral("nightLightScheduleTimer"));
    check(timer && QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection), "schedule timer tick");
}
}

namespace brightless::nightlight {
bool initialize() { ++initialized; return !failInitialize; }
void uninitialize() { ++uninitialized; }
unsigned long errorCode() { return 5; }
bool getColorEffect(ColorEffect &effect)
{
    if (failGet) return false;
    effect = current;
    return true;
}
bool setColorEffect(const ColorEffect &effect)
{
    if (failSet) return false;
    ++writes;
    current = effect;
    return true;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    {
        Fixture fixture;
        NightLightController controller;
        int reentrantAttempts = 0;
        const auto tryReentrantApply = [&] {
            if (controller.busy()) {
                ++reentrantAttempts;
                check(!controller.apply(controller.settings()), "signals cannot reenter incomplete initialization or apply");
            }
        };
        QObject::connect(&controller, &NightLightController::settingsChanged, &controller, tryReentrantApply);
        QObject::connect(&controller, &NightLightController::stateChanged, &controller, tryReentrantApply);
        controller.initialize();
        auto settings = controller.settings();
        settings["enabled"] = true;
        settings["mode"] = 2;
        check(controller.apply(settings) && writes == 1 && reentrantAttempts >= 3,
              "guarded reentrancy leaves a single valid applied effect");
    }
    {
        Fixture fixture;
        auto original = current;
        original[1] = 0.1f;
        current = original;
        {
            NightLightController controller;
            controller.initialize();
            check(controller.available() && controller.runsLocally() && !controller.sharedSchedule(), "local backend availability");
            check(writes == 0 && !controller.settings().value("enabled").toBool(), "startup must not change a disabled filter");
            auto settings = controller.settings();
            settings["mode"] = 2;
            settings["enabled"] = true;
            settings["temperature"] = 3500;
            check(controller.apply(settings), "enable always-on filter");
            check(current == brightless::nightLightColorEffect(3500), "requested matrix applied");
            check(QJsonDocument::fromJson(fixture.data()).object().value("enabled").toBool(), "enabled preference saved");
            const int before = writes;
            controller.initialize();
            tick(controller);
            check(initialized == 1 && writes == before, "initialization idempotent and identical effect not rewritten");
            const auto saved = fixture.data();
            for (const auto &change : {QVariantMap{{"temperature", 999}}, QVariantMap{{"dayTemperature", 6501}},
                                      QVariantMap{{"enabled", "false"}}, QVariantMap{{"automaticLocation", "false"}},
                                      QVariantMap{{"mode", 0}, {"automaticLocation", true}},
                                      QVariantMap{{"mode", 0}, {"latitude", 91.0}}}) {
                auto invalid = settings;
                for (auto it = change.begin(); it != change.end(); ++it) invalid[it.key()] = it.value();
                check(!controller.apply(invalid) && !controller.error().isEmpty(), "invalid preferences rejected");
                check(writes == before && fixture.data() == saved, "invalid preferences must not change filter or disk");
            }
            settings["enabled"] = false;
            check(controller.apply(settings) && current == original, "disable restores captured effect, not identity");
            settings["enabled"] = true;
            check(controller.apply(settings), "reenable");
        }
        check(current == original && uninitialized == 1, "destruction restores original effect and uninitializes");
        {
            NightLightController controller;
            // Saved preferences take effect without ever opening the Schedules UI.
            QCoreApplication::processEvents();
            check(controller.available() && current == brightless::nightLightColorEffect(3500), "startup resumes persisted schedule");
            check(controller.settings().value("temperature").toInt() == 3500, "temperature round trip");
            auto external = brightless::nightLightColorEffect(6500);
            external[0] = 0.8f;
            current = external;
            tick(controller);
            check(!controller.error().isEmpty() && current == external, "external filter takeover is not overwritten");
            check(!controller.findChild<QTimer *>("nightLightScheduleTimer")->isActive(), "conflict stops schedule updates");
        }
        check(current[0] == 0.8f && uninitialized == 2, "exit leaves another application's filter alone");
    }
    {
        Fixture fixture;
        NightLightController controller;
        controller.initialize();
        auto settings = controller.settings();
        settings["enabled"] = true;
        settings["mode"] = 2;
        failSet = true;
        check(!controller.apply(settings) && !controller.error().isEmpty() && writes == 0, "set API failure surfaced");
        failSet = false;
        check(controller.apply(settings) && writes == 1, "retry after set failure");
        const auto warm = current;
        settings["enabled"] = false;
        failSet = true;
        check(!controller.apply(settings) && current == warm && !controller.error().isEmpty(), "restore API failure surfaced");
        failSet = false;
        check(controller.apply(settings) && current == brightless::nightLightColorEffect(6500), "restore retry");
        settings["enabled"] = true;
        failGet = true;
        check(!controller.apply(settings) && !controller.error().isEmpty(), "get API failure surfaced");
        failGet = false;
    }
    {
        Fixture fixture;
        failInitialize = true;
        NightLightController controller;
        controller.initialize();
        check(!controller.available() && !controller.error().isEmpty() && uninitialized == 0, "initialize API failure");
        failInitialize = false;
        failGet = true;
        controller.initialize();
        check(!controller.available() && uninitialized == 1 && !controller.error().isEmpty(), "probe failure releases API");
        failGet = false;
        controller.initialize();
        check(controller.available(), "initialize retry");
    }
    for (const auto &invalid : {QByteArray("not JSON"), QByteArray("[]"), QByteArray("{}")}) {
        Fixture fixture;
        fixture.write(invalid);
        NightLightController controller;
        controller.initialize();
        check(controller.available() && !controller.error().isEmpty() && writes == 0
              && !controller.settings().value("enabled").toBool(), "corrupt preferences fail safely disabled");
        check(fixture.data() == invalid, "corrupt preferences preserved for recovery");
    }
    {
        Fixture fixture;
        QFile blocker(fixture.directory.path() + "/blocker");
        check(blocker.open(QIODevice::WriteOnly), "create save-path blocker");
        blocker.close();
        qputenv("BRIGHTLESS_TEST_CONFIG", blocker.fileName().toUtf8());
        NightLightController controller;
        controller.initialize();
        auto settings = controller.settings();
        settings["enabled"] = true;
        settings["mode"] = 2;
        check(!controller.apply(settings) && writes == 0 && !controller.settings().value("enabled").toBool()
              && !controller.error().isEmpty(), "failed atomic save must not alter current state or filter");
    }
    {
        Fixture fixture;
        const auto identity = current;
        NightLightController controller;
        controller.initialize();
        auto settings = controller.settings();
        settings["enabled"] = true;
        settings["mode"] = 2;
        check(controller.apply(settings), "enable before application shutdown");
        check(QMetaObject::invokeMethod(&app, "aboutToQuit", Qt::DirectConnection), "application shutdown signal");
        check(current == identity && uninitialized == 1, "aboutToQuit restores while Qt is still alive");
    }
    std::cout << "Verified Windows Night Light persistence, lifecycle, conflicts, validation and API failures with fake transport.\n";
}
