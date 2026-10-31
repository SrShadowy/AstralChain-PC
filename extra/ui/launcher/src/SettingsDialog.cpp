#include "SettingsDialog.hpp"
#include "Config.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace ac::launcher {

SettingsDialog::SettingsDialog(Config& config, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle("Astral Chain - Settings");
    resize(480, 410);
    setupUi();
    loadFromConfig();
}

void SettingsDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(createGameTab(), "Game");
    tabs->addTab(createGraphicsTab(), "Graphics");
    tabs->addTab(createAudioTab(), "Audio");
    tabs->addTab(createAdvancedTab(), "Advanced");
    mainLayout->addWidget(tabs);

    auto* buttonLayout = new QHBoxLayout();
    auto* saveBtn = new QPushButton("Save", this);
    auto* cancelBtn = new QPushButton("Cancel", this);
    buttonLayout->addStretch();
    buttonLayout->addWidget(saveBtn);
    buttonLayout->addWidget(cancelBtn);
    mainLayout->addLayout(buttonLayout);

    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSave);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QWidget* SettingsDialog::createGameTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    m_parallelCheck = new QCheckBox("Enable Parallel Thread Execution (Recommended)", page);
    m_ignoreFocusCheck = new QCheckBox("Keep Playing When Window Loses Focus", page);

    auto* form = new QFormLayout();
    m_promptsCombo = new QComboBox(page);
    m_promptsCombo->addItem("Automático (Gamepad = Switch, Teclado = PC)", "auto");
    m_promptsCombo->addItem("Nintendo Switch / Controle (Original)", "switch");
    m_promptsCombo->addItem("Teclado & Mouse (PC)", "keyboard");
    form->addRow("Ícones de Botões (Prompts):", m_promptsCombo);

    layout->addWidget(m_parallelCheck);
    layout->addWidget(m_ignoreFocusCheck);
    layout->addLayout(form);
    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createGraphicsTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QFormLayout(page);

    m_resolutionCombo = new QComboBox(page);
    m_resolutionCombo->addItems({"1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160"});

    m_fullscreenCheck = new QCheckBox("Exclusive / Borderless Fullscreen", page);
    m_vsyncCheck = new QCheckBox("Enable VSync", page);

    m_presentModeCombo = new QComboBox(page);
    m_presentModeCombo->addItem("FIFO (VSync Travado / Estável - Recomendado)", "fifo");
    m_presentModeCombo->addItem("Mailbox (Sem Tearing / Baixa Latência)", "mailbox");
    m_presentModeCombo->addItem("Immediate (Sem Sincronização / Tearing)", "immediate");

    m_fpsSpin = new QSpinBox(page);
    m_fpsSpin->setRange(30, 240);
    m_fpsSpin->setValue(60);

    m_scaleCombo = new QComboBox(page);
    m_scaleCombo->addItems({"0.5x", "0.75x", "1.0x (Native)", "1.25x", "1.5x", "2.0x"});

    m_upscalerCombo = new QComboBox(page);
    m_upscalerCombo->addItem("Bilinear (Padrão)", "bilinear");
    m_upscalerCombo->addItem("AMD FSR 1.0 (EASU + RCAS)", "fsr1");

    layout->addRow("Resolution:", m_resolutionCombo);
    layout->addRow("", m_fullscreenCheck);
    layout->addRow("", m_vsyncCheck);
    layout->addRow("Sync Mode:", m_presentModeCombo);
    layout->addRow("Target FPS:", m_fpsSpin);
    layout->addRow("Internal Scale:", m_scaleCombo);
    layout->addRow("Upscaler:", m_upscalerCombo);
    return page;
}

QWidget* SettingsDialog::createAudioTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QFormLayout(page);

    m_volumeSlider = new QSlider(Qt::Horizontal, page);
    m_volumeSlider->setRange(0, 100);
    m_volumeLabel = new QLabel("100%", page);

    auto* volLayout = new QHBoxLayout();
    volLayout->addWidget(m_volumeSlider);
    volLayout->addWidget(m_volumeLabel);

    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int val) {
        m_volumeLabel->setText(QString("%1%").arg(val));
    });

    m_muteUnfocusCheck = new QCheckBox("Mute Audio When Unfocused", page);

    layout->addRow("Master Volume:", volLayout);
    layout->addRow("", m_muteUnfocusCheck);
    return page;
}

QWidget* SettingsDialog::createAdvancedTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    m_bc7Check = new QCheckBox("Precompute BC7 Textures (Faster in-game loading, takes ~16GB)", page);
    m_forceShadersCheck = new QCheckBox("Force Full Shader Cache Recompilation On Launch", page);
    m_verboseLogCheck = new QCheckBox("Habilitar Log Detalhado do Motor (AC_VERBOSE=1)", page);
    m_cutscene30Check = new QCheckBox("Travar Cutscenes em 30 FPS nativo (AC_CUTSCENE_FPS=30 para A/V sync estrito)", page);
    m_asyncSubmitCheck = new QCheckBox("Submissão Gráfica Assíncrona (AC_ASYNC_SUBMIT=1 - Maior FPS)", page);

    layout->addWidget(m_bc7Check);
    layout->addWidget(m_forceShadersCheck);
    layout->addWidget(m_verboseLogCheck);
    layout->addWidget(m_cutscene30Check);
    layout->addWidget(m_asyncSubmitCheck);

    auto* form = new QFormLayout();
    m_wrapperEdit = new QLineEdit(page);
    m_wrapperEdit->setPlaceholderText("ex: mangohud, gamemoderun, gamescope");
    form->addRow("Command Wrapper (Exec):", m_wrapperEdit);

    m_customEnvEdit = new QLineEdit(page);
    m_customEnvEdit->setPlaceholderText("ex: MANGOHUD=1 RADV_PERFTEST=aco");
    form->addRow("Variáveis de Ambiente (Env):", m_customEnvEdit);
    layout->addLayout(form);

    layout->addStretch();
    return page;
}

void SettingsDialog::loadFromConfig()
{
    const auto& g = m_config.graphics();
    const QString res = QString("%1x%2").arg(g.width).arg(g.height);
    const int idx = m_resolutionCombo->findText(res);
    if (idx >= 0) m_resolutionCombo->setCurrentIndex(idx);

    m_fullscreenCheck->setChecked(g.fullscreen);
    m_vsyncCheck->setChecked(g.vsync);
    const int pIdx = m_presentModeCombo->findData(g.presentMode);
    if (pIdx >= 0) m_presentModeCombo->setCurrentIndex(pIdx);
    m_fpsSpin->setValue(g.fpsLimit);

    for (int i = 0; i < m_scaleCombo->count(); ++i) {
        if (m_scaleCombo->itemText(i).startsWith(QString::number(g.renderScale))) {
            m_scaleCombo->setCurrentIndex(i);
            break;
        }
    }

    const int upIdx = m_upscalerCombo->findData(g.upscaler);
    if (upIdx >= 0) m_upscalerCombo->setCurrentIndex(upIdx);

    const auto& a = m_config.audio();
    m_volumeSlider->setValue(a.masterVolume);
    m_volumeLabel->setText(QString("%1%").arg(a.masterVolume));
    m_muteUnfocusCheck->setChecked(a.muteOnUnfocus);

    const auto& gm = m_config.game();
    m_parallelCheck->setChecked(gm.parallelThreads);
    m_ignoreFocusCheck->setChecked(gm.ignoreFocus);
    m_bc7Check->setChecked(gm.bc7Precompute);
    m_forceShadersCheck->setChecked(gm.forceShaders);
    m_verboseLogCheck->setChecked(gm.verboseLog);
    m_cutscene30Check->setChecked(gm.cutscene30Fps);
    m_asyncSubmitCheck->setChecked(gm.asyncSubmit);
    m_wrapperEdit->setText(gm.commandWrapper);
    m_customEnvEdit->setText(gm.customEnv);
    const int prIdx = m_promptsCombo->findData(gm.buttonPrompts);
    if (prIdx >= 0) m_promptsCombo->setCurrentIndex(prIdx);
}

void SettingsDialog::writeToConfig()
{
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
    g.upscaler = m_upscalerCombo->currentData().toString();

    auto& a = m_config.audio();
    a.masterVolume = m_volumeSlider->value();
    a.muteOnUnfocus = m_muteUnfocusCheck->isChecked();

    auto& gm = m_config.game();
    gm.parallelThreads = m_parallelCheck->isChecked();
    gm.ignoreFocus = m_ignoreFocusCheck->isChecked();
    gm.buttonPrompts = m_promptsCombo->currentData().toString();
    gm.bc7Precompute = m_bc7Check->isChecked();
    gm.forceShaders = m_forceShadersCheck->isChecked();
    gm.verboseLog = m_verboseLogCheck->isChecked();
    gm.cutscene30Fps = m_cutscene30Check->isChecked();
    gm.asyncSubmit = m_asyncSubmitCheck->isChecked();
    gm.commandWrapper = m_wrapperEdit->text().trimmed();
    gm.customEnv = m_customEnvEdit->text().trimmed();

    m_config.save();
}

void SettingsDialog::onSave()
{
    writeToConfig();
    accept();
}

} // namespace ac::launcher
