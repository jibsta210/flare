// kshot -- screenshot capture that skips the XDG portal.
//
// Default flow: capture the workspace via KWin ScreenShot2, then show a
// fullscreen overlay to select a region and annotate. --full skips the overlay.

#include "capture.h"
#include "clipboard.h"
#include "overlay.h"
#include "telemetry.h"

#include <QApplication>
#include <QClipboard>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMimeData>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

static QString defaultPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) +
                        QStringLiteral("/Screenshots");
    QDir().mkpath(dir);
    return dir + QStringLiteral("/kshot-%1.png")
                     .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-hhmmss")));
}

// Hold the clipboard until something else takes it.
//
// On Wayland the clipboard is not a buffer you drop data into -- the owning
// process serves the bytes on demand. Exit too early and the paste finds
// nothing. Worse, QApplication quits by default the moment the last window
// closes, so a naive "set clipboard then close" loses the image every time,
// which is exactly the bug this replaces.
//
// KDE's klipper takes ownership shortly after a copy, and once it has, we are
// free to go. So poll for ownership transfer and leave as soon as it happens,
// falling back to a hard cap if no clipboard manager is running.
static void holdClipboardThenQuit(QApplication &app, int capMs = 4000)
{
    auto *elapsed = new QElapsedTimer;
    elapsed->start();
    auto *timer = new QTimer(&app);
    timer->setInterval(120);
    QObject::connect(timer, &QTimer::timeout, &app, [&app, timer, elapsed, capMs]() {
        const bool taken = !QGuiApplication::clipboard()->ownsClipboard();
        if (taken || elapsed->elapsed() > capMs) {
            timer->stop();
            delete elapsed;
            app.quit();
        }
    });
    timer->start();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kshot"));
    app.setApplicationVersion(QStringLiteral("0.3.0"));

    // Must be false: closing the overlay would otherwise terminate the process
    // before the clipboard hand-off completes.
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Screenshot via KWin ScreenShot2 -- no XDG portal round-trip."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption fullOpt(QStringLiteral("full"),
                               QStringLiteral("Capture the whole workspace, skip the editor."));
    QCommandLineOption cursorOpt(QStringLiteral("cursor"), QStringLiteral("Include the cursor."));
    QCommandLineOption outOpt({QStringLiteral("o"), QStringLiteral("output")},
                              QStringLiteral("Write to <file>."), QStringLiteral("file"));
    QCommandLineOption saveAlwaysOpt(QStringLiteral("save"),
                                     QStringLiteral("Always write a file, even on Copy."));
    // Wait before capturing. Needed to photograph transient UI -- menus,
    // tooltips, and kshot's own overlay, which cannot otherwise be captured
    // because it grabs the keyboard and a second instance would fight it.
    QCommandLineOption delayOpt(QStringLiteral("delay"),
                                QStringLiteral("Wait <seconds> before capturing."),
                                QStringLiteral("seconds"));
    parser.addOptions({fullOpt, cursorOpt, outOpt, saveAlwaysOpt, delayOpt});
    parser.process(app);

    QTextStream out(stdout);
    QTextStream err(stderr);

    if (parser.isSet(delayOpt)) {
        const double secs = parser.value(delayOpt).toDouble();
        if (secs > 0) {
            out << "kshot: capturing in " << secs << "s...\n";
            out.flush();
            // Spin the event loop rather than sleeping: a bare sleep would block
            // Qt from ever creating its Wayland connection, and the capture
            // would then fail on a technicality.
            QEventLoop wait;
            QTimer::singleShot(int(secs * 1000), &wait, &QEventLoop::quit);
            wait.exec();
        }
    }

    QElapsedTimer timer;
    timer.start();
    QString error;
    CaptureResult r = captureWorkspace(parser.isSet(cursorOpt), &error);
    const qint64 elapsed = timer.elapsed();

    if (!r.ok) {
        recordCapture(QStringLiteral("capture_fail"), elapsed, 0, error);
        err << "kshot: " << error << "\n";
        return 1;
    }
    recordCapture(QStringLiteral("capture_ok"), elapsed, r.bytes,
                  QStringLiteral("%1x%2").arg(r.image.width()).arg(r.image.height()));

    auto finish = [&](const QImage &img, bool save) {
        if (img.isNull()) {
            out << "kshot: cancelled\n";
            app.quit();
            return;
        }

        QElapsedTimer phase;
        phase.start();
        QString how;
        if (!copyImageToClipboard(img, &how))
            err << "kshot: clipboard copy failed\n";
        const qint64 clipMs = phase.elapsed();
        phase.restart();

        // Copy means copy. Save (or --save / -o) writes a file. The previous
        // build ORed in "not --clipboard-only", so Copy always saved too.
        const bool write = save || parser.isSet(outOpt) || parser.isSet(saveAlwaysOpt);
        if (write) {
            const QString path = parser.isSet(outOpt) ? parser.value(outOpt) : defaultPath();
            if (!img.save(path))
                err << "kshot: could not save to " << path << "\n";
            else
                out << "kshot: " << img.width() << "x" << img.height()
                    << " -> clipboard + " << path << "\n";
        } else {
            out << "kshot: " << img.width() << "x" << img.height() << " -> clipboard (" << how << ")\n";
        }
        recordCapture(QStringLiteral("deliver"), -1, 0,
                      QStringLiteral("via=%1").arg(how), clipMs, phase.elapsed(),
                      elapsed + clipMs + phase.elapsed());

        // wl-copy holds the selection in its own forked process, so we can go
        // as soon as the file is written. Only the Qt fallback needs us alive.
        if (how.startsWith(QStringLiteral("qt")))
            holdClipboardThenQuit(app);
        else
            QTimer::singleShot(0, &app, &QCoreApplication::quit);
    };

    if (parser.isSet(fullOpt)) {
        finish(r.image, true);   // headless: always write a file
        return app.exec();
    }

    Overlay *ov = new Overlay(r.image);
    QObject::connect(ov, &Overlay::finished, &app,
                     [&](const QImage &img, bool save) { finish(img, save); });
    ov->showFullScreen();
    ov->raise();
    ov->activateWindow();
    ov->setFocus(Qt::OtherFocusReason);
    // Belt and braces on Wayland: make sure keystrokes reach us even if the
    // compositor is slow to hand over focus.
    QTimer::singleShot(60, ov, [ov]() { ov->activateWindow(); ov->grabKeyboard(); });
    return app.exec();
}
