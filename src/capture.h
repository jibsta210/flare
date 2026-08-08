#pragma once

#include <QImage>
#include <QString>

struct CaptureResult {
    QImage image;
    qint64 bytes = 0;
    bool ok = false;
};

// Capture the whole workspace via KWin's ScreenShot2. On failure returns
// ok == false and fills *error with something actionable.
CaptureResult captureWorkspace(bool includeCursor, QString *error);
