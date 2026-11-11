#include "Theme.hpp"

namespace ac::launcher {

QString getLauncherStyleSheet()
{
    return QString::fromUtf8(R"(
        QMainWindow, QWidget#centralWidget, QDialog {
            background-color: #0a101b;
            color: #e3f2fa;
            font-family: "Fira Sans", "Noto Sans", "Segoe UI", sans-serif;
        }
        QLabel {
            color: #d8eaf4;
            font-size: 12px;
        }
        QWidget#sidePanel {
            background-color: rgba(13, 22, 37, 0.92);
            border: 1px solid rgba(0, 212, 255, 0.28);
            border-top: 2px solid #00d4ff;
            border-radius: 4px;
        }
        QWidget#statusCard {
            background-color: rgba(13, 22, 37, 0.88);
            border: 1px solid rgba(0, 212, 255, 0.24);
            border-left: 3px solid #00d4ff;
            border-radius: 3px;
        }
        QLabel#kickerLabel {
            font-size: 9px; font-weight: 700; color: #00d4ff; letter-spacing: 2px;
        }
        QLabel#titleLabel {
            font-size: 25px; font-weight: 800; color: #e8f6ff; letter-spacing: 3px; padding-bottom: 1px;
        }
        QLabel#subtitleLabel {
            font-size: 10px; color: #7d99b5; letter-spacing: 1px; padding-bottom: 6px;
        }
        QLabel#sideFooterLabel {
            font-size: 10px; color: #6482a0; border-top: 1px solid rgba(0, 212, 255, 0.16); padding-top: 8px;
        }
        QLabel#statusDot { font-size: 12px; color: #26eb8c; }
        QLabel#statusLabel { font-size: 12px; color: #e3f2fa; font-weight: 600; }
        QProgressBar#mainProgress {
            background-color: rgba(9, 15, 26, 0.90);
            border: 1px solid rgba(0, 212, 255, 0.30);
            border-radius: 2px;
        }
        QProgressBar#mainProgress::chunk {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1e4d92, stop:1 #00d4ff);
            border-radius: 1px;
        }
        QTextEdit#logView {
            background-color: rgba(8, 13, 23, 0.94);
            border: 1px solid rgba(0, 212, 255, 0.22);
            border-top: 2px solid rgba(0, 212, 255, 0.50);
            border-radius: 3px;
            color: #d8eaf4;
            font-family: "JetBrains Mono", "Fira Code", "DejaVu Sans Mono", monospace;
            font-size: 11px;
            padding: 8px 10px;
            selection-background-color: rgba(0, 212, 255, 0.30);
            selection-color: #ffffff;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: rgba(16, 28, 48, 0.92);
            border: 1px solid rgba(0, 212, 255, 0.30);
            border-radius: 2px;
            color: #e3f2fa;
            font-size: 12px;
            padding: 5px 10px;
            min-height: 18px;
        }
        QComboBox { min-width: 140px; }
        QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover {
            border-color: rgba(0, 212, 255, 0.65);
            background-color: rgba(22, 38, 64, 0.95);
        }
        QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
            border-color: #00d4ff;
        }
        QComboBox QAbstractItemView {
            background-color: #0d1626;
            border: 1px solid #00d4ff;
            selection-background-color: #234b85;
            selection-color: #ffffff;
            color: #e3f2fa;
            padding: 4px;
            outline: 0;
        }
        QGroupBox {
            background-color: rgba(13, 22, 37, 0.65);
            border: 1px solid rgba(0, 212, 255, 0.24);
            border-radius: 3px;
            margin-top: 12px;
            padding-top: 10px;
            font-weight: 700;
            color: #00d4ff;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            left: 10px;
            padding: 0 6px;
            color: #59e8ff;
        }
        QCheckBox {
            color: #e3f2fa;
            font-size: 12px;
            font-weight: 500;
            spacing: 8px;
        }
        QCheckBox:disabled { color: #526880; }
        QCheckBox::indicator {
            width: 15px; height: 15px;
            border: 1px solid rgba(0, 212, 255, 0.45);
            border-radius: 2px;
            background: rgba(10, 17, 30, 0.85);
        }
        QCheckBox::indicator:hover { border-color: #00d4ff; }
        QCheckBox::indicator:checked {
            background-color: #00d4ff;
            border-color: #59e8ff;
        }
        QSlider::groove:horizontal {
            height: 4px;
            background: rgba(16, 28, 48, 0.95);
            border: 1px solid rgba(0, 212, 255, 0.25);
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #23508e, stop:1 #00d4ff);
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #00d4ff;
            border: 1px solid #e3f2fa;
            width: 12px;
            margin: -5px 0;
            border-radius: 2px;
        }
        QSlider::handle:horizontal:hover { background: #59e8ff; }
        QTabWidget::pane {
            border: 1px solid rgba(0, 212, 255, 0.28);
            border-radius: 3px;
            background-color: rgba(13, 21, 36, 0.95);
        }
        QTabBar::tab {
            background-color: rgba(14, 24, 42, 0.85);
            color: #7d99b5;
            border: 1px solid rgba(0, 212, 255, 0.18);
            border-bottom: none;
            border-top-left-radius: 3px;
            border-top-right-radius: 3px;
            padding: 7px 16px;
            margin-right: 3px;
            font-size: 12px;
            font-weight: 600;
        }
        QTabBar::tab:selected {
            background-color: rgba(24, 44, 76, 0.98);
            color: #59e8ff;
            border-color: rgba(0, 212, 255, 0.45);
            border-top: 2px solid #00d4ff;
        }
        QTabBar::tab:hover:!selected {
            background-color: rgba(20, 36, 62, 0.90);
            color: #e3f2fa;
        }
        QTableWidget {
            background-color: rgba(9, 15, 26, 0.92);
            color: #e3f2fa;
            border: 1px solid rgba(0, 212, 255, 0.28);
            border-radius: 3px;
            gridline-color: rgba(0, 212, 255, 0.14);
            font-size: 12px;
            selection-background-color: rgba(35, 80, 142, 0.75);
            selection-color: #ffffff;
        }
        QTableWidget::item { padding: 5px 8px; }
        QHeaderView::section {
            background-color: rgba(18, 34, 58, 0.95);
            color: #59e8ff;
            border: none;
            border-bottom: 1px solid rgba(0, 212, 255, 0.35);
            padding: 6px 8px;
            font-size: 11px;
            font-weight: 700;
        }
        QScrollBar:vertical {
            background: rgba(9, 15, 26, 0.60);
            width: 10px;
            margin: 0;
        }
        QScrollBar::handle:vertical {
            background: rgba(45, 92, 158, 0.75);
            min-height: 24px;
            border-radius: 2px;
        }
        QScrollBar::handle:vertical:hover { background: #00d4ff; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QPushButton {
            background-color: rgba(25, 52, 92, 0.92);
            color: #e3f2fa;
            border: 1px solid rgba(0, 212, 255, 0.32);
            border-radius: 2px;
            padding: 6px 15px;
            font-size: 11px;
            font-weight: 700;
        }
        QPushButton:hover {
            background-color: rgba(38, 78, 136, 0.98);
            border-color: #00d4ff;
            color: #ffffff;
        }
        QPushButton:pressed {
            background-color: #00d4ff;
            color: #09101b;
        }
        QPushButton:disabled {
            background-color: rgba(20, 34, 56, 0.40);
            border-color: rgba(125, 153, 181, 0.15);
            color: rgba(125, 153, 181, 0.40);
        }
        QPushButton#primaryBtn {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1d4d91, stop:1 #009ec2);
            border: 1px solid #00d4ff;
            color: #ffffff;
        }
        QPushButton#primaryBtn:hover {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2662b5, stop:1 #00bfe8);
            border-color: #59e8ff;
        }
    )");
}

} // namespace ac::launcher
