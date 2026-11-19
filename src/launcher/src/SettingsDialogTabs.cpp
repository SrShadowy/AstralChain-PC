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
    m_outputResCombo = new QComboBox(page);
    m_outputResCombo->addItem("Auto (Janela) [Reinício]", "auto");
    m_outputResCombo->addItem("1280x720 (HD) [Reinício]", "1280x720");
    m_outputResCombo->addItem("1600x900 (Switch Docked Nativo) [Reinício]", "1600x900");
    m_outputResCombo->addItem("1920x1080 (Full HD) [Reinício]", "1920x1080");
    m_outputResCombo->addItem("2560x1440 (QHD) [Reinício]", "2560x1440");
    m_outputResCombo->addItem("3840x2160 (4K UHD + Heaps 4K) [Reinício]", "3840x2160");
    m_aspectCombo = new QComboBox(page);
    m_aspectCombo->addItem("Auto (Janela) [Reinício]", "auto");
    m_aspectCombo->addItem("16:9 (Padrão) [Reinício]", "16:9");
    m_aspectCombo->addItem("16:10 (Steam Deck) [Reinício]", "16:10");
    m_aspectCombo->addItem("21:9 (Ultrawide) [Reinício]", "21:9");
    m_aspectCombo->addItem("32:9 (Super Ultrawide) [Reinício]", "32:9");
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
    m_scaleCombo->addItem("0.50x (Scene Performance) [Reinício]", 0.50);
    m_scaleCombo->addItem("0.58x (Scene Balanced) [Reinício]", 0.58);
    m_scaleCombo->addItem("0.67x (Scene Quality) [Reinício]", 0.67);
    m_scaleCombo->addItem("0.75x (Scene Ultra Quality) [Reinício]", 0.75);
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

    m_lodBiasSpin = new QDoubleSpinBox(page);
    m_lodBiasSpin->setRange(GraphicsSettings::kDefaultLodBias, GraphicsSettings::kMaxLodBias);
    m_lodBiasSpin->setSingleStep(GraphicsSettings::kLodSpinStep);
    m_lodBiasSpin->setDecimals(1);
    m_lodBiasSpin->setSpecialValueText("0.0 (Nativa - novos samplers)");

    m_lodMinSpin = new QDoubleSpinBox(page);
    m_lodMinSpin->setRange(GraphicsSettings::kDefaultLodMin, GraphicsSettings::kMaxLodMin);
    m_lodMinSpin->setSingleStep(GraphicsSettings::kLodSpinStep);
    m_lodMinSpin->setDecimals(1);
    m_lodMinSpin->setSpecialValueText("0.0 (Desligado - novos samplers)");

    m_noShadowCheck = new QCheckBox("Desativar Sombras (aplica a novos pipelines / mais FPS)", page);

    m_shadowScaleCombo = new QComboBox(page);
    m_shadowScaleCombo->addItem("1x (2048x2048 Nativo) [Reinício]", 1);
    m_shadowScaleCombo->addItem("2x (4096x4096 Nítido) [Reinício]", 2);
    m_shadowScaleCombo->addItem("4x (8192x8192 Ultra) [Reinício]", 4);

    m_drawDistanceSpin = new QSpinBox(page);
    m_drawDistanceSpin->setRange(GraphicsSettings::kMinDrawDistance, GraphicsSettings::kMaxDrawDistance);
    m_drawDistanceSpin->setSingleStep(GraphicsSettings::kDrawDistanceStep);
    m_drawDistanceSpin->setSuffix("% (Ao vivo)");

    m_hudAreaCombo = new QComboBox(page);
    m_hudAreaCombo->addItem("Centralizada 16:9 em Ultrawide (Ao vivo)", "16:9");
    m_hudAreaCombo->addItem("Tela Cheia Borda a Borda (Ao vivo)", "screen");
    m_hudHiddenCheck = new QCheckBox("Ocultar HUD em Jogo (Atalho F12 - Ao vivo)", page);

    layout->addRow("Resolução da Janela:", m_resolutionCombo);
    layout->addRow("Resolução Interna (Output):", m_outputResCombo);
    layout->addRow("Proporção (Aspect):", m_aspectCombo);
    layout->addRow("", m_fullscreenCheck);
    layout->addRow("", m_vsyncCheck);
    layout->addRow("Modo de Sincronização:", m_presentModeCombo);
    layout->addRow("FPS Alvo:", m_fpsSpin);
    layout->addRow("", m_fpsUnlockedCheck);
    layout->addRow("Escala Scene / SSAA:", m_scaleCombo);
    layout->addRow("Upscaler:", m_upscalerCombo);
    layout->addRow("Antialiasing:", m_antialiasCombo);
    layout->addRow("", m_fsrJitterCheck);
    layout->addRow("Nitidez FSR (RCAS):", m_fsrSharpnessSpin);
    layout->addRow("", m_frameGenCheck);
    layout->addRow("Distância de Visão:", m_drawDistanceSpin);
    layout->addRow("Sombras Solares (Shadow Scale):", m_shadowScaleCombo);
    layout->addRow("Qualidade de Textura (LOD Bias):", m_lodBiasSpin);
    layout->addRow("Nível de LoD Mínimo:", m_lodMinSpin);
    layout->addRow("", m_noShadowCheck);
    layout->addRow("Área da HUD:", m_hudAreaCombo);
    layout->addRow("", m_hudHiddenCheck);
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
    m_vtxGuardCheck = new QCheckBox("Vertex Guard (Clamp de Explosão de Vértices - Ao vivo)", page);
    m_vtxDebugCheck = new QCheckBox("Vertex Debug Logging (NaN/Inf em stderr - Ao vivo)", page);

    layout->addWidget(m_bc7Check);
    layout->addWidget(m_forceShadersCheck);
    layout->addWidget(m_verboseLogCheck);
    layout->addWidget(m_cutscene30Check);
    layout->addWidget(m_asyncSubmitCheck);
    layout->addWidget(m_vtxGuardCheck);
    layout->addWidget(m_vtxDebugCheck);

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
