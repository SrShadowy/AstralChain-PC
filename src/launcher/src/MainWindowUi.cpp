#include "MainWindow.hpp"
#include "LogWidget.hpp"
#include "CustomButton.hpp"
#include "LauncherIcons.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>

namespace ac::launcher {

void MainWindow::setupUi()
{
    auto* central = new QWidget(this);
    central->setObjectName("centralWidget");
    setCentralWidget(central);

    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(22, 22, 22, 22);
    rootLayout->setSpacing(18);

    m_sidePanel = new QWidget(this);
    m_sidePanel->setObjectName("sidePanel");
    m_sidePanel->setFixedWidth(320);
    auto* leftLayout = new QVBoxLayout(m_sidePanel);
    leftLayout->setSpacing(0);
    leftLayout->setContentsMargins(16, 18, 16, 16);

    auto* kicker = new QLabel("NEURON // LEGATUS SYSTEM", m_sidePanel);
    kicker->setObjectName("kickerLabel");
    leftLayout->addWidget(kicker);
    leftLayout->addSpacing(2);

    auto* title = new QLabel("ASTRAL CHAIN", m_sidePanel);
    title->setObjectName("titleLabel");
    title->setAlignment(Qt::AlignLeft);
    leftLayout->addWidget(title);

    auto* subtitle = new QLabel(
        QStringLiteral("LINUX NATIVE  ·  VULKAN 1.3  ·  FSR 3.1  ·  v%1").arg(AC_APP_VERSION),
        m_sidePanel
    );
    subtitle->setObjectName("subtitleLabel");
    leftLayout->addWidget(subtitle);

    leftLayout->addSpacing(14);

    const QColor normalIcon("#dff2fc");
    const QColor hoverIcon("#08101c");
    const QColor secondaryIcon("#7ee8ff");

    m_playBtn = new CustomButton(" JOGAR", m_sidePanel, true /* isPrimary */);
    m_playBtn->setIcon(makeLauncherIcon("play", normalIcon, hoverIcon, kDefaultUiIconPx));
    m_playBtn->setFixedHeight(52);
    leftLayout->addWidget(m_playBtn);
    leftLayout->addSpacing(10);

    m_diagBtn = new CustomButton(" Jogar (Diagnósticos)", m_sidePanel);
    m_diagBtn->setIcon(makeLauncherIcon("terminal", secondaryIcon, hoverIcon, kDefaultUiIconPx));

    m_setupBtn = new CustomButton(" Configurar Jogo", m_sidePanel);
    m_setupBtn->setIcon(makeLauncherIcon("setup", secondaryIcon, hoverIcon, kDefaultUiIconPx));

    m_shadersBtn = new CustomButton(" Compilar Shaders", m_sidePanel);
    m_shadersBtn->setIcon(makeLauncherIcon("shaders", secondaryIcon, hoverIcon, kDefaultUiIconPx));

    m_settingsBtn = new CustomButton(" Configurações", m_sidePanel);
    m_settingsBtn->setIcon(makeLauncherIcon("settings", secondaryIcon, hoverIcon, kDefaultUiIconPx));

    m_controlsBtn = new CustomButton(" Controles", m_sidePanel);
    m_controlsBtn->setIcon(makeLauncherIcon("input", secondaryIcon, hoverIcon, kDefaultUiIconPx));

    for (auto* btn : {m_diagBtn, m_setupBtn, m_shadersBtn, m_settingsBtn, m_controlsBtn}) {
        btn->setFixedHeight(42);
        leftLayout->addWidget(btn);
        leftLayout->addSpacing(4);
    }

    leftLayout->addStretch();

    auto* footer = new QLabel("Atalhos In-Game:  [Insert] Terminal IRIS  ·  [F8] RE Info", m_sidePanel);
    footer->setObjectName("sideFooterLabel");
    leftLayout->addWidget(footer);

    rootLayout->addWidget(m_sidePanel);

    auto* rightBox = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightBox);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    auto* statusCard = new QWidget(rightBox);
    statusCard->setObjectName("statusCard");
    auto* statusCardLayout = new QVBoxLayout(statusCard);
    statusCardLayout->setContentsMargins(12, 9, 12, 9);
    statusCardLayout->setSpacing(6);

    auto* statusRow = new QHBoxLayout();
    statusRow->setSpacing(8);

    m_statusDot = new QLabel("●", statusCard);
    m_statusDot->setObjectName("statusDot");
    m_statusDot->setFixedSize(16, 16);
    m_statusDot->setAlignment(Qt::AlignCenter);
    statusRow->addWidget(m_statusDot);

    m_statusLabel = new QLabel("Verificando instalação do jogo...", statusCard);
    m_statusLabel->setObjectName("statusLabel");
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();

    statusCardLayout->addLayout(statusRow);

    m_progressBar = new QProgressBar(statusCard);
    m_progressBar->setObjectName("mainProgress");
    m_progressBar->setRange(0, 100);
    m_progressBar->setVisible(false);
    m_progressBar->setFixedHeight(5);
    m_progressBar->setTextVisible(false);
    statusCardLayout->addWidget(m_progressBar);

    rightLayout->addWidget(statusCard);

    m_logWidget = new LogWidget(rightBox);
    rightLayout->addWidget(m_logWidget);

    rootLayout->addWidget(rightBox);

    connect(m_playBtn, &CustomButton::clicked, this, &MainWindow::onPlay);
    connect(m_diagBtn, &CustomButton::clicked, this, &MainWindow::onPlayDiagnostics);
    connect(m_setupBtn, &CustomButton::clicked, this, &MainWindow::onSetupGame);
    connect(m_shadersBtn, &CustomButton::clicked, this, &MainWindow::onCompileShaders);
    connect(m_settingsBtn, &CustomButton::clicked, this, &MainWindow::onOpenSettings);
    connect(m_controlsBtn, &CustomButton::clicked, this, &MainWindow::onOpenControls);
}

} // namespace ac::launcher
