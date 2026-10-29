#include "MainWindow.hpp"
#include "SettingsDialog.hpp"
#include "ControlsDialog.hpp"
#include "SetupDialog.hpp"
#include "LogWidget.hpp"
#include "Theme.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QPainter>
#include <QDir>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QCloseEvent>
#include <QStyle>

namespace ac::launcher {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Astral Chain PC Launcher (Linux)");
    resize(980, 620);
    setMinimumSize(840, 520);

    loadArtList();
    setupUi();
    applyTheme();

    connect(&m_runner, &ProcessRunner::started, this, &MainWindow::onProcessStarted);
    connect(&m_runner, &ProcessRunner::lineReceived, this, &MainWindow::onProcessLine);
    connect(&m_runner, &ProcessRunner::finished, this, &MainWindow::onProcessFinished);

    m_artTimer = new QTimer(this);
    connect(m_artTimer, &QTimer::timeout, this, &MainWindow::onNextArt);
    m_artTimer->start(20000);

    QTimer::singleShot(200, this, &MainWindow::refreshGameStatus);
}

void MainWindow::loadArtList()
{
    QDir artDir("launcher/art");
    const QStringList files = artDir.entryList({"*.jpg", "*.png"}, QDir::Files, QDir::Name);
    for (const auto& f : files) m_artPaths << artDir.filePath(f);
    if (!m_artPaths.isEmpty()) m_currentBackground.load(m_artPaths.first());
}

void MainWindow::onNextArt()
{
    if (m_artPaths.isEmpty()) return;
    m_currentArtIndex = (m_currentArtIndex + 1) % m_artPaths.size();
    m_currentBackground.load(m_artPaths[m_currentArtIndex]);
    update();
}

void MainWindow::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    if (!m_currentBackground.isNull()) {
        p.drawPixmap(rect(), m_currentBackground.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    } else {
        p.fillRect(rect(), QColor(13, 17, 23));
    }
    p.fillRect(rect(), QColor(6, 8, 12, 190));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_runner.isRunning() && m_isGameRunning) {
        const auto ret = QMessageBox::question(this, "Jogo em Execução",
            "Astral Chain ainda está em execução.\nDeseja encerrar o processo antes de sair?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (ret == QMessageBox::Yes) {
            m_runner.stop();
            event->accept();
        } else if (ret == QMessageBox::No) {
            event->accept();
        } else {
            event->ignore();
            return;
        }
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::applyTheme()
{
    setStyleSheet(getLauncherStyleSheet());
}

void MainWindow::refreshGameStatus()
{
    QProcess proc;
    proc.start("python3", {"-m", "astral_chain.setup_game", "--status"});
    if (proc.waitForFinished(3000)) {
        const auto doc = QJsonDocument::fromJson(proc.readAllStandardOutput());
        if (doc.isObject()) {
            const auto obj = doc.object();
            m_installed = obj.value("installed").toBool();
            m_buildStatus = obj.value("build").toString();
            m_shaderStatus = obj.value("shaders").toString();

            m_statusLabel->setText(QString("Game: %1  |  Build: %2  |  Shaders: %3")
                .arg(m_installed ? "Ready" : "Not extracted").arg(m_buildStatus).arg(m_shaderStatus));
            m_statusDot->setStyleSheet(m_installed ? "font-size: 11px; color: #66bb6a;" : "font-size: 11px; color: #ff7043;");

            if (!m_runner.isRunning()) {
                m_playBtn->setEnabled(m_installed);
                m_diagBtn->setEnabled(m_installed);
                m_shadersBtn->setEnabled(m_installed);
            }
            return;
        }
    }
    m_statusLabel->setText("Status: Pronto (configure na primeira execução)");
    m_statusDot->setStyleSheet("font-size: 11px; color: #ffa726;");
}

void MainWindow::setRunningUi(bool running, qint64 pid)
{
    if (running) {
        m_playBtn->setText("⏹  PARAR");
        m_playBtn->setObjectName("stopBtn");
        m_playBtn->setEnabled(true);
        m_playGlow->setColor(QColor(239, 83, 80, 160));
        m_statusDot->setStyleSheet("font-size: 11px; color: #66bb6a;");
    } else {
        m_playBtn->setText("▶  PLAY");
        m_playBtn->setObjectName("playBtn");
        m_playBtn->setEnabled(m_installed);
        m_playGlow->setColor(QColor(0, 188, 212, 120));
        m_statusDot->setStyleSheet("font-size: 11px; color: #42a5f5;");
    }
    m_playBtn->style()->unpolish(m_playBtn);
    m_playBtn->style()->polish(m_playBtn);
    m_logWidget->setRunningState(running, pid);
}

void MainWindow::runCommand(const QString& program, const QStringList& args, const QString& statusText, bool isGame)
{
    m_isGameRunning = isGame;
    m_statusLabel->setText(statusText);
    m_statusDot->setStyleSheet("font-size: 11px; color: #42a5f5;");
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);

    m_setupBtn->setEnabled(false);
    m_shadersBtn->setEnabled(false);
    m_diagBtn->setEnabled(!isGame);

    if (isGame) {
        setRunningUi(true, 0);
    } else {
        m_playBtn->setEnabled(false);
    }
    m_runner.start(program, args);
}

void MainWindow::onPlay()
{
    if (m_runner.isRunning() && m_isGameRunning) {
        m_statusLabel->setText("Encerrando jogo...");
        m_playBtn->setText("⏳ ENCERRANDO...");
        m_playBtn->setEnabled(false);
        m_runner.stop();
        return;
    }
    QStringList args;
    const QString env = m_config.buildLaunchEnv();
    if (!env.isEmpty()) args << "--env" << env;
    if (!m_config.game().commandWrapper.trimmed().isEmpty()) {
        args << "--wrapper" << m_config.game().commandWrapper.trimmed();
    }
    runCommand("./play.sh", args, "Executando Astral Chain...", true);
}

void MainWindow::onPlayDiagnostics()
{
    if (m_runner.isRunning() && m_isGameRunning) {
        m_runner.stop();
        return;
    }
    QStringList args;
    const QString env = m_config.buildLaunchEnv();
    if (!env.isEmpty()) args << "--env" << env;
    if (!m_config.game().commandWrapper.trimmed().isEmpty()) {
        args << "--wrapper" << m_config.game().commandWrapper.trimmed();
    }
    runCommand("./play-diagnostics.sh", args, "Executando com Diagnósticos...", true);
}

void MainWindow::onSetupGame()
{
    SetupDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    QStringList args = {"--keys", dlg.keysPath(), "--base", dlg.basePath(), "--update", dlg.updatePath()};
    if (!dlg.installDir().isEmpty()) args << "--install-dir" << dlg.installDir();
    runCommand("./setup.sh", args, "Iniciando configuração do jogo...", false);
}

void MainWindow::onCompileShaders()
{
    QStringList args = {"-m", "astral_chain.setup_game", "--rebuild-if-stale"};
    if (m_config.game().forceShaders) args << "--force-shaders";
    const QString py = QFile::exists(".venv/bin/python") ? ".venv/bin/python" : "python3";
    runCommand(py, args, "Compilando shaders...", false);
}

void MainWindow::onOpenSettings() { SettingsDialog dlg(m_config, this); dlg.exec(); }
void MainWindow::onOpenControls() { ControlsDialog dlg(m_config, this); dlg.exec(); }

void MainWindow::onProcessStarted(qint64 pid)
{
    if (m_isGameRunning) {
        setRunningUi(true, pid);
        m_statusLabel->setText(QString("Astral Chain em execução (PID %1)").arg(pid));
    }
}

void MainWindow::onProcessLine(const QString& line)
{
    m_logWidget->appendLog(line);

    if (line.startsWith("PROGRESS:")) {
        const auto parts = line.split(':');
        if (parts.size() >= 3) {
            bool ok = false;
            const int pct = parts[1].toInt(&ok);
            if (ok) {
                m_progressBar->setValue(pct);
                const QString msg = parts.mid(2).join(':').trimmed();
                m_statusLabel->setText(msg);
                m_progressBar->setFormat(QString("%1 (%p%)").arg(msg));
            }
        }
    }
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus)
{
    m_isGameRunning = false;
    setRunningUi(false);
    m_progressBar->setValue(exitCode == 0 ? 100 : 0);
    m_setupBtn->setEnabled(true);
    refreshGameStatus();
    m_logWidget->appendLog(QString("\nProcesso finalizado (código %1).").arg(exitCode));
}

} // namespace ac::launcher
