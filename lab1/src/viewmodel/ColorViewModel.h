#pragma once

#include <QObject>
#include <QColor>
#include <QString>

#include "../model/ColorMath.h"

// ColorViewModel - "ViewModel/Controller" слой
//
//хранит единственный источник истины-текущий цвет в нормированном RGB
//

class ColorViewModel : public QObject
{
    Q_OBJECT

public:
    explicit ColorViewModel(QObject* parent = nullptr);

    ColorMath::RGB rgb() const { return m_rgb; }
    ColorMath::CMYK cmyk() const { return m_cmyk; }
    ColorMath::HLS hls() const { return m_hls; }

    ColorMath::CmykAlgorithm cmykAlgorithm() const { return m_cmykAlgorithm; }
    ColorMath::GamutStrategy gamutStrategy() const { return m_gamutStrategy; }

    QColor toQColor() const;

public slots:
    //способ 1:точный ввод чисел - способ 2:слайдеры -
    //способ 3:палитра (setFromQColor).
    void setRgb255(int r, int g, int b);
    void setCmykPercent(double c, double m, double y, double k);
    void setHls(double hDegrees, double lPercent, double sPercent);
    void setFromQColor(const QColor& color);

    void setCmykAlgorithm(ColorMath::CmykAlgorithm algorithm);
    void setGamutStrategy(ColorMath::GamutStrategy strategy);

signals:
    //испускается после каждого пересчёта - View должен перечитать все
    //геттеры и обновить виджеты
    void colorChanged();

    //пустая строка = предупреждения нет (скрыть плашку в UI)
    void warningRaised(const QString& message);

private:
    void recomputeFromRgb();
    void finish(bool outOfGamut);

    ColorMath::RGB  m_rgb{1.0, 0.0, 0.0};
    ColorMath::CMYK m_cmyk;
    ColorMath::HLS  m_hls;

    ColorMath::CmykAlgorithm m_cmykAlgorithm = ColorMath::CmykAlgorithm::GCR;
    ColorMath::GamutStrategy m_gamutStrategy = ColorMath::GamutStrategy::Clipping;
};