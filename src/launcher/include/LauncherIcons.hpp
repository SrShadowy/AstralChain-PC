#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

namespace ac::launcher {

constexpr int kDefaultUiIconPx = 16;

QIcon makeLauncherIcon(const QString& iconName,
                       const QColor& normalColor = QColor("#d8eaf4"),
                       const QColor& activeColor = QColor(),
                       int sizePx = kDefaultUiIconPx);

} // namespace ac::launcher
