#include "Theme.hpp"

namespace ac::launcher {

QString getLauncherStyleSheet()
{
    return QString::fromUtf8(R"(
        QWidget#sidePanel {
            background-color: rgba(15, 18, 28, 0.82);
            border: 1px solid rgba(100, 181, 246, 0.18);
            border-radius: 14px;
        }
        QLabel#titleLabel {
            font-size: 26px; font-weight: 800; color: #00d4ff; letter-spacing: 3px; padding-bottom: 2px;
        }
        QLabel#subtitleLabel {
            font-size: 10px; color: rgba(144, 164, 174, 0.8); letter-spacing: 1px; padding-bottom: 4px;
        }
        QPushButton#playBtn {
            font-size: 17px; font-weight: 800; letter-spacing: 3px; color: white;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1565c0, stop:0.5 #1976d2, stop:1 #00acc1);
            border: none; border-radius: 10px; padding-bottom: 2px;
        }
        QPushButton#playBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1976d2, stop:0.5 #2196f3, stop:1 #00bcd4);
        }
        QPushButton#playBtn:pressed {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #0d47a1, stop:1 #00838f);
        }
        QPushButton#playBtn:disabled {
            background: rgba(55, 71, 79, 0.7); color: rgba(120, 144, 156, 0.6);
        }
        QPushButton#stopBtn {
            font-size: 17px; font-weight: 800; letter-spacing: 3px; color: white;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #c62828, stop:0.5 #d32f2f, stop:1 #e53935);
            border: none; border-radius: 10px; padding-bottom: 2px;
        }
        QPushButton#stopBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #d32f2f, stop:0.5 #e53935, stop:1 #ef5350);
        }
        QPushButton#stopBtn:pressed {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #b71c1c, stop:1 #c62828);
        }
        QPushButton#sideBtn {
            font-size: 12px; font-weight: 500; color: #cfd8dc;
            background-color: rgba(30, 40, 56, 0.65);
            border: 1px solid rgba(69, 90, 100, 0.45);
            border-radius: 8px; padding: 4px 14px; text-align: left;
        }
        QPushButton#sideBtn:hover {
            background-color: rgba(40, 55, 80, 0.85); border-color: rgba(100, 181, 246, 0.55); color: #e3f2fd;
        }
        QPushButton#sideBtn:pressed { background-color: rgba(21, 101, 192, 0.6); border-color: #42a5f5; }
        QPushButton#sideBtn:disabled {
            background-color: rgba(30, 40, 56, 0.35); color: rgba(120, 144, 156, 0.45); border-color: rgba(69, 90, 100, 0.2);
        }
        QPushButton#artBtn {
            font-size: 11px; color: rgba(144, 164, 174, 0.85); background-color: rgba(20, 28, 40, 0.5);
            border: 1px solid rgba(69, 90, 100, 0.3); border-radius: 8px; padding: 4px 12px; text-align: left;
        }
        QPushButton#artBtn:hover { color: #b0bec5; background-color: rgba(30, 42, 60, 0.7); border-color: rgba(100, 181, 246, 0.4); }
        QLabel#statusDot { font-size: 11px; color: #66bb6a; }
        QLabel#statusLabel { font-size: 13px; color: rgba(236, 239, 241, 0.9); font-weight: 500; }
        QProgressBar#mainProgress {
            background-color: rgba(20, 26, 38, 0.8); border: 1px solid rgba(69, 90, 100, 0.3); border-radius: 3px;
        }
        QProgressBar#mainProgress::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1976d2, stop:1 #00bcd4); border-radius: 2px;
        }
        QTextEdit#logView {
            background-color: rgba(10, 13, 20, 0.88);
            border: 1px solid rgba(100, 181, 246, 0.15);
            border-radius: 10px; color: #90a4ae; font-family: "JetBrains Mono", "Fira Code", monospace; font-size: 11px;
            padding: 8px 10px; selection-background-color: #1976d2; selection-color: white;
        }
        QLineEdit {
            background-color: rgba(18, 24, 36, 0.85); border: 1px solid rgba(69, 90, 100, 0.5);
            border-radius: 6px; color: #e0e0e0; font-size: 11px; padding: 3px 8px;
        }
        QLineEdit:focus { border-color: #00bcd4; }
        QComboBox {
            background-color: rgba(30, 40, 56, 0.75); border: 1px solid rgba(69, 90, 100, 0.45);
            border-radius: 6px; color: #cfd8dc; font-size: 11px; padding: 3px 8px; min-width: 80px;
        }
        QComboBox:hover { border-color: rgba(100, 181, 246, 0.55); }
        QComboBox QAbstractItemView {
            background-color: #161c28; border: 1px solid #37474f; selection-background-color: #1976d2; color: #cfd8dc;
        }
        QCheckBox { color: #b0bec5; font-size: 11px; spacing: 5px; }
        QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid rgba(69, 90, 100, 0.6); border-radius: 3px; background: rgba(20, 26, 38, 0.6); }
        QCheckBox::indicator:checked { background-color: #00acc1; border-color: #00bcd4; }
        QDialog { background-color: #121720; }
        QTabWidget::pane { border: 1px solid rgba(69, 90, 100, 0.35); border-radius: 8px; background-color: rgba(18, 24, 36, 0.9); }
        QTabBar::tab {
            background-color: rgba(25, 33, 46, 0.7); color: #78909c; border: 1px solid rgba(69, 90, 100, 0.3);
            border-bottom: none; border-top-left-radius: 6px; border-top-right-radius: 6px; padding: 6px 14px; margin-right: 2px; font-size: 12px;
        }
        QTabBar::tab:selected { background-color: rgba(18, 24, 36, 0.9); color: #00bcd4; border-color: rgba(100, 181, 246, 0.4); font-weight: 600; }
        QTabBar::tab:hover:!selected { background-color: rgba(40, 55, 75, 0.6); color: #cfd8dc; }
        QTableWidget {
            background-color: rgba(15, 20, 30, 0.8); color: #cfd8dc; border: 1px solid rgba(69, 90, 100, 0.35);
            border-radius: 6px; gridline-color: rgba(69, 90, 100, 0.2); font-size: 12px;
        }
        QTableWidget::item { padding: 4px 8px; }
        QTableWidget::item:selected { background-color: rgba(25, 118, 210, 0.35); }
        QHeaderView::section {
            background-color: rgba(30, 40, 56, 0.8); color: #90a4ae; border: none; border-bottom: 1px solid rgba(69, 90, 100, 0.3); padding: 6px 8px; font-size: 11px; font-weight: 600;
        }
        QPushButton {
            background-color: rgba(30, 40, 56, 0.75); color: #cfd8dc; border: 1px solid rgba(69, 90, 100, 0.45);
            border-radius: 6px; padding: 4px 10px; font-size: 11px; font-weight: 500;
        }
        QPushButton:hover { background-color: rgba(40, 55, 80, 0.85); border-color: rgba(100, 181, 246, 0.55); color: #e3f2fd; }
        QPushButton:pressed { background-color: rgba(21, 101, 192, 0.6); }
    )");
}

} // namespace ac::launcher
