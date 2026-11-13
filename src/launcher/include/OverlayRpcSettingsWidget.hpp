#pragma once

#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;

namespace ac::launcher {

struct OverlaySettings;
struct DiscordRpcSettings;

class OverlayRpcSettingsWidget : public QWidget {
    Q_OBJECT

public:
    explicit OverlayRpcSettingsWidget(QWidget* parent = nullptr);

    void loadFrom(const OverlaySettings& overlay, const DiscordRpcSettings& rpc);
    void writeTo(OverlaySettings& overlay, DiscordRpcSettings& rpc) const;

signals:
    void modified();

private:
    void setupUi();

    QCheckBox* m_hudEnabledCheck{nullptr};
    QComboBox* m_hudModeCombo{nullptr};
    QComboBox* m_hudCornerCombo{nullptr};
    QDoubleSpinBox* m_hudOpacitySpin{nullptr};
    QCheckBox* m_hudWithMenuCheck{nullptr};

    QCheckBox* m_rpcEnabledCheck{nullptr};
    QCheckBox* m_rpcFileCheck{nullptr};
    QCheckBox* m_rpcMissionCheck{nullptr};
    QCheckBox* m_rpcAreaCheck{nullptr};
    QCheckBox* m_rpcBossCheck{nullptr};
    QCheckBox* m_rpcDiffCheck{nullptr};
    QCheckBox* m_rpcTimeCheck{nullptr};
    QLineEdit* m_rpcClientIdEdit{nullptr};
    bool m_loading{false};
};

} // namespace ac::launcher
