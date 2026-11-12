#include "LogWidget.hpp"
#include "LauncherIcons.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTextEdit>
#include <QClipboard>
#include <QGuiApplication>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QScrollBar>
#include <QDateTime>

namespace ac::launcher {

namespace {
constexpr int kLogIconSizePx = 15;
} // namespace

LogWidget::LogWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUi();
    QDir().mkpath("userdata/logs");
    m_logFile.setFileName("userdata/logs/latest.log");
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
}

LogWidget::~LogWidget()
{
    if (m_logFile.isOpen()) m_logFile.close();
}

void LogWidget::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);

    // 1. BARRA SUPERIOR (Filtros, Busca e Status)
    auto* topToolbar = new QHBoxLayout();
    topToolbar->setContentsMargins(0, 0, 0, 0);
    topToolbar->setSpacing(8);

    m_statusBadge = new QLabel("STANDBY", this);
    m_statusBadge->setObjectName("logStatusBadge");
    m_statusBadge->setFixedHeight(28);
    topToolbar->addWidget(m_statusBadge);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Buscar no log...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMinimumWidth(160);
    m_searchEdit->setFixedHeight(28);
    topToolbar->addWidget(m_searchEdit);

    const QColor iconColor("#7ee8ff");
    const QColor warnColor("#ffb826");
    const QColor btnColor("#d8eaf4");

    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->setIconSize(QSize(kLogIconSizePx, kLogIconSizePx));
    m_categoryCombo->addItem(makeLauncherIcon("channels", iconColor, iconColor, kLogIconSizePx), "Todos os canais", static_cast<int>(LogCategory::All));
    m_categoryCombo->addItem(makeLauncherIcon("audio", iconColor, iconColor, kLogIconSizePx), "Audio ([audio])", static_cast<int>(LogCategory::Audio));
    m_categoryCombo->addItem(makeLauncherIcon("gpu", iconColor, iconColor, kLogIconSizePx), "GPU & Vulkan ([vk])", static_cast<int>(LogCategory::GpuVulkan));
    m_categoryCombo->addItem(makeLauncherIcon("shaders", iconColor, iconColor, kLogIconSizePx), "Shaders ([shaders])", static_cast<int>(LogCategory::Shaders));
    m_categoryCombo->addItem(makeLauncherIcon("video", iconColor, iconColor, kLogIconSizePx), "Vídeo & Tela ([video])", static_cast<int>(LogCategory::VideoPresent));
    m_categoryCombo->addItem(makeLauncherIcon("backend", iconColor, iconColor, kLogIconSizePx), "Backend & NvN ([nvn])", static_cast<int>(LogCategory::BackendHle));
    m_categoryCombo->addItem(makeLauncherIcon("logic", iconColor, iconColor, kLogIconSizePx), "Lógica & App ([logic])", static_cast<int>(LogCategory::LogicApp));
    m_categoryCombo->addItem(makeLauncherIcon("input", iconColor, iconColor, kLogIconSizePx), "Entrada ([input])", static_cast<int>(LogCategory::Input));
    m_categoryCombo->addItem(makeLauncherIcon("warning", warnColor, warnColor, kLogIconSizePx), "Erros & Avisos", static_cast<int>(LogCategory::ErrorsWarnings));
    m_categoryCombo->setMinimumWidth(210);
    m_categoryCombo->setFixedHeight(28);
    topToolbar->addWidget(m_categoryCombo);

    topToolbar->addStretch();
    mainLayout->addLayout(topToolbar);

    // 2. CONSOLE CENTRAL DE LOG
    m_textEdit = new QTextEdit(this);
    m_textEdit->setObjectName("logView");
    m_textEdit->setReadOnly(true);
    m_textEdit->document()->setMaximumBlockCount(4000);
    mainLayout->addWidget(m_textEdit);

    // 3. BARRA INFERIOR (Ações e Autoscroll)
    auto* bottomToolbar = new QHBoxLayout();
    bottomToolbar->setContentsMargins(0, 0, 0, 0);
    bottomToolbar->setSpacing(8);

    m_autoScrollCheck = new QCheckBox("Autoscroll automático", this);
    m_autoScrollCheck->setChecked(true);
    bottomToolbar->addWidget(m_autoScrollCheck);
    bottomToolbar->addStretch();

    m_copyBtn = new QPushButton(makeLauncherIcon("copy", btnColor, iconColor, kLogIconSizePx), "Copiar", this);
    m_clearBtn = new QPushButton(makeLauncherIcon("trash", btnColor, iconColor, kLogIconSizePx), "Limpar", this);
    m_openFolderBtn = new QPushButton(makeLauncherIcon("folder", btnColor, iconColor, kLogIconSizePx), "Pasta de Logs", this);
    m_exportBtn = new QPushButton(makeLauncherIcon("save", btnColor, iconColor, kLogIconSizePx), "Exportar...", this);

    for (auto* btn : {m_copyBtn, m_clearBtn, m_openFolderBtn, m_exportBtn}) {
        btn->setIconSize(QSize(kLogIconSizePx, kLogIconSizePx));
        btn->setFixedHeight(28);
        btn->setCursor(Qt::PointingHandCursor);
        bottomToolbar->addWidget(btn);
    }
    mainLayout->addLayout(bottomToolbar);

    connect(m_searchEdit, &QLineEdit::textChanged, this, &LogWidget::onFilterChanged);
    connect(m_categoryCombo, &QComboBox::currentIndexChanged, this, &LogWidget::onCategoryChanged);
    connect(m_copyBtn, &QPushButton::clicked, this, &LogWidget::onCopyClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &LogWidget::onClearClicked);
    connect(m_openFolderBtn, &QPushButton::clicked, this, &LogWidget::onOpenFolderClicked);
    connect(m_exportBtn, &QPushButton::clicked, this, &LogWidget::onExportClicked);

    setRunningState(false);
}

bool LogWidget::shouldShowLine(const QString& line) const
{
    if (!m_filterQuery.isEmpty() && !line.contains(m_filterQuery, Qt::CaseInsensitive)) return false;
    const QString l = line.toLower();
    switch (m_category) {
    case LogCategory::Audio:
        return l.contains("[audio") || l.contains("audiostream") || l.contains("wave_out");
    case LogCategory::GpuVulkan:
        return l.contains("[vk]") || l.contains("[gpu]") || l.contains("[vulkan]") || l.contains("radv") || l.contains("vulkan");
    case LogCategory::Shaders:
        return l.contains("[shader") || l.contains("glslang") || l.contains("spirv") || l.contains("preparing shader");
    case LogCategory::VideoPresent:
        return l.contains("[video") || l.contains("[present") || l.contains("[post") || l.contains("swapchain") || l.contains("window");
    case LogCategory::BackendHle:
        return l.contains("[nvn") || l.contains("[backend") || l.contains("[hle") || l.contains("pl ") || l.contains("applet");
    case LogCategory::LogicApp:
        return l.contains("[logic") || l.contains("[app") || l.contains("fps") || l.contains("progress:") || l.contains("parallel");
    case LogCategory::Input:
        return l.contains("[input") || l.contains("gamepad") || l.contains("mouse") || l.contains("keyboard") || l.contains("button");
    case LogCategory::ErrorsWarnings:
        return l.contains("error") || l.contains("warn") || l.contains("fatal") || l.contains("crash") || l.contains("fail") || l.contains("stop");
    case LogCategory::All:
    default:
        return true;
    }
}

QString LogWidget::formatLineHtml(const QString& raw) const
{
    QString escaped = raw.toHtmlEscaped();
    if (escaped.contains("[audio]")) {
        escaped.replace("[audio]", "<span style='color:#4fc3f7; font-weight:bold;'>[audio]</span>");
    } else if (escaped.contains("[vk]") || escaped.contains("[gpu]") || escaped.contains("[vulkan]")) {
        escaped.replace("[vk]", "<span style='color:#ba68c8; font-weight:bold;'>[vk]</span>");
        escaped.replace("[gpu]", "<span style='color:#ba68c8; font-weight:bold;'>[gpu]</span>");
        escaped.replace("[vulkan]", "<span style='color:#ba68c8; font-weight:bold;'>[vulkan]</span>");
    } else if (escaped.contains("[shaders]") || escaped.contains("[shader]")) {
        escaped.replace("[shaders]", "<span style='color:#ffd54f; font-weight:bold;'>[shaders]</span>");
        escaped.replace("[shader]", "<span style='color:#ffd54f; font-weight:bold;'>[shader]</span>");
    } else if (escaped.contains("[video]") || escaped.contains("[present]") || escaped.contains("[post]")) {
        escaped.replace("[video]", "<span style='color:#81c784; font-weight:bold;'>[video]</span>");
        escaped.replace("[present]", "<span style='color:#81c784; font-weight:bold;'>[present]</span>");
        escaped.replace("[post]", "<span style='color:#81c784; font-weight:bold;'>[post]</span>");
    } else if (escaped.contains("[backend]") || escaped.contains("[nvn]") || escaped.contains("PL ")) {
        escaped.replace("[backend]", "<span style='color:#ffb74d; font-weight:bold;'>[backend]</span>");
        escaped.replace("[nvn]", "<span style='color:#ffb74d; font-weight:bold;'>[nvn]</span>");
    } else if (escaped.contains("[logic]") || escaped.contains("[app]")) {
        escaped.replace("[logic]", "<span style='color:#64b5f6; font-weight:bold;'>[logic]</span>");
        escaped.replace("[app]", "<span style='color:#64b5f6; font-weight:bold;'>[app]</span>");
    } else if (escaped.contains("[input]")) {
        escaped.replace("[input]", "<span style='color:#4db6ac; font-weight:bold;'>[input]</span>");
    }

    if (escaped.contains("error", Qt::CaseInsensitive) || escaped.contains("fatal", Qt::CaseInsensitive) || escaped.contains("crash", Qt::CaseInsensitive)) {
        return QString("<span style='color:#ef5350;'>%1</span>").arg(escaped);
    }
    if (escaped.contains("warning", Qt::CaseInsensitive) || escaped.contains("warn", Qt::CaseInsensitive)) {
        return QString("<span style='color:#ffa726;'>%1</span>").arg(escaped);
    }
    return QString("<span style='color:#cfd8dc;'>%1</span>").arg(escaped);
}

void LogWidget::appendLog(const QString& line)
{
    if (m_logFile.isOpen()) {
        m_logFile.write((line + "\n").toUtf8());
        m_logFile.flush();
    }
    m_lines.append(line);
    if (m_lines.size() > 4000) m_lines.removeFirst();

    if (shouldShowLine(line)) {
        m_textEdit->append(formatLineHtml(line));
        if (m_autoScrollCheck->isChecked()) {
            auto* bar = m_textEdit->verticalScrollBar();
            bar->setValue(bar->maximum());
        }
    }
}

void LogWidget::clearLog()
{
    m_lines.clear();
    m_textEdit->clear();
}

void LogWidget::refreshView()
{
    m_textEdit->clear();
    for (const auto& line : m_lines) {
        if (shouldShowLine(line)) {
            m_textEdit->append(formatLineHtml(line));
        }
    }
    if (m_autoScrollCheck->isChecked()) {
        auto* bar = m_textEdit->verticalScrollBar();
        bar->setValue(bar->maximum());
    }
}

void LogWidget::onFilterChanged()
{
    m_filterQuery = m_searchEdit->text().trimmed();
    refreshView();
}

void LogWidget::onCategoryChanged(int index)
{
    m_category = static_cast<LogCategory>(m_categoryCombo->itemData(index).toInt());
    refreshView();
}

void LogWidget::onCopyClicked()
{
    QGuiApplication::clipboard()->setText(m_textEdit->toPlainText());
}

void LogWidget::onClearClicked()
{
    clearLog();
}

void LogWidget::onExportClicked()
{
    const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    const QString defaultPath = QString("userdata/logs/log_%1.txt").arg(ts);
    const QString path = QFileDialog::getSaveFileName(this, "Exportar Log", defaultPath, "Text files (*.txt);;All files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(m_textEdit->toPlainText().toUtf8());
    }
}

void LogWidget::onOpenFolderClicked()
{
    const QString logPath = QDir("userdata/logs").absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
}

void LogWidget::setRunningState(bool running, qint64 pid)
{
    if (running) {
        m_statusBadge->setText(QString("ATIVO (PID %1)").arg(pid));
        m_statusBadge->setStyleSheet(
            "color: #26eb8c; font-weight: 700; font-size: 11px; padding: 2px 10px; "
            "background: rgba(38, 235, 140, 0.12); border-radius: 2px; border: 1px solid rgba(38, 235, 140, 0.45);");
    } else {
        m_statusBadge->setText("STANDBY");
        m_statusBadge->setStyleSheet(
            "color: #7d99b5; font-weight: 600; font-size: 11px; padding: 2px 10px; "
            "background: rgba(16, 28, 48, 0.85); border-radius: 2px; border: 1px solid rgba(0, 212, 255, 0.25);");
    }
}

} // namespace ac::launcher
