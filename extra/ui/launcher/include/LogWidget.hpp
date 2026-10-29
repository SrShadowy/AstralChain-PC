#pragma once

#include <QWidget>
#include <QStringList>
#include <QFile>

class QTextEdit;
class QLineEdit;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;

namespace ac::launcher {

enum class LogCategory {
    All = 0,
    Audio,
    GpuVulkan,
    Shaders,
    VideoPresent,
    BackendHle,
    LogicApp,
    Input,
    ErrorsWarnings
};

class LogWidget : public QWidget {
    Q_OBJECT

public:
    explicit LogWidget(QWidget* parent = nullptr);
    ~LogWidget() override;

    void appendLog(const QString& line);
    void clearLog();
    void setRunningState(bool running, qint64 pid = 0);

private slots:
    void onFilterChanged();
    void onCategoryChanged(int index);
    void onCopyClicked();
    void onClearClicked();
    void onExportClicked();
    void onOpenFolderClicked();

private:
    void setupUi();
    [[nodiscard]] bool shouldShowLine(const QString& line) const;
    [[nodiscard]] QString formatLineHtml(const QString& raw) const;
    void refreshView();

    QLabel* m_statusBadge{nullptr};
    QLineEdit* m_searchEdit{nullptr};
    QComboBox* m_categoryCombo{nullptr};
    QCheckBox* m_autoScrollCheck{nullptr};
    QPushButton* m_copyBtn{nullptr};
    QPushButton* m_clearBtn{nullptr};
    QPushButton* m_openFolderBtn{nullptr};
    QPushButton* m_exportBtn{nullptr};

    QTextEdit* m_textEdit{nullptr};
    QStringList m_lines;
    LogCategory m_category{LogCategory::All};
    QString m_filterQuery;
    QFile m_logFile;
};

} // namespace ac::launcher
