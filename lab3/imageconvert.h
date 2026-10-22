#pragma once
// Преобразование QImage <-> ImageBuffer.
// Доступ к пикселям идёт через указатели на строки (scanLine), без pixel()/setPixel().

#include <QImage>
#include "pixelbuffer.h"

namespace ImageConvert {

ImageBuffer fromQImage(const QImage& image);
QImage toQImage(const ImageBuffer& buffer);

}
