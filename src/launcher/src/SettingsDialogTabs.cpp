#include "SettingsDialog.hpp"
#include "MouseSettingsWidget.hpp"
#include "OverlayRpcSettingsWidget.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace ac::launcher {

QWidget* SettingsDialog::createGameTab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    m_parallelCheck = new QCheckBox("Execução Paralela de Threads (Recomendado - Reinício)", page);
    m_ignoreFocusCheck = new QCheckBox("Continuar Executando ao Perder o Foco da Janela (Ao vivo)", page);
    m_xboxConfirmACheck = new QCheckBox("Confirmar / Ação com Botão A no Controle (Inverter B -> A)", page);

    auto* form = new QFormLayout();
    m_promptsCombo = new QComboBox(page);
    m_promptsCombo->addItem("Automático (Xbox / Switch / Teclado)", "auto");
    m_promptsCombo->addItem("Controle Xbox (Padrão PC)", "xbox");
    m_promptsCombo->addItem("Nintendo Switch (Original)", "switch");
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
    form->addRow("Idioma dos Textos (Reinício):", m_languageCombo);

    m_voiceLangCombo = new QComboBox(page);
    m_voiceLangCombo->addItem("Automático (Acompanha Idioma)", "auto");
    m_voiceLangCombo->addItem("Japonês (ja)", "ja");
    m_voiceLangCombo->addItem("Inglês (en)", "en");
    form->addRow("Idioma das Vozes:", m_voiceLangCombo);

    m_mouseWidget = new MouseSettingsWidget(page);
    layout->addWidget(m_parallelCheck);
    layout->addWidget(m_ignoreFocusCheck);
    layout->addWidget(m_xboxConfirmACheck);
    layout->addLayout(form);
    layout->addWidget(m_mouseWidget);
    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::createGraphicsTab() {
    auto* page = new QWidget(this);
    auto* layout = new QFormLayout(page);

    m_resolutionCombo = new QComboBox(page);
    m_resolutionCombo->addItems({"1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160"});
    m_fullscreenCheck = new QCheckBox("Tela Cheia Exclusiva / Sem Bordas (F11)", page);
    m_vsyncCheck = new QCheckBox("Ativar VSync", page);

    m_presentModeCombo = new QComboBox(page);
    m_presentModeCombo->addItem("Mailbox (Sem Tearing / Baixa Latência - Recomendado)", "mailbox");
    m_presentModeCombo->addItem("FIFO (VSync Estrito / Travado)", "fifo");
    m_presentModeCombo->addItem("FIFO Relaxed (VSync Adaptativo / Sem Stutter)", "fifo_relaxed");
    m_presentModeCombo->addItem("Immediate (Sem Sincronização / Menor Latência)", "immediate");

    connect(m_presentModeCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (m_loadingUi) {
            return;
        }
        m_vsyncCheck->setChecked(m_presentModeCombo->itemData(idx).toString() != "immediate");
    });
    connect(m_vsyncCheck, &QCheckBox::toggled, this, [this](bool checked) {
        if (m_loadingUi) {
            return;
        }
        const QString cur = m_presentModeCombo->currentData().toString();
        if (!checked && cur == "fifo") {
            m_presentModeCombo->setCurrentIndex(m_presentModeCombo->findData("mailbox"));
        } else if (checked && cur == "immediate") {
            m_presentModeCombo->setCurrentIndex(m_presentModeCombo->findData("fifo"));
        }
    });

    m_fpsSpin = new QSpinBox(page);
    m_fpsSpin->setRange(0, GraphicsSettings::kMaxFpsLimit);
    m_fpsSpin->setSpecialValueText("0 (Ilimitado)");
    m_fpsSpin->setSingleStep(GraphicsSettings::kFpsSpinStep);

    m_fpsUnlockedCheck = new QCheckBox(
        "Desbloquear teto de 60 FPS do jogo (Patch Dynamic-Unlocked - Requer Reconstruir)", page);
    connect(m_fpsSpin, &QSpinBox::valueChanged, this, [this](int val) {
        if (!m_loadingUi && (val == 0 || val > GraphicsSettings::kDefaultFpsLimit)) {
            m_fpsUnlockedCheck->setChecked(true);
        }
    });

    m_scaleCombo = new QComboBox(page);
    m_scaleCombo->addItem("0.50x (FSR Performance)", 0.50);
    m_scaleCombo->addItem("0.58x (FSR Balanced)", 0.58);
    m_scaleCombo->addItem("0.67x (FSR Quality)", 0.67);
    m_scaleCombo->addItem("0.75x (FSR Ultra Quality)", 0.75);
    m_scaleCombo->addItem("1.00x (Nativo / FSR 3.1 Native AA)", 1.00);
    m_scaleCombo->addItem("1.25x (SSAA Qualidade)", 1.25);
    m_scaleCombo->addItem("1.50x (SSAA Ultra)", 1.50);
    m_scaleCombo->addItem("1.75x (SSAA Extremo)", 1.75);
    m_scaleCombo->addItem("2.00x (SSAA 4K)", 2.00);

    m_upscalerCombo = new QComboBox(page);
    m_upscalerCombo->addItem("AMD FSR 3.1 (Temporal + Jitter + RCAS)", "fsr3");
    m_upscalerCombo->addItem("AMD FSR 1.0 (EASU + RCAS)", "fsr1");
    m_upscalerCombo->addItem("Bilinear (Desligado)", "bilinear");

    m_antialiasCombo = new QComboBox(page);
    m_antialiasCombo->addItem("SMAA Ultra (Color Edge)", "smaa");
    m_antialiasCombo->addItem("Desativado", "off");

    m_fsrJitterCheck = new QCheckBox("Camera Jitter Sub-Pixel Halton(2,3) (Nitidez Temporal)", page);
    m_fsrSharpnessSpin = new QDoubleSpinBox(page);
    m_fsrSharpnessSpin->setRange(GraphicsSettings::kMinFsrSharpness, GraphicsSettings::kMaxFsrSharpness);
    m_fsrSharpnessSpin->setSingleStep(GraphicsSettings::kFsrSharpnessStep);
    m_fsrSharpnessSpin->setDecimals(GraphicsSettings::kFsrSharpnessDecimals);
    m_frameGenCheck = new QCheckBox("AMD FSR 3.1 Frame Generation (Interpolação Óptica 2x FPS)", page);

    m_textureQualityCombo = new QComboBox(page);
    m_textureQualityCombo->addItem("Nativa (0 - novos samplers)", 0.0);
    m_textureQualityCombo->addItem("Média (1)", 1.0);
    m_textureQualityCombo->addItem("Baixa (2)", 2.0);
    m_textureQualityCombo->addItem("Muito Baixa (3)", 3.0);

    m_lodCombo = new QComboBox(page);
    m_lodCombo->addItem("Desligado (0 - novos samplers)", 0.0);
    m_lodCombo->addItem("1", 1.0);
    m_lodCombo->addItem("2", 2.0);
    m_lodCombo->addItem("3", 3.0);
    m_lodCombo->addItem("4", 4.0);

    m_noShadowCheck = new QCheckBox("Desativar Sombras (aplica a novos pipelines / mais FPS)", page);

    layout->addRow("Resolução:", m_resolutionCombo);
    layout->addRow("", m_fullscreenCheck);
    layout->addRow("", m_vsyncCheck);
    layout->addRow("Modo de Sincronização:", m_presentModeCombo);
    layout->addRow("FPS Alvo:", m_fpsSpin);
    layout->addRow("", m_fpsUnlockedCheck);
    layout->addRow("Escala / SSAA:", m_scaleCombo);
    layout->addRow("Upscaler:", m_upscalerCombo);
    layout->addRow("Antialiasing:", m_antialiasCombo);
    layout->addRow("", m_fsrJitterCheck);
    layout->addRow("Nitidez FSR (RCAS):", m_fsrSharpnessSpin);
    layout->addRow("", m_frameGenCheck);
    layout->addRow("Qualidade de Textura (LOD Bias):", m_textureQualityCombo);
    layout->addRow("Nível de LoD Mínimo:", m_lodCombo);
    layout->addRow("", m_noShadowCheck);
    return page;
}

QWidget* SettingsDialog::createAudioTab() {
    auto* page = new QWidget(this);
    auto* layout = new QFormLayout(page);

    m_volumeSlider = new QSlider(Qt::Horizontal, page);
    m_volumeSlider->setRange(AudioSettings::kMinVolume, AudioSettings::kDefaultVolume);
    m_volumeLabel = new QLabel("100%", page);
    auto* volLayout = new QHBoxLayout();
    volLayout->addWidget(m_volumeSlider);
    volLayout->addWidget(m_volumeLabel);

    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int val) {
        m_volumeLabel->setText(QString("%1%").arg(val));
    });
    m_muteUnfocusCheck = new QCheckBox("Silenciar Áudio ao Perder o Foco (Ao vivo)", page);

    layout->addRow("Volume Principal:", volLayout);
    layout->addRow("", m_muteUnfocusCheck);
    return page;
}

QWidget* SettingsDialog::createOverlayRpcTab() {
    m_overlayRpcWidget = new OverlayRpcSettingsWidget(this);
    return m_overlayRpcWidget;
}

QWidget* SettingsDialog::createAdvancedTab() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    m_bc7Check = new QCheckBox("Pré-computar Texturas BC7 (~16GB - Reinício)", page);
    m_forceShadersCheck = new QCheckBox("Forçar Recompilação Completa de Shaders (Próxima Compilação)", page);
    m_verboseLogCheck = new QCheckBox("Habilitar Log Detalhado do Motor (AC_VERBOSE=1 - Reinício)", page);
    m_cutscene30Check = new QCheckBox("Travar Cutscenes em 30 FPS nativo (A/V sync estrito - Ao vivo)", page);
    m_asyncSubmitCheck = new QCheckBox("Submissão Gráfica Assíncrona (Desligar ao vivo é one-way)", page);

    layout->addWidget(m_bc7Check);
    layout->addWidget(m_forceShadersCheck);
    layout->addWidget(m_verboseLogCheck);
    layout->addWidget(m_cutscene30Check);
    layout->addWidget(m_asyncSubmitCheck);

    auto* form = new QFormLayout();
    m_wrapperEdit = new QLineEdit(page);
    m_wrapperEdit->setPlaceholderText("ex: mangohud, gamemoderun, gamescope (Reinício)");
    form->addRow("Comando Wrapper:", m_wrapperEdit);

    m_customEnvEdit = new QLineEdit(page);
    m_customEnvEdit->setPlaceholderText("ex: MANGOHUD=1 RADV_PERFTEST=aco (Reinício)");
    form->addRow("Variáveis de Ambiente Adicionais:", m_customEnvEdit);
    layout->addLayout(form);
    layout->addStretch();
    return page;
}

} // namespace ac::launcher
