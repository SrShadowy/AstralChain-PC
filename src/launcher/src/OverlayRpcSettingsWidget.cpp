#include "OverlayRpcSettingsWidget.hpp"
#include "Config.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QVBoxLayout>

namespace ac::launcher {

namespace {
constexpr double kOpacityStep = 0.05;
}

OverlayRpcSettingsWidget::OverlayRpcSettingsWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void OverlayRpcSettingsWidget::setupUi() {
    auto* layout = new QVBoxLayout(this);

    auto* hudGroup = new QGroupBox("HUD de Desempenho (Overlay)", this);
    auto* hudForm = new QFormLayout(hudGroup);

    m_hudEnabledCheck = new QCheckBox("Exibir HUD de Desempenho na Tela", hudGroup);
    m_hudModeCombo = new QComboBox(hudGroup);
    m_hudModeCombo->addItem("Compacto (FPS + Frametime)", 0);
    m_hudModeCombo->addItem("Detalhado (FPS + Gráfico + GPU + Swapchain)", 1);
    m_hudModeCombo->addItem("Mínimo (Apenas FPS)", 2);

    m_hudCornerCombo = new QComboBox(hudGroup);
    m_hudCornerCombo->addItem("Superior Esquerdo", 0);
    m_hudCornerCombo->addItem("Superior Direito", 1);
    m_hudCornerCombo->addItem("Inferior Esquerdo", 2);
    m_hudCornerCombo->addItem("Inferior Direito", 3);

    m_hudOpacitySpin = new QDoubleSpinBox(hudGroup);
    m_hudOpacitySpin->setRange(OverlaySettings::kMinOpacity, OverlaySettings::kMaxOpacity);
    m_hudOpacitySpin->setSingleStep(kOpacityStep);
    m_hudOpacitySpin->setDecimals(2);

    m_hudWithMenuCheck = new QCheckBox("Manter HUD visível enquanto o Menu Insert estiver aberto", hudGroup);

    hudForm->addRow("", m_hudEnabledCheck);
    hudForm->addRow("Modo do HUD:", m_hudModeCombo);
    hudForm->addRow("Posição na Tela:", m_hudCornerCombo);
    hudForm->addRow("Opacidade:", m_hudOpacitySpin);
    hudForm->addRow("", m_hudWithMenuCheck);

    auto* rpcGroup = new QGroupBox("Discord Rich Presence", this);
    auto* rpcForm = new QFormLayout(rpcGroup);

    m_rpcEnabledCheck = new QCheckBox("Ativar Discord Rich Presence", rpcGroup);
    m_rpcFileCheck = new QCheckBox("Mostrar Arquivo Atual (File)", rpcGroup);
    m_rpcMissionCheck = new QCheckBox("Mostrar Missão / Sub-ordem", rpcGroup);
    m_rpcAreaCheck = new QCheckBox("Mostrar Área / Setor", rpcGroup);
    m_rpcBossCheck = new QCheckBox("Mostrar Nome do Chefe", rpcGroup);
    m_rpcDiffCheck = new QCheckBox("Mostrar Dificuldade (PT Ultimate / Standard / Casual)", rpcGroup);
    m_rpcTimeCheck = new QCheckBox("Mostrar Tempo de Jogo na Sessão", rpcGroup);

    m_rpcClientIdEdit = new QLineEdit(rpcGroup);
    m_rpcClientIdEdit->setPlaceholderText(DiscordRpcSettings::kDefaultClientId);

    rpcForm->addRow("", m_rpcEnabledCheck);
    rpcForm->addRow("", m_rpcFileCheck);
    rpcForm->addRow("", m_rpcMissionCheck);
    rpcForm->addRow("", m_rpcAreaCheck);
    rpcForm->addRow("", m_rpcBossCheck);
    rpcForm->addRow("", m_rpcDiffCheck);
    rpcForm->addRow("", m_rpcTimeCheck);
    rpcForm->addRow("Application ID (Discord):", m_rpcClientIdEdit);

    auto notify = [this]() {
        if (!m_loading) {
            emit modified();
        }
    };
    for (auto* cb : {m_hudEnabledCheck, m_hudWithMenuCheck, m_rpcEnabledCheck,
                     m_rpcFileCheck, m_rpcMissionCheck, m_rpcAreaCheck,
                     m_rpcBossCheck, m_rpcDiffCheck, m_rpcTimeCheck}) {
        connect(cb, &QCheckBox::toggled, this, notify);
    }
    connect(m_hudModeCombo, &QComboBox::currentIndexChanged, this, notify);
    connect(m_hudCornerCombo, &QComboBox::currentIndexChanged, this, notify);
    connect(m_hudOpacitySpin, &QDoubleSpinBox::valueChanged, this, notify);
    connect(m_rpcClientIdEdit, &QLineEdit::textEdited, this, notify);

    layout->addWidget(hudGroup);
    layout->addWidget(rpcGroup);
    layout->addStretch();
}

void OverlayRpcSettingsWidget::loadFrom(const OverlaySettings& overlay, const DiscordRpcSettings& rpc) {
    m_loading = true;
    m_hudEnabledCheck->setChecked(overlay.perfHudEnabled);
    m_hudModeCombo->setCurrentIndex(m_hudModeCombo->findData(overlay.perfHudMode));
    m_hudCornerCombo->setCurrentIndex(m_hudCornerCombo->findData(overlay.perfHudCorner));
    if (!m_hudOpacitySpin->hasFocus()) {
        m_hudOpacitySpin->setValue(overlay.perfHudOpacity);
    }
    m_hudWithMenuCheck->setChecked(overlay.perfHudShowWithMenu);

    m_rpcEnabledCheck->setChecked(rpc.enabled);
    m_rpcFileCheck->setChecked(rpc.showFile);
    m_rpcMissionCheck->setChecked(rpc.showMission);
    m_rpcAreaCheck->setChecked(rpc.showArea);
    m_rpcBossCheck->setChecked(rpc.showBoss);
    m_rpcDiffCheck->setChecked(rpc.showDifficulty);
    m_rpcTimeCheck->setChecked(rpc.showTime);
    if (!m_rpcClientIdEdit->hasFocus()) {
        m_rpcClientIdEdit->setText(rpc.clientId);
    }
    m_loading = false;
}

void OverlayRpcSettingsWidget::writeTo(OverlaySettings& overlay, DiscordRpcSettings& rpc) const {
    overlay.perfHudEnabled = m_hudEnabledCheck->isChecked();
    overlay.perfHudMode = m_hudModeCombo->currentData().toInt();
    overlay.perfHudCorner = m_hudCornerCombo->currentData().toInt();
    overlay.perfHudOpacity = m_hudOpacitySpin->value();
    overlay.perfHudShowWithMenu = m_hudWithMenuCheck->isChecked();

    rpc.enabled = m_rpcEnabledCheck->isChecked();
    rpc.showFile = m_rpcFileCheck->isChecked();
    rpc.showMission = m_rpcMissionCheck->isChecked();
    rpc.showArea = m_rpcAreaCheck->isChecked();
    rpc.showBoss = m_rpcBossCheck->isChecked();
    rpc.showDifficulty = m_rpcDiffCheck->isChecked();
    rpc.showTime = m_rpcTimeCheck->isChecked();
    const QString cid = m_rpcClientIdEdit->text().trimmed();
    rpc.clientId = cid.isEmpty() ? DiscordRpcSettings::kDefaultClientId : cid;
}

} // namespace ac::launcher
