// Per-capture resource telemetry.
//
// WHY THIS IS IN THE APP RATHER THAN A SEPARATE SCRIPT
//   The freezes leave nothing behind -- no panic, no oops, no pstore entry, no
//   devcoredump. The only surviving signal is that freezes track screenshot
//   COUNT, not uptime. That shape says "something accumulates per capture", so
//   the app records the candidates immediately before and after every capture.
//   Then the leak question is answered by reading a slope instead of argued
//   about.
//
//   kwin_wayland is non-dumpable, so its /proc/PID/fd is root-owned and we
//   cannot count its fds from a user process. What we CAN see is our own fd
//   count and kwin's RSS, plus timing -- and a capture that starts taking
//   visibly longer is itself an early warning. The root watchdog service
//   records kwin's fd and dmabuf counts on the same timeline.
//
//   Written with fsync: the event being recorded is a hard power-off, which is
//   exactly what discards buffered writes.

#include "telemetry.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <unistd.h>

namespace {

QString logPath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
        QStringLiteral("/kshot");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/captures.jsonl");
}

int ownFdCount()
{
    QDir d(QStringLiteral("/proc/self/fd"));
    return d.entryList(QDir::Files | QDir::Dirs | QDir::System | QDir::NoDotAndDotDot).count();
}

QString bootId()
{
    QFile f(QStringLiteral("/proc/sys/kernel/random/boot_id"));
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromLatin1(f.readAll()).trimmed();
}

} // namespace

void recordCapture(const QString &event, qint64 elapsedMs, qint64 bytes, const QString &detail,
                   qint64 clipboardMs, qint64 saveMs, qint64 totalMs)
{
    QJsonObject o;
    o.insert(QStringLiteral("iso"), QDateTime::currentDateTime().toString(Qt::ISODate));
    o.insert(QStringLiteral("event"), event);
    o.insert(QStringLiteral("boot_id"), bootId());
    o.insert(QStringLiteral("elapsed_ms"), elapsedMs);
    o.insert(QStringLiteral("bytes"), bytes);
    o.insert(QStringLiteral("self_fds"), ownFdCount());
    o.insert(QStringLiteral("path"), QStringLiteral("screenshot2"));
    if (!detail.isEmpty())
        o.insert(QStringLiteral("detail"), detail);
    // The capture is only one phase. The clipboard hand-off blocks on wl-copy
    // forking its holder process, and that was previously untimed -- so a stall
    // there was invisible while "capture was fast" looked like proof it wasn't
    // the clipboard. Measure every phase or measure nothing.
    if (clipboardMs >= 0) o.insert(QStringLiteral("clipboard_ms"), clipboardMs);
    if (saveMs >= 0)      o.insert(QStringLiteral("save_ms"), saveMs);
    if (totalMs >= 0)     o.insert(QStringLiteral("total_ms"), totalMs);

    QFile f(logPath());
    if (!f.open(QIODevice::Append | QIODevice::WriteOnly))
        return;
    f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    f.write("\n");
    f.flush();
    ::fsync(f.handle()); // must survive a hard power-off
    f.close();
}
