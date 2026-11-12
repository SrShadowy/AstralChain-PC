#include "LauncherIcons.hpp"

#include <QByteArray>
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace ac::launcher {

namespace {

constexpr int kHidpiScale = 2;

QPixmap renderTintedSvg(const QByteArray& rawSvg, const QColor& color, int sizePx)
{
    QByteArray tinted = rawSvg;
    tinted.replace("currentColor", color.name(QColor::HexRgb).toUtf8());

    QSvgRenderer renderer(tinted);
    const int pixelSize = sizePx * kHidpiScale;
    QPixmap pixmap(pixelSize, pixelSize);
    pixmap.fill(Qt::transparent);

    if (renderer.isValid()) {
        QPainter painter(&pixmap);
        renderer.render(&painter);
    }

    pixmap.setDevicePixelRatio(static_cast<qreal>(kHidpiScale));
    return pixmap;
}

} // namespace

QIcon makeLauncherIcon(const QString& iconName,
                       const QColor& normalColor,
                       const QColor& activeColor,
                       int sizePx)
{
    QFile file(QStringLiteral(":/icons/%1.svg").arg(iconName));
    if (!file.open(QIODevice::ReadOnly)) {
        return QIcon();
    }
    const QByteArray rawSvg = file.readAll();

    QIcon icon;
    const QPixmap normalPixmap = renderTintedSvg(rawSvg, normalColor, sizePx);
    icon.addPixmap(normalPixmap, QIcon::Normal, QIcon::Off);

    if (activeColor.isValid()) {
        const QPixmap activePixmap = renderTintedSvg(rawSvg, activeColor, sizePx);
        icon.addPixmap(activePixmap, QIcon::Active, QIcon::Off);
        icon.addPixmap(activePixmap, QIcon::Selected, QIcon::Off);
    }

    return icon;
}

} // namespace ac::launcher
