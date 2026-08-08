#pragma once

#include <QColor>
#include <QRectF>

class QPainter;

enum class IconId { Crop, Rect, Ellipse, Arrow, Pen, Marker, Text, Copy, Save, Cancel, Undo };

// Draw an icon into `target`. Authored on a 24x24 grid and scaled, so cells can
// be any size. `weight` is the stroke width in grid units.
void drawIcon(QPainter &p, IconId id, const QRectF &target, const QColor &color,
              qreal weight = 1.8);
