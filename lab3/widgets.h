#pragma once
// Простые виджеты для отображения: картинка и гистограмма.

#include <QImage>
#include <QWidget>
#include "algorithms.h"

class ImageView : public QWidget
{
    Q_OBJECT
public:
    explicit ImageView(QWidget* parent = nullptr);
    void setImage(const QImage& image);

protected:
    void paintEvent(QPaintEvent*) override;

private:
    QImage m_image;
};

class HistogramWidget : public QWidget
{
    Q_OBJECT
public:
    explicit HistogramWidget(QWidget* parent = nullptr);
    void setHistogram(const Histogram& hist);
    void clear();

protected:
    void paintEvent(QPaintEvent*) override;

private:
    Histogram m_hist;
    bool m_valid = false;
};
