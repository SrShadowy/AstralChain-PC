#include "MainWindow.hpp"
#include "LogWidget.hpp"
#include "CustomButton.hpp"
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
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(20);

    m_sidePanel = new QWidget(this);
    m_sidePanel->setObjectName("sidePanel");
    m_sidePanel->setFixedWidth(320);
    auto* leftLayout = new QVBoxLayout(m_sidePanel);
    leftLayout->setSpacing(0);
    leftLayout->setContentsMargins(16, 20, 16, 18);

    auto* title = new QLabel("ASTRAL CHAIN", m_sidePanel);
    title->setObjectName("titleLabel");
    title->setAlignment(Qt::AlignLeft);
    leftLayout->addWidget(title);

    auto* subtitle = new QLabel("Native PC Port  ·  Qt6 / SDL3 / Vulkan", m_sidePanel);
    subtitle->setObjectName("subtitleLabel");
    leftLayout->addWidget(subtitle);

    leftLayout->addSpacing(18);

    m_playBtn = new CustomButton("▶  PLAY", m_sidePanel, true /* isPrimary */);
    m_playBtn->setFixedHeight(52);
    leftLayout->addWidget(m_playBtn);
    leftLayout->addSpacing(10);

    m_diagBtn = new CustomButton("  Play (Diagnostics)", m_sidePanel);
    m_setupBtn = new CustomButton("  Setup Game", m_sidePanel);
    m_shadersBtn = new CustomButton("  Compile Shaders", m_sidePanel);
    m_settingsBtn = new CustomButton("  Settings", m_sidePanel);
    m_controlsBtn = new CustomButton("  Controls", m_sidePanel);

    for (auto* btn : {m_diagBtn, m_setupBtn, m_shadersBtn, m_settingsBtn, m_controlsBtn}) {
        btn->setFixedHeight(42);
        leftLayout->addWidget(btn);
        leftLayout->addSpacing(4);
    }

    leftLayout->addStretch();
    rootLayout->addWidget(m_sidePanel);

    auto* rightBox = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightBox);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    auto* statusRow = new QHBoxLayout();
    statusRow->setSpacing(8);

    m_statusDot = new QLabel("●", rightBox);
    m_statusDot->setObjectName("statusDot");
    m_statusDot->setFixedSize(18, 18);
    m_statusDot->setAlignment(Qt::AlignCenter);
    statusRow->addWidget(m_statusDot);

    m_statusLabel = new QLabel("Checking game installation...", rightBox);
    m_statusLabel->setObjectName("statusLabel");
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();

    rightLayout->addLayout(statusRow);

    m_progressBar = new QProgressBar(rightBox);
    m_progressBar->setObjectName("mainProgress");
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(false);
    m_progressBar->setFixedHeight(5);
    m_progressBar->setTextVisible(false);
    rightLayout->addWidget(m_progressBar);

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
