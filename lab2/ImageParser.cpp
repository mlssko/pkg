#include "ImageParser.h"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <cstring>

namespace {

quint32 le16(const uchar *p) { return p[0] | (p[1] << 8); }
quint32 le32(const uchar *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((quint32)p[3] << 24); }
quint32 be16(const uchar *p) { return (p[0] << 8) | p[1]; }
quint32 be32(const uchar *p) { return ((quint32)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

// Прочитать ровно n байт с позиции pos
bool readAt(QIODevice *d, qint64 pos, void *buf, int n)
{
    return d->seek(pos) && d->read(static_cast<char *>(buf), n) == n;
}

void bad(ImageInfo &r) { r.status = QStringLiteral("Файл поврежден"); }

// ---------------------------------------------------------------- TIFF (IFD)

struct TiffTags {
    int width = 0, height = 0;
    int bits = 0, bitsCount = 0;
    int samples = 1, compression = 1, photometric = -1, resUnit = 2;
    double xres = 0, yres = 0;
};

// base - смещение TIFF-заголовка в устройстве (для Exif внутри JPEG)
bool parseTiff(QIODevice *d, qint64 base, TiffTags &t)
{
    uchar h[8];
    if (!readAt(d, base, h, 8))
        return false;
    bool le;
    if (h[0] == 'I' && h[1] == 'I') le = true;
    else if (h[0] == 'M' && h[1] == 'M') le = false;
    else return false;

    auto u16 = [le](const uchar *p) -> quint32 { return le ? le16(p) : be16(p); };
    auto u32 = [le](const uchar *p) -> quint32 { return le ? le32(p) : be32(p); };
    if (u16(h + 2) != 42)
        return false;

    uchar cnt[2];
    if (!readAt(d, base + u32(h + 4), cnt, 2))
        return false;
    int n = u16(cnt);
    if (n == 0 || n > 512)
        return false;
    QByteArray buf(n * 12, 0);
    if (d->read(buf.data(), buf.size()) != buf.size())
        return false;

    auto rational = [&](quint32 off) -> double {
        uchar r[8];
        if (!readAt(d, base + off, r, 8))
            return 0;
        quint32 den = u32(r + 4);
        return den ? double(u32(r)) / den : 0;
    };

    for (int i = 0; i < n; ++i) {
        const uchar *e = reinterpret_cast<const uchar *>(buf.constData()) + i * 12;
        int tag = u16(e), type = u16(e + 2);
        quint32 count = u32(e + 4);
        quint32 value = (type == 3) ? u16(e + 8) : u32(e + 8);
        switch (tag) {
        case 256: t.width = value; break;
        case 257: t.height = value; break;
        case 258: {
            if (count < 1 || count > 16) break;
            uchar tmp[32];
            const uchar *src = e + 8;
            if (count * 2 > 4) {
                if (!readAt(d, base + u32(e + 8), tmp, count * 2))
                    return false;
                src = tmp;
            }
            t.bits = 0;
            for (quint32 k = 0; k < count; ++k)
                t.bits += u16(src + 2 * k);
            t.bitsCount = count;
            break;
        }
        case 259: t.compression = value; break;
        case 262: t.photometric = value; break;
        case 277: t.samples = value; break;
        case 282: t.xres = rational(u32(e + 8)); break;
        case 283: t.yres = rational(u32(e + 8)); break;
        case 296: t.resUnit = value; break;
        }
    }
    return true;
}

void applyTiffRes(const TiffTags &t, ImageInfo &r)
{
    if (t.xres <= 0 || t.yres <= 0)
        return;
    double k = (t.resUnit == 2) ? 1.0 : (t.resUnit == 3 ? 2.54 : 0.0); // 3 = точек на см
    if (k == 0)
        return;
    r.dpiX = t.xres * k;
    r.dpiY = t.yres * k;
}

QString tiffCompression(int c)
{
    switch (c) {
    case 1: return QStringLiteral("Нет");
    case 2: return QStringLiteral("CCITT RLE");
    case 3: return QStringLiteral("CCITT Group 3");
    case 4: return QStringLiteral("CCITT Group 4");
    case 5: return QStringLiteral("LZW");
    case 6: case 7: return QStringLiteral("JPEG");
    case 8: case 32946: return QStringLiteral("Deflate");
    case 32773: return QStringLiteral("PackBits");
    default: return QStringLiteral("Код %1").arg(c);
    }
}

QString tiffPhotometric(int p)
{
    switch (p) {
    case 0: return QStringLiteral("WhiteIsZero");
    case 1: return QStringLiteral("BlackIsZero");
    case 2: return QStringLiteral("RGB");
    case 3: return QStringLiteral("Палитра");
    case 4: return QStringLiteral("Маска");
    case 5: return QStringLiteral("CMYK");
    case 6: return QStringLiteral("YCbCr");
    default: return QString();
    }
}

void parseTif(QFile &f, ImageInfo &r)
{
    r.format = "TIFF";
    TiffTags t;
    if (!parseTiff(&f, 0, t) || t.width <= 0 || t.height <= 0) {
        bad(r);
        return;
    }
    r.width = t.width;
    r.height = t.height;
    r.depth = (t.bitsCount == 1) ? t.bits * t.samples : t.bits;
    r.compression = tiffCompression(t.compression);
    applyTiffRes(t, r);
    QString ph = tiffPhotometric(t.photometric);
    if (!ph.isEmpty())
        r.extra = QStringLiteral("Цветовая модель: ") + ph;
}

// ---------------------------------------------------------------- PNG

void parsePng(QFile &f, ImageInfo &r)
{
    r.format = "PNG";
    const qint64 size = f.size();
    uchar b[21];
    // сигнатура (8 байт) + чанк IHDR: длина(4) тип(4) данные(13)
    if (size < 33 || !readAt(&f, 8, b, 21) || std::memcmp(b + 4, "IHDR", 4) != 0) {
        bad(r);
        return;
    }
    r.width = be32(b + 8);
    r.height = be32(b + 12);
    int bitDepth = b[16], colorType = b[17];
    int channels = 0;
    switch (colorType) {
    case 0: channels = 1; break;
    case 2: channels = 3; break;
    case 3: channels = 1; break;
    case 4: channels = 2; break;
    case 6: channels = 4; break;
    }
    if (channels == 0 || r.width <= 0 || r.height <= 0) {
        bad(r);
        return;
    }
    r.depth = bitDepth * channels;
    r.compression = (b[18] == 0) ? QStringLiteral("Deflate") : QStringLiteral("Неизвестно");
    r.extra = QStringLiteral("Фильтрация: %1; %2")
                  .arg(b[19] == 0 ? QStringLiteral("адаптивная (метод 0)") : QStringLiteral("неизвестна"))
                  .arg(b[20] ? QStringLiteral("Adam7") : QStringLiteral("без чересстрочности"));

    // Идём по чанкам до IDAT: ищем pHYs (плотность пикселей)
    qint64 pos = 33;
    while (pos + 8 <= size) {
        uchar c[8];
        if (!readAt(&f, pos, c, 8))
            break;
        quint32 len = be32(c);
        if (!std::memcmp(c + 4, "IDAT", 4) || !std::memcmp(c + 4, "IEND", 4))
            break;
        if (!std::memcmp(c + 4, "pHYs", 4) && len == 9) {
            uchar p[9];
            if (readAt(&f, pos + 8, p, 9) && p[8] == 1) { // единица: метр
                r.dpiX = be32(p) * 0.0254;
                r.dpiY = be32(p + 4) * 0.0254;
            }
        }
        pos += 12 + qint64(len);
    }

    // В конце файла должен быть чанк IEND (12 байт)
    uchar tail[12];
    if (!readAt(&f, size - 12, tail, 12) || std::memcmp(tail + 4, "IEND", 4) != 0)
        bad(r);
}

// ---------------------------------------------------------------- JPEG

void parseJpeg(QFile &f, ImageInfo &r)
{
    r.format = "JPEG";
    const qint64 size = f.size();
    uchar tail[2];
    bool hasEoi = size >= 4 && readAt(&f, size - 2, tail, 2) && tail[0] == 0xFF && tail[1] == 0xD9;

    qint64 pos = 2; // после SOI (FF D8)
    bool foundSof = false;
    while (pos + 4 <= size) {
        uchar h[4];
        if (!readAt(&f, pos, h, 4) || h[0] != 0xFF)
            break;
        uchar m = h[1];
        if (m == 0xFF) { pos++; continue; }                          // заполнитель
        if (m == 0x00 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) { pos += 2; continue; }
        if (m == 0xD9 || m == 0xDA) break;                           // EOI / SOS
        int len = be16(h + 2);
        if (len < 2)
            break;

        // SOFn: длина(2) точность(1) высота(2) ширина(2) компонентов(1)
        if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
            uchar s[6];
            if (!readAt(&f, pos + 4, s, 6))
                break;
            r.height = be16(s + 1);
            r.width = be16(s + 3);
            r.depth = s[0] * s[5];
            switch (m) {
            case 0xC0: r.compression = QStringLiteral("Huffman, Baseline DCT"); break;
            case 0xC1: r.compression = QStringLiteral("Huffman, Extended DCT"); break;
            case 0xC2: r.compression = QStringLiteral("Huffman, Progressive DCT"); break;
            case 0xC9: case 0xCA: r.compression = QStringLiteral("Арифметическое кодирование"); break;
            default: r.compression = QStringLiteral("JPEG (SOF%1)").arg(m - 0xC0);
            }
            r.extra = QStringLiteral("Компонентов: %1").arg(s[5]);
            foundSof = true;
        }
        // APP0 JFIF: "JFIF\0" версия(2) единицы(1) Xdens(2) Ydens(2)
        else if (m == 0xE0 && len >= 16 && r.dpiX == 0) {
            uchar a[12];
            if (readAt(&f, pos + 4, a, 12) && !std::memcmp(a, "JFIF", 5)) {
                double x = be16(a + 8), y = be16(a + 10);
                if (a[7] == 1) { r.dpiX = x; r.dpiY = y; }               // точек на дюйм
                else if (a[7] == 2) { r.dpiX = x * 2.54; r.dpiY = y * 2.54; } // на см
            }
        }
        // APP1 Exif: "Exif\0\0" + TIFF-заголовок
        else if (m == 0xE1 && len >= 16 && r.dpiX == 0) {
            QByteArray data(len - 2, 0);
            if (readAt(&f, pos + 4, data.data(), data.size()) && data.startsWith(QByteArray("Exif\0\0", 6))) {
                QByteArray tiff = data.mid(6);
                QBuffer buf(&tiff);
                buf.open(QIODevice::ReadOnly);
                TiffTags t;
                if (parseTiff(&buf, 0, t))
                    applyTiffRes(t, r);
            }
        }
        pos += 2 + len;
    }

    if (!foundSof || r.width <= 0 || r.height <= 0 || !hasEoi)
        bad(r);
}

// ---------------------------------------------------------------- GIF

void parseGif(QFile &f, ImageInfo &r)
{
    r.format = "GIF";
    const qint64 size = f.size();
    uchar b[13];
    if (size < 14 || !readAt(&f, 0, b, 13)) {
        bad(r);
        return;
    }
    r.width = le16(b + 6);
    r.height = le16(b + 8);
    uchar packed = b[10];
    bool hasPalette = packed & 0x80;
    int paletteBits = (packed & 7) + 1;
    r.depth = hasPalette ? paletteBits : ((packed >> 4) & 7) + 1;
    r.compression = "LZW";
    r.extra = hasPalette ? QStringLiteral("Глобальная палитра: %1 цветов").arg(1 << paletteBits)
                         : QStringLiteral("Глобальной палитры нет");

    uchar last;
    if (r.width <= 0 || r.height <= 0 || !readAt(&f, size - 1, &last, 1) || last != 0x3B) // трейлер
        bad(r);
}

// ---------------------------------------------------------------- BMP

void parseBmp(QFile &f, ImageInfo &r)
{
    r.format = "BMP";
    const qint64 size = f.size();
    uchar b[54];
    if (size < 26 || !readAt(&f, 0, b, 26)) {
        bad(r);
        return;
    }
    quint32 fileSize = le32(b + 2), offBits = le32(b + 10), biSize = le32(b + 14);
    int bpp;
    quint32 comp = 0, clrUsed = 0;
    if (biSize == 12) {                        // BITMAPCOREHEADER (OS/2)
        r.width = le16(b + 18);
        r.height = le16(b + 20);
        bpp = le16(b + 24);
    } else {                                   // BITMAPINFOHEADER и новее
        if (biSize < 40 || size < 54 || !readAt(&f, 0, b, 54)) {
            bad(r);
            return;
        }
        r.width = int(le32(b + 18));
        r.height = qAbs(int(le32(b + 22)));
        bpp = le16(b + 28);
        comp = le32(b + 30);
        r.dpiX = le32(b + 38) * 0.0254;        // пикселей на метр -> dpi
        r.dpiY = le32(b + 42) * 0.0254;
        clrUsed = le32(b + 46);
    }
    r.depth = bpp;

    switch (comp) {
    case 0: r.compression = "BI_RGB"; break;
    case 1: r.compression = "BI_RLE8"; break;
    case 2: r.compression = "BI_RLE4"; break;
    case 3: r.compression = "BI_BITFIELDS"; break;
    case 4: r.compression = "BI_JPEG"; break;
    case 5: r.compression = "BI_PNG"; break;
    default: r.compression = QStringLiteral("Код %1").arg(comp);
    }
    if (bpp <= 8 && bpp > 0) {
        int colors = clrUsed ? int(clrUsed) : (1 << bpp);
        r.extra = QStringLiteral("Палитра: %1 цветов").arg(colors);
    } else {
        r.extra = QStringLiteral("Палитры нет");
    }

    if (r.width <= 0 || r.height <= 0 || bpp <= 0 || qint64(fileSize) > size || qint64(offBits) > size) {
        bad(r);
        return;
    }
    if (comp == 0) { // для несжатого BMP размер данных известен точно
        qint64 row = ((qint64(r.width) * bpp + 31) / 32) * 4;
        if (size < qint64(offBits) + row * r.height)
            bad(r);
    }
}

// ---------------------------------------------------------------- PCX

void parsePcx(QFile &f, ImageInfo &r)
{
    r.format = "PCX";
    const qint64 size = f.size();
    uchar b[128];
    if (size < 128 || !readAt(&f, 0, b, 128)) {
        bad(r);
        return;
    }
    int bpp = b[3], planes = b[65];
    r.width = int(le16(b + 8)) - int(le16(b + 4)) + 1;
    r.height = int(le16(b + 10)) - int(le16(b + 6)) + 1;
    r.dpiX = le16(b + 12);
    r.dpiY = le16(b + 14);
    r.depth = bpp * planes;
    r.compression = (b[2] == 1) ? QStringLiteral("RLE") : QStringLiteral("Нет");
    r.extra = QStringLiteral("Версия: %1").arg(b[1]);

    if (r.width <= 0 || r.height <= 0 || planes == 0 || bpp == 0 || b[2] > 1) {
        bad(r);
        return;
    }
    if (bpp == 8 && planes == 1 && b[1] == 5) { // 256 цветов: палитра 0x0C + 768 байт в конце
        uchar mark;
        if (size < 128 + 769 || !readAt(&f, size - 769, &mark, 1) || mark != 0x0C)
            bad(r);
        else
            r.extra += QStringLiteral("; палитра 256 цветов в конце файла");
    }
}

} // namespace

ImageInfo readImageInfo(const QString &path)
{
    ImageInfo r;
    r.path = path;
    r.name = QFileInfo(path).fileName();
    r.status = "OK";

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        r.status = QStringLiteral("Не удалось открыть файл");
        return r;
    }

    // Формат определяем по сигнатуре (расширение может быть подменено)
    uchar h[8] = {0};
    int n = f.read(reinterpret_cast<char *>(h), 8);
    bool pcxExt = path.endsWith(".pcx", Qt::CaseInsensitive);

    if (n >= 8 && !std::memcmp(h, "\x89PNG\r\n\x1a\n", 8))
        parsePng(f, r);
    else if (n >= 3 && h[0] == 0xFF && h[1] == 0xD8 && h[2] == 0xFF)
        parseJpeg(f, r);
    else if (n >= 6 && !std::memcmp(h, "GIF8", 4))
        parseGif(f, r);
    else if (n >= 2 && h[0] == 'B' && h[1] == 'M')
        parseBmp(f, r);
    else if (n >= 4 && ((!std::memcmp(h, "II*\0", 4)) || (!std::memcmp(h, "MM\0*", 4))))
        parseTif(f, r);
    else if (pcxExt && n >= 3 && h[0] == 0x0A && h[2] <= 1)
        parsePcx(f, r);
    else
        r.status = QStringLiteral("Неизвестный формат");

    f.close(); // дескриптор закрываем сразу после чтения заголовка
    return r;
}
