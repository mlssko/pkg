#include "imageconvert.h"

ImageBuffer ImageConvert::fromQImage(const QImage& image)
{
    // Приводим к единому формату, где пиксель = 32 бита 0xFFRRGGBB
    QImage img = image.convertToFormat(QImage::Format_RGB32);

    ImageBuffer buffer(img.width(), img.height());
    for (int y = 0; y < img.height(); ++y) {
        const quint32* src = reinterpret_cast<const quint32*>(img.constScanLine(y));
        unsigned char* dst = buffer.row(y);
        for (int x = 0; x < img.width(); ++x) {
            quint32 p = src[x];
            dst[3 * x]     = (p >> 16) & 0xFF; // R
            dst[3 * x + 1] = (p >> 8) & 0xFF;  // G
            dst[3 * x + 2] = p & 0xFF;         // B
        }
    }
    return buffer;
}

QImage ImageConvert::toQImage(const ImageBuffer& buffer)
{
    QImage img(buffer.width(), buffer.height(), QImage::Format_RGB32);
    for (int y = 0; y < buffer.height(); ++y) {
        const unsigned char* src = buffer.row(y);
        quint32* dst = reinterpret_cast<quint32*>(img.scanLine(y));
        for (int x = 0; x < buffer.width(); ++x) {
            quint32 r = src[3 * x];
            quint32 g = src[3 * x + 1];
            quint32 b = src[3 * x + 2];
            dst[x] = 0xFF000000u | (r << 16) | (g << 8) | b;
        }
    }
    return img;
}
