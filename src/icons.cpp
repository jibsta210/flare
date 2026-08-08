// Vector toolbar icons, drawn with QPainter.
//
// WHY NOT QIcon::fromTheme
//   Two reasons. Qt6 has a long-standing habit of failing to resolve
//   hicolor/scalable icons, and a systemd-launched app can come up with no
//   platform theme at all -- in which case fromTheme() silently returns a null
//   icon and every button renders blank. That failure mode has cost real time on
//   this machine before. Drawing the glyphs directly cannot fail, needs no icon
//   theme, no resource file, and no install step.
//
//   Each icon is authored on a 24x24 grid and scaled into whatever cell it gets,
//   so the toolbar can be resized without redrawing artwork.

#include "icons.h"

#include <QPainter>
#include <QPainterPath>

namespace {

// Map the 24x24 authoring grid onto the target rect.
struct Grid {
    QRectF r;
    qreal s;
    explicit Grid(const QRectF &target) : r(target), s(target.width() / 24.0) {}
    QPointF p(qreal x, qreal y) const { return {r.x() + x * s, r.y() + y * s}; }
    QRectF box(qreal x, qreal y, qreal w, qreal h) const
    {
        return {r.x() + x * s, r.y() + y * s, w * s, h * s};
    }
};

} // namespace

void drawIcon(QPainter &p, IconId id, const QRectF &target, const QColor &color, qreal weight)
{
    const Grid g(target);
    QPen pen(color, qMax(1.4, weight * g.s), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (id) {
    case IconId::Crop: {
        // Two overlapping corner brackets -- reads as "choose a region".
        p.drawLine(g.p(7, 2), g.p(7, 17));
        p.drawLine(g.p(2, 7), g.p(17, 7));
        p.drawLine(g.p(17, 7), g.p(17, 22));
        p.drawLine(g.p(7, 17), g.p(22, 17));
        break;
    }
    case IconId::Rect:
        p.drawRoundedRect(g.box(3.5, 5, 17, 14), 2 * g.s, 2 * g.s);
        break;

    case IconId::Ellipse:
        p.drawEllipse(g.box(3.5, 5, 17, 14));
        break;

    case IconId::Arrow: {
        p.drawLine(g.p(4, 20), g.p(19, 5));
        QPainterPath head;
        head.moveTo(g.p(20, 4));
        head.lineTo(g.p(12.5, 5.5));
        head.lineTo(g.p(18.5, 11.5));
        head.closeSubpath();
        p.fillPath(head, color);
        break;
    }
    case IconId::Pen: {
        // A nib with a short stroke trailing from it.
        QPainterPath nib;
        nib.moveTo(g.p(15.5, 3.5));
        nib.lineTo(g.p(20.5, 8.5));
        nib.lineTo(g.p(9, 20));
        nib.lineTo(g.p(3.5, 21.5));
        nib.lineTo(g.p(5, 16));
        nib.closeSubpath();
        p.drawPath(nib);
        p.drawLine(g.p(14, 6), g.p(18, 10));
        break;
    }
    case IconId::Marker: {
        // Deliberately fatter than the pen so the two are distinguishable at
        // 20px, which is the whole point of having both.
        QPen thick(color, 5.0 * g.s, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        QColor faded = color;
        faded.setAlpha(150);
        thick.setColor(faded);
        p.setPen(thick);
        p.drawLine(g.p(4, 16), g.p(20, 8));
        p.setPen(pen);
        p.drawLine(g.p(3.5, 21), g.p(20.5, 21));
        break;
    }
    case IconId::Text: {
        // Serif "A" with a baseline -- unambiguous at 20px, where a plain glyph
        // rendered via drawText would be at the mercy of the ambient font.
        p.drawLine(g.p(5, 19), g.p(11.5, 5));
        p.drawLine(g.p(11.5, 5), g.p(18, 19));
        p.drawLine(g.p(7.8, 13.5), g.p(15.2, 13.5));
        break;
    }
    case IconId::Copy: {
        // Two offset sheets.
        p.drawRoundedRect(g.box(8, 3, 13, 14), 2 * g.s, 2 * g.s);
        QPainterPath back;
        back.moveTo(g.p(16, 20));
        back.lineTo(g.p(5, 20));
        back.lineTo(g.p(5, 7));
        p.drawPath(back);
        break;
    }
    case IconId::Save: {
        // Arrow descending into a tray -- clearer at small sizes than a floppy.
        p.drawLine(g.p(12, 3), g.p(12, 15));
        QPainterPath tip;
        tip.moveTo(g.p(12, 17));
        tip.lineTo(g.p(7.5, 11.5));
        tip.lineTo(g.p(16.5, 11.5));
        tip.closeSubpath();
        p.fillPath(tip, color);
        QPainterPath tray;
        tray.moveTo(g.p(4, 16));
        tray.lineTo(g.p(4, 20.5));
        tray.lineTo(g.p(20, 20.5));
        tray.lineTo(g.p(20, 16));
        p.drawPath(tray);
        break;
    }
    case IconId::Cancel:
        p.drawLine(g.p(5.5, 5.5), g.p(18.5, 18.5));
        p.drawLine(g.p(18.5, 5.5), g.p(5.5, 18.5));
        break;

    case IconId::Undo: {
        QPainterPath arc;
        arc.moveTo(g.p(5, 12));
        arc.arcTo(g.box(5, 6, 14, 12), 180, -230);
        p.drawPath(arc);
        QPainterPath tip;
        tip.moveTo(g.p(4, 6));
        tip.lineTo(g.p(10, 9.5));
        tip.lineTo(g.p(3.5, 13));
        tip.closeSubpath();
        p.fillPath(tip, color);
        break;
    }
    }
    p.restore();
}
