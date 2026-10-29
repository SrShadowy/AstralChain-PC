#pragma once

#include <QDialog>
#include <QMap>
#include <QString>
#include <memory>

class QTableWidget;
class QPushButton;

namespace ac::launcher {

class Config;

class ControlsDialog : public QDialog {
    Q_OBJECT

public:
    explicit ControlsDialog(Config& config, QWidget* parent = nullptr);
    ~ControlsDialog() override = default;

private slots:
    void onResetDefaults();
    void onSave();

private:
    void setupUi();
    void loadBindings();

    Config& m_config;
    QTableWidget* m_table{nullptr};
    QPushButton* m_saveButton{nullptr};
    QPushButton* m_resetButton{nullptr};
    QPushButton* m_cancelButton{nullptr};
};

} // namespace ac::launcher
