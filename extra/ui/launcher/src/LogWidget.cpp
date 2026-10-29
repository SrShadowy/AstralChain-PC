#include "LogWidget.hpp"

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

    auto* toolbar = new QHBoxLayout();
    toolbar->setSpacing(6);

    m_statusBadge = new QLabel("○ Inativo", this);
    m_statusBadge->setObjectName("logStatusBadge");
    toolbar->addWidget(m_statusBadge);
    toolbar->addSpacing(6);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Buscar no log...");
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMaximumWidth(180);
    toolbar->addWidget(m_searchEdit);

    m_categoryCombo = new QComboBox(this);
    m_categoryCombo->addItem("📋 Todos os canais", static_cast<int>(LogCategory::All));
    m_categoryCombo->addItem("🔊 Audio ([audio])", static_cast<int>(LogCategory::Audio));
    m_categoryCombo->addItem("🎮 GPU & Vulkan ([vk])", static_cast<int>(LogCategory::GpuVulkan));
    m_categoryCombo->addItem("✨ Shaders ([shaders])", static_cast<int>(LogCategory::Shaders));
    m_categoryCombo->addItem("📺 Vídeo & Tela ([video])", static_cast<int>(LogCategory::VideoPresent));
    m_categoryCombo->addItem("⚙️ Backend & NvN ([nvn])", static_cast<int>(LogCategory::BackendHle));
    m_categoryCombo->addItem("🧠 Lógica & App ([logic])", static_cast<int>(LogCategory::LogicApp));
    m_categoryCombo->addItem("🕹️ Entrada ([input])", static_cast<int>(LogCategory::Input));
    m_categoryCombo->addItem("⚠️ Erros & Avisos", static_cast<int>(LogCategory::ErrorsWarnings));
    toolbar->addWidget(m_categoryCombo);

    m_autoScrollCheck = new QCheckBox("Autoscroll", this);
    m_autoScrollCheck->setChecked(true);
    toolbar->addWidget(m_autoScrollCheck);
    toolbar->addStretch();

    m_copyBtn = new QPushButton("📋 Copiar", this);
    m_clearBtn = new QPushButton("🗑 Limpar", this);
    m_openFolderBtn = new QPushButton("📁 Pasta", this);
    m_exportBtn = new QPushButton("💾 Exportar", this);

    for (auto* btn : {m_copyBtn, m_clearBtn, m_openFolderBtn, m_exportBtn}) {
        btn->setFixedHeight(26);
        btn->setCursor(Qt::PointingHandCursor);
        toolbar->addWidget(btn);
    }
    mainLayout->addLayout(toolbar);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setObjectName("logView");
    m_textEdit->setReadOnly(true);
    m_textEdit->document()->setMaximumBlockCount(4000);
    mainLayout->addWidget(m_textEdit);

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
        m_statusBadge->setText(QString("● Executando (PID %1)").arg(pid));
        m_statusBadge->setStyleSheet(
            "color: #66bb6a; font-weight: bold; font-size: 11px; padding: 2px 10px; "
            "background: rgba(76, 175, 80, 0.15); border-radius: 10px; border: 1px solid rgba(76, 175, 80, 0.35);");
    } else {
        m_statusBadge->setText("○ Inativo");
        m_statusBadge->setStyleSheet(
            "color: #90a4ae; font-size: 11px; padding: 2px 10px; "
            "background: rgba(144, 164, 174, 0.1); border-radius: 10px; border: 1px solid rgba(144, 164, 174, 0.25);");
    }
}

} // namespace ac::launcher
