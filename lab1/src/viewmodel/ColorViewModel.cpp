#include "ColorViewModel.h"

#include <algorithm>
#include <cmath>

ColorViewModel::ColorViewModel(QObject* parent)
    : QObject(parent)
{
    recomputeFromRgb();
}

QColor ColorViewModel::toQColor() const
{
    auto to255 = [](double v) {
        return static_cast<int>(std::round(ColorMath::clampValue(v, 0.0, 1.0) * 255.0));
    };
    return QColor(to255(m_rgb.r), to255(m_rgb.g), to255(m_rgb.b));
}

void ColorViewModel::recomputeFromRgb()
{
    m_cmyk = ColorMath::rgbToCmyk(m_rgb, m_cmykAlgorithm);
    m_hls  = ColorMath::rgbToHls(m_rgb);
}

void ColorViewModel::finish(bool outOfGamut)
{
    recomputeFromRgb();
    emit colorChanged();

    if (outOfGamut) {
        const QString strategy = (m_gamutStrategy == ColorMath::GamutStrategy::Clipping)
        ? tr("обрезание (Clipping)")
        : tr("масштабирование диапазона (Scaling)");
        emit warningRaised(tr("Цвет вышел за границы модели - применено %1.").arg(strategy));
    } else {
        emit warningRaised(QString());
    }
}

void ColorViewModel::setRgb255(int r, int g, int b)
{
    r = std::clamp(r, 0, 255);
    g = std::clamp(g, 0, 255);
    b = std::clamp(b, 0, 255);
    m_rgb = ColorMath::RGB{ r / 255.0, g / 255.0, b / 255.0 };
    finish(false);
}

void ColorViewModel::setCmykPercent(double c, double m, double y, double k)
{
    ColorMath::CMYK cmyk{
        ColorMath::clampValue(c / 100.0, 0.0, 1.0),
        ColorMath::clampValue(m / 100.0, 0.0, 1.0),
        ColorMath::clampValue(y / 100.0, 0.0, 1.0),
        ColorMath::clampValue(k / 100.0, 0.0, 1.0)
    };
    bool outOfGamut = false;
    m_rgb = ColorMath::cmykToRgb(cmyk, m_gamutStrategy, outOfGamut);
    finish(outOfGamut);
}

void ColorViewModel::setHls(double hDegrees, double lPercent, double sPercent)
{
    ColorMath::HLS hls{ hDegrees, lPercent / 100.0, sPercent / 100.0 };
    bool outOfGamut = false;
    m_rgb = ColorMath::hlsToRgb(hls, m_gamutStrategy, outOfGamut);
    finish(outOfGamut);
}

void ColorViewModel::setFromQColor(const QColor& color)
{
    setRgb255(color.red(), color.green(), color.blue());
}

void ColorViewModel::setCmykAlgorithm(ColorMath::CmykAlgorithm algorithm)
{
    m_cmykAlgorithm = algorithm;
    finish(false);
}

void ColorViewModel::setGamutStrategy(ColorMath::GamutStrategy strategy)
{
    m_gamutStrategy = strategy;
    finish(false);
}