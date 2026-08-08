// Capture backend: KWin's org.kde.KWin.ScreenShot2, no portal in the path.
//
// WHY NOT THE PORTAL
//   flameshot captures via org.freedesktop.portal.Desktop, so every screenshot
//   is: flameshot -> xdg-desktop-portal-kde -> KWin, three processes, a dmabuf
//   export, and a fresh ~165 MB flameshot process per shot. On this machine
//   hard freezes correlate with screenshot COUNT (20/15/27 on boots that froze,
//   2/9 on boots that shut down cleanly, including a clean 13.5 h boot), which
//   is the signature of something accumulating per capture.
//
//   ScreenShot2 takes a pipe fd and KWin writes raw pixels into it. One D-Bus
//   round trip, no portal, no dmabuf handed to us, no per-capture process.
//
// AUTHORIZATION
//   KWin gates this interface. It resolves the caller's PID to /proc/PID/exe,
//   searches installed .desktop files for one whose Exec matches that binary,
//   and reads X-KDE-DBUS-Restricted-Interfaces from it. Hence: a compiled
//   binary at a fixed path plus a matching .desktop file. A script would not
//   work -- its /proc/PID/exe is the interpreter, and authorizing /usr/bin/python3
//   would let every Python program on the system capture the screen silently.
//
// THE DEADLOCK TO AVOID
//   A pipe holds 64 KB; a 4K frame is ~35 MB. Issue the call and then read, and
//   KWin blocks writing to a full pipe while we block waiting for the reply --
//   forever. The reader thread therefore starts BEFORE the call is made.

#include "capture.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
#include <QVariantMap>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <thread>
#include <vector>

namespace {

constexpr const char *kService = "org.kde.KWin";
constexpr const char *kPath = "/org/kde/KWin/ScreenShot2";
constexpr const char *kIface = "org.kde.KWin.ScreenShot2";

// Drain to EOF. Runs on its own thread so KWin never stalls on a full pipe.
void drain(int fd, std::vector<char> *out, bool *ok)
{
    *ok = true;
    char buf[1 << 20];
    for (;;) {
        ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n > 0) {
            out->insert(out->end(), buf, buf + n);
        } else if (n == 0) {
            break;
        } else if (errno == EINTR) {
            continue;
        } else {
            *ok = false;
            break;
        }
    }
    ::close(fd);
}

} // namespace

CaptureResult captureWorkspace(bool includeCursor, QString *error)
{
    CaptureResult result;

    QDBusInterface iface(kService, kPath, kIface, QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        *error = QStringLiteral("KWin ScreenShot2 interface unavailable: %1")
                     .arg(iface.lastError().message());
        return result;
    }

    int fds[2];
    if (::pipe(fds) != 0) {
        *error = QStringLiteral("pipe() failed: %1").arg(QString::fromLocal8Bit(strerror(errno)));
        return result;
    }

    std::vector<char> pixels;
    bool readOk = false;
    std::thread reader(drain, fds[0], &pixels, &readOk); // start draining FIRST

    QVariantMap options;
    options.insert(QStringLiteral("include-cursor"), includeCursor);
    options.insert(QStringLiteral("native-resolution"), true);

    // 120 s: a 4K readback on a busy compositor can take a while, and a
    // spurious timeout here would look exactly like the freeze we are chasing.
    iface.setTimeout(120000);
    QDBusReply<QVariantMap> reply =
        iface.call(QStringLiteral("CaptureWorkspace"), options,
                   QVariant::fromValue(QDBusUnixFileDescriptor(fds[1])));

    // Our copy of the write end must close or the reader never sees EOF.
    // QDBusUnixFileDescriptor duplicated it, so this does not disturb KWin.
    ::close(fds[1]);
    reader.join();

    if (!reply.isValid()) {
        *error = QStringLiteral("%1: %2").arg(reply.error().name(), reply.error().message());
        if (reply.error().name().contains(QLatin1String("NoAuthorized"))) {
            *error += QStringLiteral(
                "\n\nKWin refused: this binary is not authorized for ScreenShot2.\n"
                "Install the .desktop file (make install) and launch flare from its\n"
                "installed path so KWin can match /proc/PID/exe to it.");
        }
        return result;
    }

    const QVariantMap meta = reply.value();
    const int width = meta.value(QStringLiteral("width")).toInt();
    const int height = meta.value(QStringLiteral("height")).toInt();
    const int stride = meta.value(QStringLiteral("stride")).toInt();
    const int format = meta.value(QStringLiteral("format")).toInt();

    if (width <= 0 || height <= 0 || stride <= 0) {
        *error = QStringLiteral("KWin returned unusable geometry: %1x%2 stride %3")
                     .arg(width).arg(height).arg(stride);
        return result;
    }
    if (!readOk) {
        *error = QStringLiteral("read from KWin's pipe failed");
        return result;
    }

    const qint64 expected = qint64(stride) * height;
    if (qint64(pixels.size()) < expected) {
        // A short read means a truncated capture. Rendering it anyway would
        // produce a half-torn image and quietly hide a real failure.
        *error = QStringLiteral("truncated capture: %1 of %2 bytes (%3x%4 stride %5)")
                     .arg(pixels.size()).arg(expected).arg(width).arg(height).arg(stride);
        return result;
    }

    // Copy out of the vector: QImage would otherwise alias memory we free.
    QImage img(reinterpret_cast<const uchar *>(pixels.data()), width, height, stride,
               static_cast<QImage::Format>(format));
    result.image = img.copy();
    result.bytes = qint64(pixels.size());
    result.ok = !result.image.isNull();
    if (!result.ok)
        *error = QStringLiteral("QImage rejected format %1").arg(format);
    return result;
}
