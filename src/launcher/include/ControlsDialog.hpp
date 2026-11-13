#pragma once

#include <QDialog>

class QTableWidget;
class QPushButton;

namespace ac::launcher {

class Config;
class ConfigWatcher;

class ControlsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ControlsDialog(Config& config,
                            ConfigWatcher* watcher = nullptr,
                            QWidget* parent = nullptr);
    ~ControlsDialog() override = default;

public slots:
    void reloadExternal();

private slots:
    void onResetDefaults();
    void onApply();
    void onSave();

private:
    void setupUi();
    void loadBindings();
    void writeBindingsAndSave();

    Config& m_config;
    ConfigWatcher* m_watcher{nullptr};
    QTableWidget* m_table{nullptr};
    QPushButton* m_applyButton{nullptr};
    QPushButton* m_saveButton{nullptr};
    QPushButton* m_resetButton{nullptr};
    QPushButton* m_cancelButton{nullptr};
};

} // namespace ac::launcher
