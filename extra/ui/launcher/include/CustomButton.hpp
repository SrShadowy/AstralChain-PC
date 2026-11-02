#pragma once

#include <QWidget>
#include <QPushButton>
#include <QPropertyAnimation>
#include <QCursor>

class QTimer;
class QEnterEvent;

namespace ac::launcher {

class CustomButton : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal progress READ progress WRITE setProgress)
    Q_PROPERTY(qreal pulse READ pulse WRITE setPulse)

public:
    explicit CustomButton(const QString& text, QWidget* parent = nullptr, bool isPrimary = false);
    ~CustomButton() override = default;

    QPushButton* button() const { return m_button; }
    qreal progress() const { return m_progress; }
    void setProgress(qreal value);

    qreal pulse() const { return m_pulse; }
    void setPulse(qreal value) { m_pulse = qBound(0.0, value, 1.0); update(); }

    void setText(const QString& text) { m_button->setText(text); }
    QString text() const { return m_button->text(); }

    void setEnabled(bool enabled);
    bool isEnabled() const { return QWidget::isEnabled() && m_button->isEnabled(); }

    void setDangerMode(bool danger);
    bool isDangerMode() const { return m_isDanger; }
    void setCursor(const QCursor& cursor);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    void updateButtonPosition();
    void updateGeometryCalculations();
    void updateButtonStyle();
    void animate(bool hovered);

    QPushButton* m_button{nullptr};
    QPropertyAnimation* m_animation{nullptr};
    QPropertyAnimation* m_pulseAnimation{nullptr};
    QTimer* m_leaveTimer{nullptr};

    qreal m_progress{0.0};
    qreal m_pulse{0.0};
    bool m_isPrimary{false};
    bool m_isDanger{false};

    int m_buttonW{0};
    int m_buttonH{0};
    int m_buttonTop{0};
    int m_restX{0};
    int m_hoverX{0};
};

} // namespace ac::launcher
