#include "widgets.h"

#include <QPainter>
#include <QPolygonF>
#include <algorithm>

// ---------------------------------------------------------------- ImageView

ImageView::ImageView(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(250, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ImageView::setImage(const QImage& image)
{
    m_image = image;
    update();
}

void ImageView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(60, 60, 60));

    if (m_image.isNull()) {
        p.setPen(Qt::white);
        p.drawText(rect(), Qt::AlignCenter, "нет изображения");
        return;
    }

    // вписываем картинку в виджет с сохранением пропорций
    double scale = std::min(double(width()) / m_image.width(),
                            double(height()) / m_image.height());
    int w = static_cast<int>(m_image.width() * scale);
    int h = static_cast<int>(m_image.height() * scale);
    QRect target((width() - w) / 2, (height() - h) / 2, w, h);

    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.drawImage(target, m_image);
}

// ---------------------------------------------------------- HistogramWidget

HistogramWidget::HistogramWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(140);
    setMaximumHeight(160);
}

void HistogramWidget::setHistogram(const Histogram& hist)
{
    m_hist = hist;
    m_valid = true;
    update();
}

void HistogramWidget::clear()
{
    m_valid = false;
    update();
}

void HistogramWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);

    QRect area = rect().adjusted(5, 5, -5, -18);
    p.setPen(Qt::gray);
    p.drawRect(area);

    if (!m_valid) {
        p.drawText(area, Qt::AlignCenter, "нет данных");
        return;
    }

    int peak = 1;
    for (int i = 0; i < 256; ++i) {
        peak = std::max(peak, m_hist.lum[i]);
        peak = std::max(peak, std::max(m_hist.r[i], std::max(m_hist.g[i], m_hist.b[i])));
    }

    double bw = double(area.width()) / 256.0;
    double ah = area.height();

    // яркость - серые столбики
    for (int i = 0; i < 256; ++i) {
        double hgt = m_hist.lum[i] * ah / peak;
        p.fillRect(QRectF(area.left() + i * bw, area.bottom() - hgt, bw + 0.5, hgt),
                   QColor(190, 190, 190));
    }

    // R, G, B - линиями
    auto drawCurve = [&](const std::array<int, 256>& data, QColor color) {
        QPolygonF poly;
        for (int i = 0; i < 256; ++i)
            poly << QPointF(area.left() + (i + 0.5) * bw, area.bottom() - data[i] * ah / peak);
        p.setPen(QPen(color, 1));
        p.drawPolyline(poly);
    };
    drawCurve(m_hist.r, QColor(220, 0, 0));
    drawCurve(m_hist.g, QColor(0, 160, 0));
    drawCurve(m_hist.b, QColor(0, 0, 220));

    p.setPen(Qt::black);
    p.drawText(area.left(), height() - 4, "0");
    p.drawText(area.right() - 22, height() - 4, "255");
    p.drawText(area.left() + 35, height() - 4, "серый - яркость, R, G, B - линии");
}
