#include "CustomButton.hpp"

#include <QPainter>
#include <QEvent>
#include <QMouseEvent>
#include <QColor>
#include <QPen>
#include <QTimer>
#include <QStyle>
#include <QEasingCurve>

namespace ac::launcher {

namespace {

constexpr int kFramePad = 6;
constexpr int kShadowX = 4;
constexpr int kShadowY = 5;
constexpr int kSlideDist = 20;

} // namespace

CustomButton::CustomButton(const QString& text, QWidget* parent, bool isPrimary)
    : QWidget(parent)
    , m_isPrimary(isPrimary)
{
    setAttribute(Qt::WA_TranslucentBackground);

    m_button = new QPushButton(text, this);
    m_button->setCursor(Qt::PointingHandCursor);
    m_button->installEventFilter(this);
    installEventFilter(this);
    setCursor(Qt::PointingHandCursor);

    connect(m_button, &QPushButton::clicked, this, &CustomButton::clicked);

    updateButtonStyle();

    m_animation = new QPropertyAnimation(this, "progress", this);
    m_animation->setDuration(160);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);

    m_pulseAnimation = new QPropertyAnimation(this, "pulse", this);
    m_pulseAnimation->setDuration(900);
    m_pulseAnimation->setStartValue(0.35);
    m_pulseAnimation->setKeyValueAt(0.5, 1.0);
    m_pulseAnimation->setEndValue(0.35);
    m_pulseAnimation->setLoopCount(-1);
    m_pulseAnimation->setEasingCurve(QEasingCurve::InOutSine);

    connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
        if (m_animation->endValue().toReal() <= 0.0) {
            m_pulseAnimation->stop();
            setPulse(0.0);
        }
    });

    m_leaveTimer = new QTimer(this);
    m_leaveTimer->setSingleShot(true);
    m_leaveTimer->setInterval(35);
    connect(m_leaveTimer, &QTimer::timeout, this, [this]() {
        if (!isEnabled()) {
            animate(false);
            return;
        }
        if (!underMouse() && !m_button->underMouse()) {
            animate(false);
        } else if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
            animate(false);
        }
    });

    updateGeometryCalculations();
}

void CustomButton::setProgress(qreal value)
{
    m_progress = qBound(0.0, value, 1.0);
    updateButtonPosition();
    update();
}

void CustomButton::setEnabled(bool enabled)
{
    QWidget::setEnabled(enabled);
    m_button->setEnabled(enabled);
    if (!enabled) {
        m_leaveTimer->stop();
        animate(false);
        m_pulseAnimation->stop();
        setPulse(0.0);
    }
    update();
}

void CustomButton::setDangerMode(bool danger)
{
    if (m_isDanger != danger) {
        m_isDanger = danger;
        updateButtonStyle();
        update();
    }
}

void CustomButton::setIcon(const QIcon& icon)
{
    m_icon = icon;
    if (m_icon.isNull()) {
        m_button->setIcon(QIcon());
        return;
    }
    constexpr int kButtonIconPx = 15;
    m_button->setIconSize(QSize(kButtonIconPx, kButtonIconPx));
    const QIcon::Mode mode = (m_isHovered && !m_isDanger) ? QIcon::Active : QIcon::Normal;
    m_button->setIcon(QIcon(m_icon.pixmap(kButtonIconPx, mode)));
}

void CustomButton::setCursor(const QCursor& cursor)
{
    QWidget::setCursor(cursor);
    if (m_button) m_button->setCursor(cursor);
}

void CustomButton::updateGeometryCalculations()
{
    const int W = width(), H = height();
    if (W <= 0 || H <= 0) return;

    m_buttonH = qMax(24, H - 2 * (kFramePad + 2));
    m_buttonTop = (H - m_buttonH) / 2;
    m_hoverX = kFramePad + 4;
    m_restX = m_hoverX + kSlideDist;

    const int maxButtonW = W - m_restX - kFramePad - kShadowX - 4;
    m_buttonW = qMax(40, maxButtonW);

    m_button->resize(m_buttonW, m_buttonH);
    updateButtonPosition();
}

void CustomButton::updateButtonPosition()
{
    const int x = qRound(m_restX + (m_hoverX - m_restX) * m_progress);
    m_button->move(x, m_buttonTop);
}

void CustomButton::updateButtonStyle()
{
    const int fontSize = m_isPrimary ? 11 : 10;
    const char* bg    = m_isDanger ? "#a31934" : (m_isPrimary ? "#194a8c" : "#12233c");
    const char* fg    = m_isDanger ? "#ffeef2" : (m_isPrimary ? "#eef9ff" : "#d8eaf4");
    const char* bar   = m_isDanger ? "#ff2e57" : (m_isPrimary ? "#00d4ff" : "#2b62b0");
    const char* hBg   = m_isDanger ? "#ff2e57" : (m_isPrimary ? "#00d4ff" : "#dff2fc");
    const char* hFg   = m_isDanger ? "#ffffff" : "#08101c";
    const char* pBg   = m_isDanger ? "#ff6b88" : "#59e8ff";
    const char* dBg   = m_isDanger ? "rgba(163, 25, 52, 0.30)" : "rgba(18, 35, 60, 0.40)";
    const char* dFg   = "rgba(125, 153, 181, 0.38)";

    m_button->setStyleSheet(QString(R"(
        QPushButton {
            background-color: %1; color: %2;
            border: 1px solid rgba(0, 212, 255, 0.24);
            border-left: 3px solid %3;
            border-radius: 2px;
            padding-left: 14px; padding-right: 14px; text-align: left;
            font-family: "Fira Sans Condensed ExtraBold", "Fira Sans", "Segoe UI", sans-serif;
            font-weight: 800; font-size: %4pt;
        }
        QPushButton[hovered="true"], QPushButton:hover {
            background-color: %5; color: %6; border-color: #59e8ff;
        }
        QPushButton:pressed { background-color: %7; color: %6; }
        QPushButton:disabled { background-color: %8; color: %9; border-color: rgba(125, 153, 181, 0.12); }
    )").arg(bg, fg, bar).arg(fontSize).arg(hBg, hFg, pBg, dBg, dFg));
}

void CustomButton::animate(bool hovered)
{
    m_isHovered = hovered;
    if (!m_icon.isNull()) {
        setIcon(m_icon);
    }

    m_button->setProperty("hovered", hovered);
    m_button->style()->unpolish(m_button);
    m_button->style()->polish(m_button);

    m_animation->stop();
    m_animation->setStartValue(m_progress);
    m_animation->setEndValue(hovered ? 1.0 : 0.0);
    m_animation->start();

    if (hovered) {
        if (m_pulseAnimation->state() != QAbstractAnimation::Running) {
            m_pulseAnimation->start();
        }
    } else if (m_progress <= 0.001) {
        m_pulseAnimation->stop();
        setPulse(0.0);
    }
}

bool CustomButton::eventFilter(QObject* watched, QEvent* event)
{
    if (!isEnabled()) return QWidget::eventFilter(watched, event);

    if (watched == m_button || watched == this) {
        if (event->type() == QEvent::Enter) {
            m_leaveTimer->stop();
            animate(true);
        } else if (event->type() == QEvent::Leave) {
            m_leaveTimer->start();
        } else if (event->type() == QEvent::MouseMove) {
            if (m_leaveTimer->isActive()) m_leaveTimer->stop();
            if (m_progress < 1.0 && m_animation->state() != QAbstractAnimation::Running) {
                animate(true);
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void CustomButton::enterEvent(QEnterEvent* event)
{
    QWidget::enterEvent(event);
    if (isEnabled()) {
        m_leaveTimer->stop();
        animate(true);
    }
}

void CustomButton::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    if (isEnabled()) m_leaveTimer->start();
}

void CustomButton::mousePressEvent(QMouseEvent* event)
{
    if (isEnabled() && event->button() == Qt::LeftButton) m_button->setDown(true);
    QWidget::mousePressEvent(event);
}

void CustomButton::mouseReleaseEvent(QMouseEvent* event)
{
    if (isEnabled() && event->button() == Qt::LeftButton) {
        m_button->setDown(false);
        if (rect().contains(event->pos())) m_button->animateClick();
    }
    QWidget::mouseReleaseEvent(event);
}

void CustomButton::hideEvent(QHideEvent* event)
{
    m_leaveTimer->stop();
    m_animation->stop();
    m_pulseAnimation->stop();
    setProgress(0.0);
    setPulse(0.0);
    QWidget::hideEvent(event);
}

void CustomButton::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::EnabledChange) {
        m_button->setEnabled(isEnabled());
        if (!isEnabled()) {
            m_leaveTimer->stop();
            animate(false);
            m_pulseAnimation->stop();
            setPulse(0.0);
        }
        update();
    }
    QWidget::changeEvent(event);
}

void CustomButton::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateGeometryCalculations();
}

void CustomButton::paintEvent(QPaintEvent*)
{
    if (m_progress <= 0.001) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QRectF buttonRect(m_button->geometry());
    const QRectF frame = buttonRect.adjusted(-kFramePad, -kFramePad, kFramePad, kFramePad);

    // 1. Projected shadow
    QColor shadowColor = m_isDanger ? QColor(24, 6, 8) : QColor(8, 10, 18);
    shadowColor.setAlphaF(0.75 * m_progress);
    painter.setPen(Qt::NoPen);
    painter.setBrush(shadowColor);
    painter.drawRoundedRect(frame.translated(kShadowX, kShadowY), 2.5, 2.5);

    // 2. Animated border
    QColor borderColor = m_isDanger ? QColor(255, 120, 120) : QColor(215, 230, 223);
    borderColor.setAlphaF(m_pulse * m_progress);
    painter.setPen(QPen(borderColor, 1.8));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(frame, 2.5, 2.5);

    // 3. Astral Chain IRIS corner accents
    QColor accentColor = m_isDanger ? QColor(255, 70, 70) : QColor(0, 220, 255);
    accentColor.setAlphaF(qBound(0.0, m_pulse * 1.15, 1.0) * m_progress);
    painter.setPen(QPen(accentColor, 2.2));

    constexpr qreal cLen = 7.0;
    const QPointF tl(frame.left(), frame.top()), br(frame.right(), frame.bottom());
    painter.drawLine(QPointF(tl.x(), tl.y() + cLen), tl);
    painter.drawLine(tl, QPointF(tl.x() + cLen, tl.y()));
    painter.drawLine(QPointF(br.x() - cLen, br.y()), br);
    painter.drawLine(br, QPointF(br.x(), br.y() - cLen));
}

} // namespace ac::launcher
