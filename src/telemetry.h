#pragma once
#include <QString>

// Append one telemetry line and fsync it. Never throws; telemetry must not
// break a capture.
void recordCapture(const QString &event, qint64 elapsedMs, qint64 bytes,
                   const QString &detail = QString());
