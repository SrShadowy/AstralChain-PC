#include "MainWindow.hpp"
#include "LogWidget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QGraphicsDropShadowEffect>

namespace ac::launcher {

void MainWindow::setupUi()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* rootLayout = new QHBoxLayout(central);
    rootLayout->setContentsMargins(24, 24, 24, 24);
    rootLayout->setSpacing(20);

    m_sidePanel = new QWidget(this);
    m_sidePanel->setObjectName("sidePanel");
    m_sidePanel->setFixedWidth(290);
    auto* leftLayout = new QVBoxLayout(m_sidePanel);
    leftLayout->setSpacing(0);
    leftLayout->setContentsMargins(20, 24, 20, 20);

    auto* title = new QLabel("ASTRAL CHAIN", m_sidePanel);
    title->setObjectName("titleLabel");
    title->setAlignment(Qt::AlignLeft);
    leftLayout->addWidget(title);

    auto* subtitle = new QLabel("Native PC Port  ·  Qt6 / SDL3 / Vulkan", m_sidePanel);
    subtitle->setObjectName("subtitleLabel");
    leftLayout->addWidget(subtitle);

    leftLayout->addSpacing(20);

    m_playBtn = new QPushButton("▶  PLAY", m_sidePanel);
    m_playBtn->setObjectName("playBtn");
    m_playBtn->setFixedHeight(52);
    m_playBtn->setCursor(Qt::PointingHandCursor);

    m_playGlow = new QGraphicsDropShadowEffect(m_playBtn);
    m_playGlow->setBlurRadius(28);
    m_playGlow->setColor(QColor(0, 188, 212, 120));
    m_playGlow->setOffset(0, 2);
    m_playBtn->setGraphicsEffect(m_playGlow);

    leftLayout->addWidget(m_playBtn);
    leftLayout->addSpacing(14);

    m_diagBtn = new QPushButton("  Play (Diagnostics)", m_sidePanel);
    m_setupBtn = new QPushButton("  Setup Game", m_sidePanel);
    m_shadersBtn = new QPushButton("  Compile Shaders", m_sidePanel);
    m_settingsBtn = new QPushButton("  Settings", m_sidePanel);
    m_controlsBtn = new QPushButton("  Controls", m_sidePanel);

    for (auto* btn : {m_diagBtn, m_setupBtn, m_shadersBtn, m_settingsBtn, m_controlsBtn}) {
        btn->setObjectName("sideBtn");
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(38);
        leftLayout->addWidget(btn);
        leftLayout->addSpacing(6);
    }

    leftLayout->addStretch();

    m_artBtn = new QPushButton("🖼  Switch Art", m_sidePanel);
    m_artBtn->setObjectName("artBtn");
    m_artBtn->setCursor(Qt::PointingHandCursor);
    m_artBtn->setFixedHeight(34);
    leftLayout->addWidget(m_artBtn);

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

    connect(m_playBtn, &QPushButton::clicked, this, &MainWindow::onPlay);
    connect(m_diagBtn, &QPushButton::clicked, this, &MainWindow::onPlayDiagnostics);
    connect(m_setupBtn, &QPushButton::clicked, this, &MainWindow::onSetupGame);
    connect(m_shadersBtn, &QPushButton::clicked, this, &MainWindow::onCompileShaders);
    connect(m_settingsBtn, &QPushButton::clicked, this, &MainWindow::onOpenSettings);
    connect(m_controlsBtn, &QPushButton::clicked, this, &MainWindow::onOpenControls);
    connect(m_artBtn, &QPushButton::clicked, this, &MainWindow::onNextArt);
}

} // namespace ac::launcher
