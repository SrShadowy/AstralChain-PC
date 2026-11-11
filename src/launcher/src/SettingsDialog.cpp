#include "SettingsDialog.hpp"
#include "Config.hpp"
#include "MouseSettingsWidget.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace ac::launcher {

SettingsDialog::SettingsDialog(Config& config, QWidget* parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle("Astral Chain - Configurações");
    resize(480, 420);
    setupUi();
    loadFromConfig();
}

void SettingsDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* tabs = new QTabWidget(this);
    tabs->addTab(createGameTab(), "Jogo");
    tabs->addTab(createGraphicsTab(), "Gráficos");
    tabs->addTab(createAudioTab(), "Áudio");
    tabs->addTab(createAdvancedTab(), "Avançado");
    mainLayout->addWidget(tabs);

    auto* buttonLayout = new QHBoxLayout();
    auto* saveBtn = new QPushButton("Salvar", this);
    auto* cancelBtn = new QPushButton("Cancelar", this);
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

    m_parallelCheck = new QCheckBox("Execução Paralela de Threads (Recomendado)", page);
    m_ignoreFocusCheck = new QCheckBox("Continuar Executando ao Perder o Foco da Janela", page);

    auto* form = new QFormLayout();
    m_promptsCombo = new QComboBox(page);
    m_promptsCombo->addItem("Automático (Gamepad = Switch, Teclado = PC)", "auto");
    m_promptsCombo->addItem("Nintendo Switch / Controle (Original)", "switch");
    m_promptsCombo->addItem("Teclado & Mouse (PC)", "keyboard");
    form->addRow("Ícones de Botões (Prompts):", m_promptsCombo);

    m_languageCombo = new QComboBox(page);
    m_languageCombo->addItem("Automático (Padrão do Sistema / Host)", "auto");
    m_languageCombo->addItem("Português (Brasil) [Tradução]", "pt-BR");
    m_languageCombo->addItem("Inglês (en-US)", "en-US");
    m_languageCombo->addItem("Japonês (ja)", "ja");
    m_languageCombo->addItem("Espanhol (es)", "es");
    m_languageCombo->addItem("Francês (fr)", "fr");
    m_languageCombo->addItem("Alemão (de)", "de");
    m_languageCombo->addItem("Italiano (it)", "it");
    m_languageCombo->addItem("Russo (ru)", "ru");
    m_languageCombo->addItem("Chinês Simplificado (zh-CN)", "zh-CN");
    m_languageCombo->addItem("Chinês Tradicional (zh-TW)", "zh-TW");
    m_languageCombo->addItem("Coreano (ko)", "ko");
    m_languageCombo->addItem("Holandês (nl)", "nl");
    form->addRow("Idioma dos Textos:", m_languageCombo);

    m_voiceLangCombo = new QComboBox(page);
    m_voiceLangCombo->addItem("Automático (Acompanha Idioma)", "auto");
    m_voiceLangCombo->addItem("Japonês (ja)", "ja");
    m_voiceLangCombo->addItem("Inglês (en)", "en");
    form->addRow("Idioma das Vozes:", m_voiceLangCombo);

    m_mouseWidget = new MouseSettingsWidget(page);

    layout->addWidget(m_parallelCheck);
    layout->addWidget(m_ignoreFocusCheck);
    layout->addLayout(form);
    layout->addWidget(m_mouseWidget);
    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createGraphicsTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QFormLayout(page);

    m_resolutionCombo = new QComboBox(page);
    m_resolutionCombo->addItems({"1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160"});

    m_fullscreenCheck = new QCheckBox("Tela Cheia Exclusiva / Sem Bordas", page);
    m_vsyncCheck = new QCheckBox("Ativar VSync", page);

    m_presentModeCombo = new QComboBox(page);
    m_presentModeCombo->addItem("Mailbox (Sem Tearing / Baixa Latência - Recomendado)", "mailbox");
    m_presentModeCombo->addItem("FIFO (VSync Estrito / 60 Hz Travado)", "fifo");
    m_presentModeCombo->addItem("FIFO Relaxed (VSync Adaptativo / Sem Stutter)", "fifo_relaxed");
    m_presentModeCombo->addItem("Immediate (Sem Sincronização / Menor Latência)", "immediate");

    connect(m_presentModeCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        const QString mode = m_presentModeCombo->itemData(idx).toString();
        m_vsyncCheck->setChecked(mode != "immediate");
    });
    connect(m_vsyncCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (!checked && m_presentModeCombo->currentData().toString() == "fifo") {
            int idx = m_presentModeCombo->findData("mailbox");
            if (idx >= 0) m_presentModeCombo->setCurrentIndex(idx);
        } else if (checked && m_presentModeCombo->currentData().toString() == "immediate") {
            int idx = m_presentModeCombo->findData("fifo");
            if (idx >= 0) m_presentModeCombo->setCurrentIndex(idx);
        }
    });

    m_fpsSpin = new QSpinBox(page);
    m_fpsSpin->setRange(0, 240);
    m_fpsSpin->setSpecialValueText("0 (Ilimitado)");
    m_fpsSpin->setSingleStep(5);
    m_fpsSpin->setValue(60);

    // The game's own rate cap comes from the vendored Dynamic-FPS patch, which is compiled in (a
    // two-instruction difference), so this toggle changes the next build, not the running one.
    m_fpsUnlockedCheck = new QCheckBox(
        "Desbloquear o teto de 60 FPS do jogo (patch Dynamic-Unlocked; exige reconstruir)", page);
    m_fpsUnlockedCheck->setToolTip(
        "Sem isto o patch Dynamic-60fps fixa o timer do jogo em 60 FPS.\n"
        "Com isto o jogo usa o próprio passo medido, de 15 a 480 FPS.\n"
        "Target FPS continua sendo a taxa de apresentação (AC_FPS).");

    m_scaleCombo = new QComboBox(page);
    m_scaleCombo->addItem("0.50x (FSR Performance)", 0.50);
    m_scaleCombo->addItem("0.58x (FSR Balanced)", 0.58);
    m_scaleCombo->addItem("0.67x (FSR Quality)", 0.67);
    m_scaleCombo->addItem("0.75x (FSR Ultra Quality)", 0.75);
    m_scaleCombo->addItem("1.00x (Nativo / FSR 3.1 Native AA)", 1.00);
    m_scaleCombo->addItem("1.25x (Supersampling)", 1.25);
    m_scaleCombo->addItem("1.50x (Supersampling)", 1.50);
    m_scaleCombo->addItem("2.00x (Supersampling)", 2.00);

    m_upscalerCombo = new QComboBox(page);
    m_upscalerCombo->addItem("AMD FSR 3.1 (Temporal + Jitter + RCAS)", "fsr3");
    m_upscalerCombo->addItem("AMD FSR 1.0 (EASU + RCAS)", "fsr1");
    m_upscalerCombo->addItem("Bilinear (Desligado)", "bilinear");

    m_fsrJitterCheck = new QCheckBox("Camera Jitter Sub-Pixel Halton(2,3) (Nitidez Temporal)", page);
    m_fsrSharpnessSpin = new QDoubleSpinBox(page);
    m_fsrSharpnessSpin->setRange(0.0, 1.0);
    m_fsrSharpnessSpin->setSingleStep(0.05);
    m_fsrSharpnessSpin->setDecimals(2);
    m_frameGenCheck = new QCheckBox("AMD FSR 3.1 Frame Generation (Interpolação Óptica 2x FPS)", page);

    m_textureQualityCombo = new QComboBox(page);
    m_textureQualityCombo->addItem("Nativa (0)", 0.0);
    m_textureQualityCombo->addItem("Média (1)", 1.0);
    m_textureQualityCombo->addItem("Baixa (2)", 2.0);
    m_textureQualityCombo->addItem("Muito Baixa (3)", 3.0);

    m_lodCombo = new QComboBox(page);
    m_lodCombo->addItem("Desligado (0)", 0.0);
    m_lodCombo->addItem("1", 1.0);
    m_lodCombo->addItem("2", 2.0);
    m_lodCombo->addItem("3", 3.0);
    m_lodCombo->addItem("4", 4.0);

    m_noShadowCheck = new QCheckBox("Desativar Sombras (mais FPS)", page);

    layout->addRow("Resolução:", m_resolutionCombo);
    layout->addRow("", m_fullscreenCheck);
    layout->addRow("", m_vsyncCheck);
    layout->addRow("Modo de Sincronização:", m_presentModeCombo);
    layout->addRow("FPS Alvo:", m_fpsSpin);
    layout->addRow("", m_fpsUnlockedCheck);
    layout->addRow("Escala / Qualidade FSR:", m_scaleCombo);
    layout->addRow("Upscaler:", m_upscalerCombo);
    layout->addRow("", m_fsrJitterCheck);
    layout->addRow("Nitidez FSR 3.1 (RCAS):", m_fsrSharpnessSpin);
    layout->addRow("", m_frameGenCheck);
    layout->addRow("Qualidade de Textura (LOD Bias):", m_textureQualityCombo);
    layout->addRow("Nível de LoD Mínimo:", m_lodCombo);
    layout->addRow("", m_noShadowCheck);
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

    m_muteUnfocusCheck = new QCheckBox("Silenciar Áudio ao Perder o Foco", page);

    layout->addRow("Volume Principal:", volLayout);
    layout->addRow("", m_muteUnfocusCheck);
    return page;
}

QWidget* SettingsDialog::createAdvancedTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    m_bc7Check = new QCheckBox("Pré-computar Texturas BC7 (Carregamento mais rápido, ocupa ~16GB)", page);
    m_forceShadersCheck = new QCheckBox("Forçar Recompilação Completa de Shaders no Início", page);
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
    form->addRow("Comando Wrapper (Exec / MangoHud / GameMode):", m_wrapperEdit);

    m_customEnvEdit = new QLineEdit(page);
    m_customEnvEdit->setPlaceholderText("ex: MANGOHUD=1 RADV_PERFTEST=aco");
    form->addRow("Variáveis de Ambiente Adicionais (Env):", m_customEnvEdit);
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
    int pIdx = m_presentModeCombo->findData(g.presentMode);
    if (pIdx < 0) {
        pIdx = m_presentModeCombo->findData(g.vsync ? "fifo" : "mailbox");
    }
    if (pIdx >= 0) m_presentModeCombo->setCurrentIndex(pIdx);
    m_fpsSpin->setValue(g.fpsLimit);
    m_fpsUnlockedCheck->setChecked(g.fpsUnlocked);

    const int upIdx = m_upscalerCombo->findData(g.upscaler);
    if (upIdx >= 0) m_upscalerCombo->setCurrentIndex(upIdx);
    m_fsrJitterCheck->setChecked(g.fsrJitter);
    m_fsrSharpnessSpin->setValue(g.fsrSharpness);
    m_frameGenCheck->setChecked(g.frameGen);

    // nearest-preset selection for discrete graphics toggles (scale / bias / LoD)
    auto selectNearest = [](QComboBox* combo, double value) {
        int best = 0; double bestDelta = 1e18;
        for (int i = 0; i < combo->count(); ++i) {
            const double delta = qAbs(combo->itemData(i).toDouble() - value);
            if (delta < bestDelta) { bestDelta = delta; best = i; }
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
    m_bc7Check->setChecked(gm.bc7Precompute);
    m_forceShadersCheck->setChecked(gm.forceShaders);
    m_verboseLogCheck->setChecked(gm.verboseLog);
    m_cutscene30Check->setChecked(gm.cutscene30Fps);
    m_asyncSubmitCheck->setChecked(gm.asyncSubmit);
    m_wrapperEdit->setText(gm.commandWrapper);
    m_customEnvEdit->setText(gm.customEnv);
    const int prIdx = m_promptsCombo->findData(gm.buttonPrompts);
    if (prIdx >= 0) m_promptsCombo->setCurrentIndex(prIdx);
    const int langIdx = m_languageCombo->findData(gm.language);
    if (langIdx >= 0) m_languageCombo->setCurrentIndex(langIdx);
    const int voiceIdx = m_voiceLangCombo->findData(gm.voiceLanguage);
    if (voiceIdx >= 0) m_voiceLangCombo->setCurrentIndex(voiceIdx);

    if (m_mouseWidget) {
        m_mouseWidget->loadFrom(m_config.mouse());
    }
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
    g.fpsUnlocked = m_fpsUnlockedCheck->isChecked();
    g.renderScale = m_scaleCombo->currentData().toDouble();
    g.upscaler = m_upscalerCombo->currentData().toString();
    g.fsrJitter = m_fsrJitterCheck->isChecked();
    g.fsrSharpness = m_fsrSharpnessSpin->value();
    g.frameGen = m_frameGenCheck->isChecked();
    g.lodBias = m_textureQualityCombo->currentData().toDouble();
    g.lodMin = m_lodCombo->currentData().toDouble();
    g.noShadow = m_noShadowCheck->isChecked();

    auto& a = m_config.audio();
    a.masterVolume = m_volumeSlider->value();
    a.muteOnUnfocus = m_muteUnfocusCheck->isChecked();

    if (m_mouseWidget) {
        m_mouseWidget->writeTo(m_config.mouse());
    }

    auto& gm = m_config.game();
    gm.parallelThreads = m_parallelCheck->isChecked();
    gm.ignoreFocus = m_ignoreFocusCheck->isChecked();
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

    m_config.save();
}

void SettingsDialog::onSave()
{
    writeToConfig();
    accept();
}

} // namespace ac::launcher
