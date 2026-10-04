#include <QtTest/QtTest>

#include "../src/model/ColorMath.h"

using namespace ColorMath;

namespace {
const double EPS = 1e-4;
}

class TestColorMath : public QObject
{
    Q_OBJECT

private slots:

    // RGB <-> HLS

    void redToHls()
    {
        // Чистый красный: H=0, L=0.5, S=1
        RGB red{1.0, 0.0, 0.0};
        HLS hls = rgbToHls(red);
        QVERIFY(qAbs(hls.h - 0.0) < EPS);
        QVERIFY(qAbs(hls.l - 0.5) < EPS);
        QVERIFY(qAbs(hls.s - 1.0) < EPS);
    }

    void whiteToHls()
    {
        RGB white{1.0, 1.0, 1.0};
        HLS hls = rgbToHls(white);
        QVERIFY(qAbs(hls.l - 1.0) < EPS);
        QVERIFY(qAbs(hls.s - 0.0) < EPS);
    }

    void hlsRoundTrip()
    {
        RGB original{0.2, 0.7, 0.4};
        HLS hls = rgbToHls(original);
        bool outOfGamut = false;
        RGB back = hlsToRgb(hls, GamutStrategy::Clipping, outOfGamut);
        QVERIFY(!outOfGamut);
        QVERIFY(qAbs(back.r - original.r) < EPS);
        QVERIFY(qAbs(back.g - original.g) < EPS);
        QVERIFY(qAbs(back.b - original.b) < EPS);
    }

    //  RGB <-> CMYK

    void redToCmykGcr()
    {
        // RGB(255,0,0) -> CMYK строго (0,1,1,0), т.к. min(C0,M0,Y0)=0
        RGB red{1.0, 0.0, 0.0};
        CMYK cmyk = rgbToCmyk(red, CmykAlgorithm::GCR);
        QVERIFY(qAbs(cmyk.c - 0.0) < EPS);
        QVERIFY(qAbs(cmyk.m - 1.0) < EPS);
        QVERIFY(qAbs(cmyk.y - 1.0) < EPS);
        QVERIFY(qAbs(cmyk.k - 0.0) < EPS);
    }

    void blackToCmyk()
    {
        RGB black{0.0, 0.0, 0.0};
        CMYK cmykGcr = rgbToCmyk(black, CmykAlgorithm::GCR);
        CMYK cmykUcr = rgbToCmyk(black, CmykAlgorithm::UCR);
        QVERIFY(qAbs(cmykGcr.k - 1.0) < EPS);
        QVERIFY(qAbs(cmykUcr.k - 1.0) < EPS);
        QVERIFY(qAbs(cmykGcr.c) < EPS && qAbs(cmykGcr.m) < EPS && qAbs(cmykGcr.y) < EPS);
    }

    void whiteToCmyk()
    {
        RGB white{1.0, 1.0, 1.0};
        CMYK cmyk = rgbToCmyk(white, CmykAlgorithm::GCR);
        QVERIFY(qAbs(cmyk.c) < EPS && qAbs(cmyk.m) < EPS && qAbs(cmyk.y) < EPS && qAbs(cmyk.k) < EPS);
    }

    void gcrProducesMoreOrEqualBlackThanUcr()
    {
        // Для промежуточного серовато-коричневого цвета GCR должен давать
        // не меньше чёрного, чем UCR (т.к. K_ucr = rawK^2 <= rawK = K_gcr).
        RGB c{0.5, 0.35, 0.2};
        CMYK gcr = rgbToCmyk(c, CmykAlgorithm::GCR);
        CMYK ucr = rgbToCmyk(c, CmykAlgorithm::UCR);
        QVERIFY(gcr.k >= ucr.k - EPS);
    }

    void cmykRoundTrip()
    {
        RGB original{0.6, 0.3, 0.8};
        CMYK cmyk = rgbToCmyk(original, CmykAlgorithm::GCR);
        bool outOfGamut = false;
        RGB back = cmykToRgb(cmyk, GamutStrategy::Clipping, outOfGamut);
        QVERIFY(!outOfGamut);
        QVERIFY(qAbs(back.r - original.r) < EPS);
        QVERIFY(qAbs(back.g - original.g) < EPS);
        QVERIFY(qAbs(back.b - original.b) < EPS);
    }

    //Стратегии выхода за пределы гаммы

    void clippingStaysInRange()
    {
        // Некорректные L/S (вне [0..1]) дают цвет вне гаммы - проверяем,
        // что Clipping возвращает все компоненты в [0..1] и выставляет флаг.
        HLS bad{120.0, 1.5, 1.5};
        bool outOfGamut = false;
        RGB clipped = hlsToRgb(bad, GamutStrategy::Clipping, outOfGamut);
        QVERIFY(outOfGamut);
        QVERIFY(clipped.r >= -EPS && clipped.r <= 1.0 + EPS);
        QVERIFY(clipped.g >= -EPS && clipped.g <= 1.0 + EPS);
        QVERIFY(clipped.b >= -EPS && clipped.b <= 1.0 + EPS);
    }

    void scalingStaysInRange()
    {
        HLS bad{120.0, 1.5, 1.5};
        bool outOfGamut = false;
        RGB scaled = hlsToRgb(bad, GamutStrategy::Scaling, outOfGamut);
        QVERIFY(outOfGamut);
        QVERIFY(scaled.r >= -EPS && scaled.r <= 1.0 + EPS);
        QVERIFY(scaled.g >= -EPS && scaled.g <= 1.0 + EPS);
        QVERIFY(scaled.b >= -EPS && scaled.b <= 1.0 + EPS);
    }

    void normalColorNeverFlaggedOutOfGamut()
    {
        RGB original{0.5, 0.5, 0.5};
        HLS hls = rgbToHls(original);
        bool outOfGamut = false;
        hlsToRgb(hls, GamutStrategy::Clipping, outOfGamut);
        QVERIFY(!outOfGamut);
    }
};

QTEST_MAIN(TestColorMath)
#include "test_colormath.moc"