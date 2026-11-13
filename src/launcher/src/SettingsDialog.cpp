#include "SettingsDialog.hpp"
#include "ConfigWatcher.hpp"
#include "MouseSettingsWidget.hpp"
#include "OverlayRpcSettingsWidget.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace ac::launcher {

namespace {
constexpr int kLiveDebounceMs = 80;
constexpr int kDialogWidth = 620;
constexpr int kDialogHeight = 560;
} // namespace

SettingsDialog::SettingsDialog(Config& config,
                               ConfigWatcher* watcher,
                               bool gameRunning,
                               QWidget* parent)
    : QDialog(parent)
    , m_config(config)
    , m_snapshot(config)
    , m_watcher(watcher) {
    setWindowTitle("IRIS // Configurações do Sistema");
    resize(kDialogWidth, kDialogHeight);

    m_liveSaveTimer = new QTimer(this);
    m_liveSaveTimer->setSingleShot(true);
    m_liveSaveTimer->setInterval(kLiveDebounceMs);
    connect(m_liveSaveTimer, &QTimer::timeout, this, &SettingsDialog::flushLiveSave);

    setupUi(gameRunning);
    loadFromConfig();
    wireLiveSignals();

    if (m_watcher) {
        connect(m_watcher, &ConfigWatcher::externalConfigChanged, this, &SettingsDialog::reloadExternal);
    }
}

void SettingsDialog::setupUi(bool gameRunning) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(createGameTab(), "  Jogo & Mouse  ");
    tabs->addTab(createGraphicsTab(), "  Gráficos & FSR 3.1  ");
    tabs->addTab(createAudioTab(), "  Áudio  ");
    tabs->addTab(createOverlayRpcTab(), "  HUD & Discord RPC  ");
    tabs->addTab(createAdvancedTab(), "  Avançado  ");
    mainLayout->addWidget(tabs);

    auto* buttonLayout = new QHBoxLayout();
    m_liveSyncCheck = new QCheckBox("Aplicar ao vivo enquanto o jogo roda", this);
    m_liveSyncCheck->setChecked(gameRunning);
    m_liveSyncCheck->setToolTip("Salva automaticamente no launcher.ini (80 ms) para o jogo e o terminal ImGui atualizarem na hora.");

    auto* cancelBtn = new QPushButton("Cancelar", this);
    auto* applyBtn = new QPushButton("Aplicar", this);
    auto* saveBtn = new QPushButton("Salvar Configurações", this);
    saveBtn->setObjectName("primaryBtn");

    buttonLayout->addWidget(m_liveSyncCheck);
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(applyBtn);
    buttonLayout->addWidget(saveBtn);
    mainLayout->addLayout(buttonLayout);

    connect(applyBtn, &QPushButton::clicked, this, &SettingsDialog::onApply);
    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSave);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::wireLiveSignals() {
    for (auto* cb : {m_parallelCheck, m_ignoreFocusCheck, m_xboxConfirmACheck, m_fullscreenCheck,
                     m_vsyncCheck, m_fpsUnlockedCheck, m_fsrJitterCheck, m_frameGenCheck,
                     m_noShadowCheck, m_muteUnfocusCheck, m_bc7Check, m_forceShadersCheck,
                     m_verboseLogCheck, m_cutscene30Check, m_asyncSubmitCheck}) {
        connect(cb, &QCheckBox::toggled, this, &SettingsDialog::onLiveControlModified);
    }
    for (auto* combo : {m_promptsCombo, m_languageCombo, m_voiceLangCombo, m_resolutionCombo,
                        m_presentModeCombo, m_scaleCombo, m_upscalerCombo, m_antialiasCombo,
                        m_textureQualityCombo, m_lodCombo}) {
        connect(combo, &QComboBox::currentIndexChanged, this, &SettingsDialog::onLiveControlModified);
    }
    connect(m_fpsSpin, &QSpinBox::valueChanged, this, &SettingsDialog::onLiveControlModified);
    connect(m_fsrSharpnessSpin, &QDoubleSpinBox::valueChanged, this, &SettingsDialog::onLiveControlModified);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &SettingsDialog::onLiveControlModified);
    connect(m_mouseWidget, &MouseSettingsWidget::modified, this, &SettingsDialog::onLiveControlModified);
    connect(m_overlayRpcWidget, &OverlayRpcSettingsWidget::modified, this, &SettingsDialog::onLiveControlModified);
    connect(m_wrapperEdit, &QLineEdit::textEdited, this, &SettingsDialog::onLiveControlModified);
    connect(m_customEnvEdit, &QLineEdit::textEdited, this, &SettingsDialog::onLiveControlModified);
}

void SettingsDialog::loadFromConfig() {
    m_loadingUi = true;
    const auto& g = m_config.graphics();
    const QString res = QString("%1x%2").arg(g.width).arg(g.height);
    int idx = m_resolutionCombo->findText(res);
    if (idx < 0) {
        m_resolutionCombo->addItem(res);
        idx = m_resolutionCombo->findText(res);
    }
    m_resolutionCombo->setCurrentIndex(idx);

    m_fullscreenCheck->setChecked(g.fullscreen);
    m_vsyncCheck->setChecked(g.vsync);
    int pIdx = m_presentModeCombo->findData(g.presentMode);
    if (pIdx < 0) {
        pIdx = m_presentModeCombo->findData(g.vsync ? "fifo" : "mailbox");
    }
    if (pIdx >= 0) {
        m_presentModeCombo->setCurrentIndex(pIdx);
    }
    if (!m_fpsSpin->hasFocus()) {
        m_fpsSpin->setValue(g.fpsLimit);
    }
    m_fpsUnlockedCheck->setChecked(g.fpsUnlocked || g.fpsLimit == 0 || g.fpsLimit > GraphicsSettings::kDefaultFpsLimit);
    m_upscalerCombo->setCurrentIndex(qMax(0, m_upscalerCombo->findData(g.upscaler)));
    m_antialiasCombo->setCurrentIndex(qMax(0, m_antialiasCombo->findData(g.antialias)));
    m_fsrJitterCheck->setChecked(g.fsrJitter);
    if (!m_fsrSharpnessSpin->hasFocus()) {
        m_fsrSharpnessSpin->setValue(g.fsrSharpness);
    }
    m_frameGenCheck->setChecked(g.frameGen);

    auto selectNearest = [](QComboBox* combo, double value) {
        int best = 0;
        double bestDelta = 1e18;
        for (int i = 0; i < combo->count(); ++i) {
            const double delta = qAbs(combo->itemData(i).toDouble() - value);
            if (delta < bestDelta) {
                bestDelta = delta;
                best = i;
            }
        }
        combo->setCurrentIndex(best);
    };
    selectNearest(m_scaleCombo, g.renderScale);
    selectNearest(m_textureQualityCombo, g.lodBias);
    selectNearest(m_lodCombo, g.lodMin);
    m_noShadowCheck->setChecked(g.noShadow);

    const auto& a = m_config.audio();
    m_volumeSlider->setValue(a.masterVolume);
    m_volumeLabel->setText(QString("%1%").arg(a.masterVolume));
    m_muteUnfocusCheck->setChecked(a.muteOnUnfocus);

    const auto& gm = m_config.game();
    m_parallelCheck->setChecked(gm.parallelThreads);
    m_ignoreFocusCheck->setChecked(gm.ignoreFocus);
    m_xboxConfirmACheck->setChecked(gm.xboxConfirmA);
    m_bc7Check->setChecked(gm.bc7Precompute);
    m_forceShadersCheck->setChecked(gm.forceShaders);
    m_verboseLogCheck->setChecked(gm.verboseLog);
    m_cutscene30Check->setChecked(gm.cutscene30Fps);
    m_asyncSubmitCheck->setChecked(gm.asyncSubmit);
    if (!m_wrapperEdit->hasFocus()) {
        m_wrapperEdit->setText(gm.commandWrapper);
    }
    if (!m_customEnvEdit->hasFocus()) {
        m_customEnvEdit->setText(gm.customEnv);
    }
    m_promptsCombo->setCurrentIndex(qMax(0, m_promptsCombo->findData(gm.buttonPrompts)));
    m_languageCombo->setCurrentIndex(qMax(0, m_languageCombo->findData(gm.language)));
    m_voiceLangCombo->setCurrentIndex(qMax(0, m_voiceLangCombo->findData(gm.voiceLanguage)));

    m_mouseWidget->loadFrom(m_config.mouse());
    m_overlayRpcWidget->loadFrom(m_config.overlay(), m_config.discordRpc());
    m_loadingUi = false;
}

void SettingsDialog::writeToConfig() {
    auto& g = m_config.graphics();
    const QStringList parts = m_resolutionCombo->currentText().split('x');
    if (parts.size() == 2) {
        g.width = parts[0].toInt();
        g.height = parts[1].toInt();
    }
    g.fullscreen = m_fullscreenCheck->isChecked();
    g.vsync = m_vsyncCheck->isChecked();
    g.presentMode = m_presentModeCombo->currentData().toString();
    g.fpsLimit = m_fpsSpin->value();
    g.fpsUnlocked = m_fpsUnlockedCheck->isChecked();
    g.renderScale = m_scaleCombo->currentData().toDouble();
    g.upscaler = m_upscalerCombo->currentData().toString();
    g.antialias = m_antialiasCombo->currentData().toString();
    g.fsrJitter = m_fsrJitterCheck->isChecked();
    g.fsrSharpness = m_fsrSharpnessSpin->value();
    g.frameGen = m_frameGenCheck->isChecked();
    g.lodBias = m_textureQualityCombo->currentData().toDouble();
    g.lodMin = m_lodCombo->currentData().toDouble();
    g.noShadow = m_noShadowCheck->isChecked();

    auto& a = m_config.audio();
    a.masterVolume = m_volumeSlider->value();
    a.muteOnUnfocus = m_muteUnfocusCheck->isChecked();

    m_mouseWidget->writeTo(m_config.mouse());
    m_overlayRpcWidget->writeTo(m_config.overlay(), m_config.discordRpc());

    auto& gm = m_config.game();
    gm.parallelThreads = m_parallelCheck->isChecked();
    gm.ignoreFocus = m_ignoreFocusCheck->isChecked();
    gm.xboxConfirmA = m_xboxConfirmACheck->isChecked();
    gm.buttonPrompts = m_promptsCombo->currentData().toString();
    gm.language = m_languageCombo->currentData().toString();
    gm.voiceLanguage = m_voiceLangCombo->currentData().toString();
    gm.bc7Precompute = m_bc7Check->isChecked();
    gm.forceShaders = m_forceShadersCheck->isChecked();
    gm.verboseLog = m_verboseLogCheck->isChecked();
    gm.cutscene30Fps = m_cutscene30Check->isChecked();
    gm.asyncSubmit = m_asyncSubmitCheck->isChecked();
    gm.commandWrapper = m_wrapperEdit->text().trimmed();
    gm.customEnv = m_customEnvEdit->text().trimmed();
}

void SettingsDialog::saveConfigNow() {
    if (m_watcher) {
        m_watcher->saveAndTrack();
    } else {
        m_config.save();
    }
}

void SettingsDialog::onLiveControlModified() {
    if (m_loadingUi || !m_liveSyncCheck->isChecked()) {
        return;
    }
    m_liveSaveTimer->start();
}

void SettingsDialog::flushLiveSave() {
    if (m_loadingUi || !m_liveSyncCheck->isChecked()) {
        return;
    }
    writeToConfig();
    saveConfigNow();
    m_liveCommittedDirty = true;
}

void SettingsDialog::reloadExternal() {
    m_liveSaveTimer->stop();
    loadFromConfig();
    if (!m_liveCommittedDirty) {
        m_snapshot = m_config;
    }
}

void SettingsDialog::onApply() {
    m_liveSaveTimer->stop();
    writeToConfig();
    saveConfigNow();
    m_snapshot = m_config;
    m_liveCommittedDirty = false;
}

void SettingsDialog::onSave() {
    onApply();
    accept();
}

void SettingsDialog::reject() {
    m_liveSaveTimer->stop();
    if (m_liveCommittedDirty) {
        m_config = m_snapshot;
        saveConfigNow();
    }
    QDialog::reject();
}

} // namespace ac::launcher
