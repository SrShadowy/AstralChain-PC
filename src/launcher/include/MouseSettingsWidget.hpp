#pragma once

#include <QWidget>

class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;

namespace ac::launcher {

struct MouseSettings;

class MouseSettingsWidget : public QWidget {
    Q_OBJECT

public:
    explicit MouseSettingsWidget(QWidget* parent = nullptr);

    void loadFrom(const MouseSettings& settings);
    void writeTo(MouseSettings& settings) const;

signals:
    void modified();

private:
    void setupUi();

    QCheckBox* m_mouseLookCheck{nullptr};
    QSlider* m_sensSlider{nullptr};
    QSpinBox* m_sensSpin{nullptr};
    QDoubleSpinBox* m_yScaleSpin{nullptr};
    QComboBox* m_curveCombo{nullptr};
    QCheckBox* m_invertCheck{nullptr};
    QCheckBox* m_menuNavCheck{nullptr};
    QCheckBox* m_menuMotionCheck{nullptr};
    QSpinBox* m_menuCellSpin{nullptr};
    bool m_loading{false};
};

} // namespace ac::launcher
