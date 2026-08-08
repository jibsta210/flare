#pragma once

#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainterPath>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QVector>
#include <QWidget>

enum class Tool { Select, Rect, Ellipse, Arrow, Pen, Highlight, Text };
enum class Action { None, Copy, Save, Undo, Cancel };

// Which part of the selection a drag grabs. The selection stays adjustable
// after the first drag -- getting a crop exactly right in one motion is rare,
// and re-taking the whole screenshot to fix a few pixels is the main thing that
// makes a capture tool feel cheap.
enum class Handle {
    None, Move,
    TopLeft, Top, TopRight,
    Right, BottomRight, Bottom, BottomLeft, Left
};

struct Annotation {
    Tool tool = Tool::Rect;
    bool filled = false;     // solid shape rather than outline
    QRect rect;              // Rect / Ellipse / Arrow (p1 = topLeft, p2 = bottomRight)
    QPainterPath path;       // Pen / Highlight
    QString text;            // Text
    QPoint pos;              // Text anchor (baseline-left)
    QFont font;
    QColor color;
    int width = 3;
};

struct ToolbarItem {
    enum Kind { ToolItem, ActionItem, ColorItem, ToggleFill, Separator } kind = ToolItem;
    Tool tool = Tool::Select;
    Action action = Action::None;
    QColor color;            // ColorItem only
    const char *tip = "";
};

class Overlay : public QWidget
{
    Q_OBJECT
public:
    explicit Overlay(const QImage &shot, QWidget *parent = nullptr);

    QImage result() const { return m_result; }

signals:
    void finished(const QImage &image, bool save);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void inputMethodEvent(QInputMethodEvent *) override;

private:
    QImage composite() const;
    void accept(bool save);
    void commitText();
    QRect normalizedSelection() const;

    void drawToolbar(QPainter &p);
    void drawHandles(QPainter &p, const QRect &sel);
    QRect toolbarRect() const;
    QRect cellRect(int index) const;
    int itemAtPoint(const QPoint &pt) const;
    Handle handleAtPoint(const QPoint &pt) const;
    void applyHandleDrag(const QPoint &pos);
    void updateCursor(const QPoint &pos);

    QImage m_shot;
    QImage m_result;

    QPoint m_origin;
    QRect m_selection;
    bool m_selecting = false;
    bool m_haveSelection = false;

    // Selection adjustment
    Handle m_activeHandle = Handle::None;
    QRect m_dragStartSel;
    QPoint m_dragStartPos;

    Tool m_tool = Tool::Select;
    QColor m_accent;
    QColor m_drawColor;
    bool m_fill = false;
    int m_penWidth = 3;

    QVector<Annotation> m_annotations;
    Annotation m_pending;
    bool m_drawing = false;
    int m_hover = -1;

    // Text entry
    bool m_typing = false;
    QString m_textBuffer;
    QPoint m_textPos;
};
