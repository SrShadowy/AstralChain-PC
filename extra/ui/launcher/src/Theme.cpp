#include "Theme.hpp"

namespace ac::launcher {

QString getLauncherStyleSheet()
{
    return QString::fromUtf8(R"(
        QMainWindow, QWidget#centralWidget {
            background-color: #3b3a4a;
        }
        QWidget#sidePanel {
            background-color: rgba(45, 44, 58, 0.90);
            border: 1px solid rgba(100, 181, 246, 0.22);
            border-radius: 14px;
        }
        QLabel#titleLabel {
            font-size: 26px; font-weight: 800; color: #00d4ff; letter-spacing: 3px; padding-bottom: 2px;
        }
        QLabel#subtitleLabel {
            font-size: 10px; color: rgba(215, 230, 223, 0.82); letter-spacing: 1px; padding-bottom: 4px;
        }
        QLabel#statusDot { font-size: 11px; color: #66bb6a; }
        QLabel#statusLabel { font-size: 13px; color: rgba(236, 239, 241, 0.92); font-weight: 500; }
        QProgressBar#mainProgress {
            background-color: rgba(28, 27, 36, 0.85); border: 1px solid rgba(100, 181, 246, 0.3); border-radius: 3px;
        }
        QProgressBar#mainProgress::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #3168b8, stop:1 #00bcd4); border-radius: 2px;
        }
        QTextEdit#logView {
            background-color: rgba(28, 27, 36, 0.92);
            border: 1px solid rgba(100, 181, 246, 0.2);
            border-radius: 10px; color: #d7e6df; font-family: "JetBrains Mono", "Fira Code", monospace; font-size: 11px;
            padding: 8px 10px; selection-background-color: #3168b8; selection-color: white;
        }
        QLineEdit {
            background-color: rgba(43, 42, 54, 0.90);
            border: 1px solid rgba(100, 181, 246, 0.35);
            border-radius: 6px;
            color: #d7e6df;
            font-size: 11px;
            padding: 4px 10px;
        }
        QLineEdit:focus { border-color: #00bcd4; }
        QComboBox {
            background-color: rgba(43, 42, 54, 0.90);
            border: 1px solid rgba(100, 181, 246, 0.35);
            border-radius: 6px;
            color: #d7e6df;
            font-size: 11px;
            padding: 4px 10px;
            min-width: 140px;
        }
        QComboBox:hover { border-color: #00bcd4; }
        QComboBox QAbstractItemView {
            background-color: #2b2a36;
            border: 1px solid rgba(100, 181, 246, 0.35);
            selection-background-color: #3168b8;
            color: #d7e6df;
            padding: 4px;
        }
        QCheckBox {
            color: #d7e6df;
            font-size: 11px;
            font-weight: 500;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 15px;
            height: 15px;
            border: 1px solid rgba(100, 181, 246, 0.45);
            border-radius: 3px;
            background: rgba(28, 27, 36, 0.7);
        }
        QCheckBox::indicator:checked { background-color: #00bcd4; border-color: #00e5ff; }
        QDialog { background-color: #3b3a4a; }
        QTabWidget::pane { border: 1px solid rgba(100, 181, 246, 0.25); border-radius: 8px; background-color: rgba(43, 42, 54, 0.95); }
        QTabBar::tab {
            background-color: rgba(43, 42, 54, 0.7); color: #90a4ae; border: 1px solid rgba(100, 181, 246, 0.2);
            border-bottom: none; border-top-left-radius: 6px; border-top-right-radius: 6px; padding: 6px 14px; margin-right: 2px; font-size: 12px;
        }
        QTabBar::tab:selected { background-color: rgba(59, 58, 74, 0.95); color: #00bcd4; border-color: rgba(100, 181, 246, 0.4); font-weight: 600; }
        QTabBar::tab:hover:!selected { background-color: rgba(50, 49, 64, 0.8); color: #cfd8dc; }
        QTableWidget {
            background-color: rgba(28, 27, 36, 0.85); color: #cfd8dc; border: 1px solid rgba(100, 181, 246, 0.25);
            border-radius: 6px; gridline-color: rgba(100, 181, 246, 0.15); font-size: 12px;
        }
        QTableWidget::item { padding: 4px 8px; }
        QTableWidget::item:selected { background-color: rgba(49, 104, 184, 0.5); }
        QHeaderView::section {
            background-color: rgba(43, 42, 54, 0.9); color: #90a4ae; border: none; border-bottom: 1px solid rgba(100, 181, 246, 0.2); padding: 6px 8px; font-size: 11px; font-weight: 600;
        }
        QPushButton {
            background-color: #3168b8; color: #d7e6df; border: none;
            border-radius: 4px; padding: 5px 14px; font-size: 11px; font-weight: 600;
        }
        QPushButton:hover { background-color: #d7e6df; color: #3168b8; }
        QPushButton:pressed { background-color: #b9d0c6; color: #3168b8; }
        QPushButton:disabled { background-color: rgba(49, 104, 184, 0.35); color: rgba(215, 230, 223, 0.4); }
    )");
}

} // namespace ac::launcher
