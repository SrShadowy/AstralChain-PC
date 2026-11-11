#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;

namespace ac::launcher {

class SetupDialog : public QDialog {
    Q_OBJECT

public:
    explicit SetupDialog(QWidget* parent = nullptr);
    ~SetupDialog() override = default;

    QString keysPath() const;
    QString basePath() const;
    QString updatePath() const;
    QString installDir() const;

private slots:
    void onBrowseKeys();
    void onBrowseBase();
    void onBrowseUpdate();
    void onBrowseInstallDir();
    void validateInputs();
    void onStart();

private:
    void setupUi();
    void loadSavedPaths();
    void savePaths();

    QLineEdit* m_keysEdit{nullptr};
    QLineEdit* m_baseEdit{nullptr};
    QLineEdit* m_updateEdit{nullptr};
    QLineEdit* m_installDirEdit{nullptr};
    QPushButton* m_startBtn{nullptr};
    QPushButton* m_cancelBtn{nullptr};
};

} // namespace ac::launcher
