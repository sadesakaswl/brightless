#include "platform/sdrbrightness.h"
#include "sdrbrightnesscontroller.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSemaphore>
#include <QThread>
#include <QVector>
#include <algorithm>
#include <climits>
#include <cstdlib>
#include <functional>
#include <iostream>

namespace {
struct FakeState {
    QMutex mutex;
    QVector<QPair<QString, int>> targets;
    QHash<QString, int> nits;
    QVector<QPair<QString, int>> writes;
    bool enumerateError = false;
    bool rejectWrites = false;
    bool blockNextWrite = false;
    int enumerateCalls = 0;
    int setCalls = 0;
};

FakeState fake;
QSemaphore writeEntered;
QSemaphore allowWrite;

brightless::sdr::Target target(const QPair<QString, int> &entry)
{
    return {QStringLiteral("key:") + entry.first, entry.first, 1, 2, 3, entry.second};
}

void check(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "Windows SDR brightness check failed: " << message << '\n';
        std::exit(1);
    }
}

bool waitFor(const std::function<bool()> &condition)
{
    QDeadlineTimer deadline(5000);
    while (!condition() && !deadline.hasExpired()) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return condition();
}

void settle()
{
    for (int i = 0; i < 40; ++i) {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
}

int readback(int index)
{
    QMutexLocker lock(&fake.mutex);
    return fake.nits.value(fake.targets.value(index).first);
}

int lastWrite()
{
    QMutexLocker lock(&fake.mutex);
    return fake.writes.isEmpty() ? -1 : fake.writes.last().second;
}

int writeCount()
{
    QMutexLocker lock(&fake.mutex);
    return fake.writes.size();
}

int setCalls()
{
    QMutexLocker lock(&fake.mutex);
    return fake.setCalls;
}

void reset(bool twoTargets = true)
{
    QMutexLocker lock(&fake.mutex);
    fake.targets = {{QStringLiteral("Display-A"), 200}};
    if (twoTargets) fake.targets.append({QStringLiteral("Display-B"), 300});
    fake.nits.clear();
    for (const auto &entry : fake.targets) fake.nits.insert(entry.first, entry.second);
    fake.writes.clear();
    fake.enumerateError = false;
    fake.rejectWrites = false;
    fake.blockNextWrite = false;
    fake.enumerateCalls = 0;
    fake.setCalls = 0;
}
} // namespace

namespace brightless::sdr {
std::vector<Target> enumerate(QString &error)
{
    QMutexLocker lock(&fake.mutex);
    ++fake.enumerateCalls;
    error.clear();
    if (fake.enumerateError) {
        error = QStringLiteral("The display configuration kept changing. Try again.");
        return {};
    }
    std::vector<Target> result;
    for (const auto &entry : fake.targets) {
        auto item = target(entry);
        item.nits = fake.nits.value(entry.first);
        result.push_back(item);
    }
    return result;
}

QString setBrightness(const Target &value, int nits)
{
    bool block = false;
    {
        QMutexLocker lock(&fake.mutex);
        ++fake.setCalls;
        block = fake.blockNextWrite;
        fake.blockNextWrite = false;
    }
    if (block) {
        writeEntered.release();
        allowWrite.acquire();
    }
    QMutexLocker lock(&fake.mutex);
    if (fake.rejectWrites) return QStringLiteral("Windows display error 87: Invalid parameter");
    const auto found = std::any_of(fake.targets.begin(), fake.targets.end(),
        [&](const auto &entry) { return target(entry).key == value.key; });
    if (!found) return QStringLiteral("The HDR display is no longer available. Refresh the display list.");
    const auto clamped = std::clamp(nits, 80, 480);
    fake.writes.append({value.name, clamped});
    fake.nits.insert(value.name, clamped);
    return {};
}
} // namespace brightless::sdr

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    check(brightless::sdr::whiteLevelToNits(1000) == 80, "1000 raw is 80 nits");
    check(brightless::sdr::whiteLevelToNits(3500) == 280, "3500 raw is 280 nits");
    check(brightless::sdr::whiteLevelToNits(6000) == 480, "6000 raw is 480 nits");
    check(brightless::sdr::whiteLevelToNits(UINT32_MAX) > 480, "large raw values do not underflow");
    check(brightless::sdr::nitsToWhiteLevel(INT_MIN) == 1000, "write clamps low");
    check(brightless::sdr::nitsToWhiteLevel(INT_MAX) == 6000, "write clamps high");
    for (int nits = 80; nits <= 480; ++nits) {
        check(brightless::sdr::whiteLevelToNits(brightless::sdr::nitsToWhiteLevel(nits)) == nits,
              "80-480 nit round trip");
    }

    {
        reset();
        SdrBrightnessController controller;
        check(controller.minimumBrightness() == 80 && controller.maximumBrightness() == 480,
              "Windows write range");
        controller.initialize();
        check(waitFor([&] { return controller.ready(); }), "initial discovery completes");
        check(controller.outputs() == QList<int>({1, 2}), "HDR outputs discovered");
        check(controller.name(1) == QStringLiteral("Display-A")
              && controller.name(2) == QStringLiteral("Display-B"), "display names");
        check(controller.brightness().value("1").toInt() == 200
              && controller.brightness().value("2").toInt() == 300, "reported white levels");
        check(writeCount() == 0 && controller.error().isEmpty(), "discovery never writes");

        {
            QMutexLocker lock(&fake.mutex);
            fake.nits.insert(QStringLiteral("Display-A"), 265);
        }
        controller.initialize();
        check(waitFor([&] { return controller.brightness().value("1").toInt() == 265; }),
              "external white-level changes are reflected");

        controller.setBrightness(1, 0);
        check(waitFor([&] { return lastWrite() == 80; }), "writes clamp to 80 nits");
        settle();
        check(controller.brightness().value("1").toInt() == 80, "readback after write");
        controller.setBrightness(2, 9999);
        check(waitFor([&] { return lastWrite() == 480; }), "writes clamp to 480 nits");
        settle();

        controller.setBrightness(1, 100);
        controller.setBrightness(1, 150);
        controller.setBrightness(1, 200);
        check(waitFor([&] { return lastWrite() == 200; }), "rapid changes settle on the newest value");
        settle();
        check(controller.brightness().value("1").toInt() == 200, "newest write wins");

        reset(false);
        controller.initialize();
        check(waitFor([&] { return controller.outputs() == QList<int>({1}); }),
              "disconnected monitor leaves the list");
        const int beforeRemoved = setCalls();
        controller.setBrightness(2, 160);
        QCoreApplication::processEvents();
        check(setCalls() == beforeRemoved, "stale output ids never write");

        reset();
        controller.initialize();
        check(waitFor([&] { return controller.outputs() == QList<int>({1, 2}); }),
              "returning monitor regains its id");
        controller.setBrightness(2, 240);
        check(waitFor([&] { return lastWrite() == 240; }), "returning monitor accepts writes");

        {
            QMutexLocker lock(&fake.mutex);
            fake.rejectWrites = true;
        }
        controller.setBrightness(1, 120);
        check(waitFor([&] { return !controller.error().isEmpty(); }), "setter failures are surfaced");
        const auto writeError = controller.error();
        bool refreshed = false;
        const auto refreshConnection = QObject::connect(&controller, &SdrBrightnessController::brightnessChanged,
            &controller, [&] { refreshed = true; });
        controller.initialize();
        check(waitFor([&] { return refreshed; }), "read-only refresh completes after failed write");
        QObject::disconnect(refreshConnection);
        check(controller.error() == writeError, "read-only refresh must preserve an unresolved write failure");
        {
            QMutexLocker lock(&fake.mutex);
            fake.rejectWrites = false;
        }
        controller.setBrightness(2, 280);
        check(waitFor([&] { return lastWrite() == 280; }), "another display still accepts writes");
        settle();
        check(controller.error() == writeError, "another display's successful write must not clear the failure");
        controller.setBrightness(1, 130);
        check(waitFor([&] { return controller.error().isEmpty(); }), "later success clears the error");

        {
            QMutexLocker lock(&fake.mutex);
            fake.enumerateError = true;
        }
        controller.initialize();
        check(waitFor([&] { return !controller.error().isEmpty(); }), "topology failures are surfaced");
        check(controller.outputs() == QList<int>({1, 2}), "topology failures keep the last display list");
        {
            QMutexLocker lock(&fake.mutex);
            fake.enumerateError = false;
        }
        controller.initialize();
        check(waitFor([&] { return controller.error().isEmpty(); }), "topology recovery clears the error");

        controller.setBrightness(99, 200);
        QCoreApplication::processEvents();
        check(controller.error().isEmpty(), "unknown ids are ignored");
    }

    {
        reset();
        auto *controller = new SdrBrightnessController;
        controller->initialize();
        check(waitFor([&] { return controller->ready(); }), "destruction fixture ready");
        controller->setBrightness(1, 250);
        check(writeCount() == 0, "write is still waiting for debounce");
        delete controller;
        check(writeCount() == 1 && readback(0) == 250, "immediate destruction flushes the debounced write");
        QCoreApplication::processEvents();
    }
    check(readback(0) == 250, "destruction waits for the pending write");

    for (const bool newerRequest : {false, true}) {
        reset(false);
        auto *controller = new SdrBrightnessController;
        controller->initialize();
        check(waitFor([&] { return controller->ready(); }), "in-flight destruction fixture ready");
        {
            QMutexLocker lock(&fake.mutex);
            fake.blockNextWrite = true;
        }
        controller->setBrightness(1, 250);
        check(waitFor([&] { return writeEntered.available() > 0; }), "older write is in flight");
        writeEntered.acquire();
        if (newerRequest) controller->setBrightness(1, 350);
        allowWrite.release();
        delete controller;
        check(readback(0) == (newerRequest ? 350 : 250), "shutdown leaves the newest requested value applied");
        check(writeCount() == (newerRequest ? 2 : 1), "shutdown does not duplicate an in-flight request");
        QCoreApplication::processEvents();
    }

    {
        reset();
        SdrBrightnessController controller;
        controller.initialize();
        check(waitFor([&] { return controller.ready(); }), "multiple write-errors fixture ready");
        {
            QMutexLocker lock(&fake.mutex);
            fake.rejectWrites = true;
        }
        controller.setBrightness(1, 120);
        controller.setBrightness(2, 140);
        check(waitFor([&] { return !controller.error().isEmpty(); }), "multiple failures are reported");
        check(controller.error().contains("Display-A"), "first failing output is reported consistently");
        {
            QMutexLocker lock(&fake.mutex);
            fake.rejectWrites = false;
        }
        controller.setBrightness(1, 130);
        check(waitFor([&] { return controller.error().contains("Display-B"); }), "retrying one output preserves the other failure");
        controller.setBrightness(2, 150);
        check(waitFor([&] { return controller.error().isEmpty(); }), "all successful retries clear the write errors");
    }
    std::cout << "Verified Windows SDR white-level discovery, conversion, writes, errors and lifecycle with fake transport.\n";
}
