#include "MouseSettingsWidget.hpp"
#include "Config.hpp"

#include <QGroupBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>

namespace ac::launcher {

namespace {
constexpr int kMouseSensMin = 25;
constexpr int kMouseSensMax = 1000;
constexpr double kMouseYScaleMin = 0.20;
constexpr double kMouseYScaleMax = 3.00;
constexpr double kMouseYScaleStep = 0.05;
constexpr int kMouseMenuCellMin = 10;
constexpr int kMouseMenuCellMax = 200;
constexpr const char* kCurvePowerId = "power";
constexpr const char* kCurveLinearId = "linear";
constexpr const char* kCurveAggressiveId = "aggressive";
}

MouseSettingsWidget::MouseSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void MouseSettingsWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* group = new QGroupBox("Mouse & Câmera (PC)", this);
    auto* form = new QFormLayout(group);

    m_mouseLookCheck = new QCheckBox("Controlar Câmera com Mouse (F10 Alterna Captura)", group);

    auto* sensLayout = new QHBoxLayout();
    m_sensSlider = new QSlider(Qt::Horizontal, group);
    m_sensSlider->setRange(kMouseSensMin, kMouseSensMax);
    m_sensSpin = new QSpinBox(group);
    m_sensSpin->setRange(kMouseSensMin, kMouseSensMax);
    m_sensSpin->setSuffix("%");
    sensLayout->addWidget(m_sensSlider);
    sensLayout->addWidget(m_sensSpin);

    connect(m_sensSlider, &QSlider::valueChanged, m_sensSpin, &QSpinBox::setValue);
    connect(m_sensSpin, &QSpinBox::valueChanged, m_sensSlider, &QSlider::setValue);

    m_yScaleSpin = new QDoubleSpinBox(group);
    m_yScaleSpin->setRange(kMouseYScaleMin, kMouseYScaleMax);
    m_yScaleSpin->setSingleStep(kMouseYScaleStep);
    m_yScaleSpin->setSuffix("x");

    m_curveCombo = new QComboBox(group);
    m_curveCombo->addItem("Suave / Curva Natural (Power 1.35 - Recomendado)", kCurvePowerId);
    m_curveCombo->addItem("Linear (1:1)", kCurveLinearId);
    m_curveCombo->addItem("Agressiva (Power 1.75)", kCurveAggressiveId);

    m_invertCheck = new QCheckBox("Inverter Eixo Vertical (Look Down / Up)", group);
    m_menuNavCheck = new QCheckBox("Ponteiro de Menu (mouse navega e clica em menus)", group);
    m_menuMotionCheck = new QCheckBox("Passos Direcionais ao Mover o Mouse", group);

    m_menuCellSpin = new QSpinBox(group);
    m_menuCellSpin->setRange(kMouseMenuCellMin, kMouseMenuCellMax);
    m_menuCellSpin->setSuffix(" px");

    connect(m_menuNavCheck, &QCheckBox::toggled, m_menuMotionCheck, &QCheckBox::setEnabled);
    connect(m_menuNavCheck, &QCheckBox::toggled, m_menuCellSpin, &QSpinBox::setEnabled);

    auto notify = [this]() {
        if (!m_loading) {
            emit modified();
        }
    };
    connect(m_mouseLookCheck, &QCheckBox::toggled, this, notify);
    connect(m_sensSpin, &QSpinBox::valueChanged, this, notify);
    connect(m_yScaleSpin, &QDoubleSpinBox::valueChanged, this, notify);
    connect(m_curveCombo, &QComboBox::currentIndexChanged, this, notify);
    connect(m_invertCheck, &QCheckBox::toggled, this, notify);
    connect(m_menuNavCheck, &QCheckBox::toggled, this, notify);
    connect(m_menuMotionCheck, &QCheckBox::toggled, this, notify);
    connect(m_menuCellSpin, &QSpinBox::valueChanged, this, notify);

    form->addRow("", m_mouseLookCheck);
    form->addRow("Sensibilidade:", sensLayout);
    form->addRow("Escala Vertical (Y):", m_yScaleSpin);
    form->addRow("Curva de Resposta:", m_curveCombo);
    form->addRow("", m_invertCheck);
    form->addRow("", m_menuNavCheck);
    form->addRow("", m_menuMotionCheck);
    form->addRow("Passo de Menu:", m_menuCellSpin);

    layout->addWidget(group);
}

void MouseSettingsWidget::loadFrom(const MouseSettings& settings) {
    m_loading = true;
    m_mouseLookCheck->setChecked(settings.mouseLook);
    if (!m_sensSpin->hasFocus()) {
        m_sensSlider->setValue(static_cast<int>(settings.sensitivity));
        m_sensSpin->setValue(static_cast<int>(settings.sensitivity));
    }
    if (!m_yScaleSpin->hasFocus()) {
        m_yScaleSpin->setValue(settings.yScale);
    }
    int cIdx = m_curveCombo->findData(settings.curve);
    if (cIdx >= 0) {
        m_curveCombo->setCurrentIndex(cIdx);
    }
    m_invertCheck->setChecked(settings.invertY);
    m_menuNavCheck->setChecked(settings.menuNav);
    m_menuMotionCheck->setChecked(settings.menuMotion);
    m_menuMotionCheck->setEnabled(settings.menuNav);
    if (!m_menuCellSpin->hasFocus()) {
        m_menuCellSpin->setValue(settings.menuCell);
    }
    m_menuCellSpin->setEnabled(settings.menuNav);
    m_loading = false;
}

void MouseSettingsWidget::writeTo(MouseSettings& settings) const {
    settings.mouseLook = m_mouseLookCheck->isChecked();
    settings.sensitivity = m_sensSpin->value();
    settings.yScale = m_yScaleSpin->value();
    settings.curve = m_curveCombo->currentData().toString();
    settings.invertY = m_invertCheck->isChecked();
    settings.menuNav = m_menuNavCheck->isChecked();
    settings.menuMotion = m_menuMotionCheck->isChecked();
    settings.menuCell = m_menuCellSpin->value();
}

} // namespace ac::launcher
