#pragma once
#include <QString>
class QImage;

// Put `image` on the clipboard so it survives this process exiting.
// Sets *how to the mechanism used ("wl-copy" or "qt ...").
bool copyImageToClipboard(const QImage &image, QString *how = nullptr);
