// Put an image on the Wayland clipboard, and make it stay there.
//
// THE PROBLEM
//   On Wayland the clipboard is not storage -- the owning client serves the
//   bytes on demand. A short-lived tool that sets the clipboard and exits leaves
//   nothing behind: the paste arrives after the owner is gone. Qt makes this
//   worse in two ways. QApplication quits by default when the last window
//   closes, killing the owner immediately; and setting a selection needs a
//   surface and an input serial, so a windowless run (--full) cannot set it at
//   all. Measured on this machine: after a --full capture the clipboard still
//   held the previous text/plain content.
//
// THE FIX
//   Hand the image to wl-copy, which forks a child whose entire job is to sit
//   there and serve the selection. That is the same mechanism every other
//   Wayland screenshot tool relies on, and it survives our exit by design.
//   Verified: 958157 bytes pasteable afterwards.
//
//   Qt's own clipboard remains as a fallback for a session without
//   wl-clipboard installed, where it at least works while the app is alive.

#include "clipboard.h"

#include <QBuffer>
#include <QByteArray>
#include <QClipboard>
#include <QGuiApplication>
#include <QImage>
#include <QMimeData>
#include <QProcess>
#include <QStandardPaths>

bool copyImageToClipboard(const QImage &image, QString *how)
{
    if (image.isNull())
        return false;

    QByteArray png;
    {
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        if (!image.save(&buf, "PNG")) {
            if (how) *how = QStringLiteral("PNG encode failed");
            return false;
        }
    }

    const QString wlCopy = QStandardPaths::findExecutable(QStringLiteral("wl-copy"));
    if (!wlCopy.isEmpty()) {
        QProcess p;
        p.setProgram(wlCopy);
        p.setArguments({QStringLiteral("--type"), QStringLiteral("image/png")});
        p.start();
        if (p.waitForStarted(3000)) {
            p.write(png);
            p.closeWriteChannel();
            // wl-copy forks a background holder and the foreground half exits
            // straight away, so this returns promptly rather than blocking for
            // as long as the clipboard is held.
            p.waitForFinished(5000);
            if (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0) {
                if (how) *how = QStringLiteral("wl-copy");
                return true;
            }
        }
        // Fall through to Qt rather than failing outright.
    }

    auto *mime = new QMimeData;
    mime->setImageData(image);
    mime->setData(QStringLiteral("image/png"), png);
    QGuiApplication::clipboard()->setMimeData(mime);
    if (how) *how = QStringLiteral("qt (only while flare runs)");
    return true;
}
